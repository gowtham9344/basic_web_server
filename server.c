#include <string.h>  
#include <unistd.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <netinet/in.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <sys/wait.h>
#include <arpa/inet.h>
#include <signal.h>
#include <netdb.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h> 

#define SA struct sockaddr 
#define BACKLOG 10 
#define PORT "8050"

int flag = 0;

void send_response(int client_socket,const char* status, const char* content_type,const char* content);


//send css is used to only the css file.
void send_css(int client_socket,char fileName[100]){
	 FILE *file = fopen(fileName, "r");
	 // open css file
	 if (file == NULL) {
		perror("Error opening file");
		send_response(client_socket, "404 Not Found", "text/plain", "File Not Found");
		flag = 1;
		return;
	 }
	
	 //read data from the css file
	 char buffer[1024];
	 int bytesRead;
	 fread(buffer, 1, sizeof(buffer), file);
		
	 //send the data to the client
	 send_response(client_socket, "200 OK", "text/css", buffer);
    	 fclose(file);
}

// defining HTTP header and send data to the client
void send_response(int client_socket,const char* status, const char* content_type,const char* content){
	char response[2048]; 
	sprintf(response, "HTTP/1.1 %s\r\nContent-Type: %s\r\n\r\n%s", status, content_type, content);
	send(client_socket, response, strlen(response), 0);
}

// handle saving of data into the file 
void store_data(int client_socket,char content[1024],char fileName[100]){
	    // Open a file for writing
	    FILE *file = fopen(fileName, "a"); // Open in append mode to append new data
	    if (file == NULL) {
		perror("Error opening file");
		send_response(client_socket, "500 Internal Server Error", "text/plain", "Error opening file");
		flag = 1;
		return;
	    }

	    // Write the decoded data to the file
	    fprintf(file, "%s\n", content);

	    // Close the file
	    fclose(file);
}

// retrieve all data from the file in the list form
void getalldata(int client_socket,char content[1024],char fileName[100]){
	    // Open the file for reading
	    FILE *file = fopen(fileName, "r");
	    if (file == NULL) {
		perror("Error opening file");
		send_response(client_socket, "500 Internal Server Error", "text/plain", "Error opening file");
		flag = 1;
		return;
	    }

	    //get data from file
	    char line[100];
	    int len = 0;
	    while(fgets(line,100,file)){
		char line2[100];
	    	sprintf(line2,"<li>%s</li>",line);
		strcat(content,line2);
	    }
	    content[strlen(content) - 1] = '\0';
	    fclose(file);
}

//handling get request without query parameters
void handle_get_request(int client_socket,char fileName[100]) {
    char buff[1024];
	
    // get all data from the specified file
    getalldata(client_socket,buff,fileName); 
    if(flag == 1)
	return;
    
    //create html content for displaying data
    char html_content[1024];
    sprintf(html_content,"<html><head><link rel=\"stylesheet\" href=\"styles.css\"></head><body><ul>%s</ul></body></html>",buff);
  
    send_response(client_socket, "200 OK", "text/html", html_content);
}


// handle post request and get request with query parameters
void handle_post_request(int client_socket, char content[],char fileName[100]) {
    char* p;

    // replace '=' with ':'
    while((p = strstr(content,"=")) != NULL){
    	*p = ':';
    }
    char response_content[1024];
    store_data(client_socket,content,fileName);
    if(flag == 1)
	return;
    // HTML  content
    sprintf(response_content, "<html><head><link rel=\"stylesheet\" href=\"styles.css\"></head><body><h1>Data is posted</h1><p>%s</p></body></html>", content);
    send_response(client_socket, "200 OK", "text/html", response_content);
}


// Function to decode URL-encoded string to normal string
void url_decode(char *str) {
    int i, j = 0;
    char c;

    for (i = 0; str[i] != '\0'; ++i) {
        if (str[i] == '+') {
            str[j++] = ' ';
        } else if (str[i] == '%' && isxdigit(str[i + 1]) && isxdigit(str[i + 2])) {
            sscanf(&str[i + 1], "%2x", (unsigned int*)&c);
            str[j++] = c;
            i += 2;
        } else {
            str[j++] = str[i];
        }
    }

    str[j] = '\0';
}

//parsing of query parameters and store in content array
void parse_query_parameters(const char *query_string,char content[1024]) {
    char parameter[50];
    char value[50];
    content[0] = '\0';
    int len = 0;


    // Parse query parameters
    while (sscanf(query_string, "%49[^=]=%49[^&]", parameter, value) == 2) {
	char line2[100];
	memset(line2,'\0',sizeof(line2));
	sprintf(line2,"%s=%s\n",parameter,value);
        len += strlen(line2);
	strcat(content,line2);
	
        // Move to the next parameter
        query_string = strchr(query_string, '&');
        if (query_string == NULL) {
            break;
        }
        query_string++; // Skip the '&'
    }
    content[len-1] = '\0';
}


//it helps us to handle all the dead process which was created with the fork system call.
void sigchld_handler(int s){
	int saved_errno = errno;
	while(waitpid(-1,NULL,WNOHANG) > 0);
	errno = saved_errno;
}

// give IPV4 or IPV6  based on the family set in the sa
void *get_in_addr(struct sockaddr *sa){
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*)sa)->sin_addr);	
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}

// this is the code for server creation. here i have used TCP instead of UDP because i need all the data without any loss. if we use UDP we
// have to implement those in the upper layers.
// this function will return socket descripter to the calling function.
int server_creation(){
	int sockfd;
	struct addrinfo hints,*servinfo,*p;
	int yes = 1;
	int rv;
	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;// my ip
	
	// set the address of the server with the port info.
	if((rv = getaddrinfo(NULL,PORT,&hints,&servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n",gai_strerror(rv));	
		return 1;
	}
	
	// loop through all the results and bind to the socket in the first we can
	for(p = servinfo; p!= NULL; p=p->ai_next){
		sockfd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
		if(sockfd==-1){ 
			perror("server: socket\n"); 
			continue; 
		} 
		
		// SO_REUSEADDR is used to reuse the same port even if it was already created by this.
		// this is needed when the program is closed due to some system errors then socket will be closed automaticlly after few
		// minutes in that case before the socket is closed if we rerun the program then we have use the already used port 	
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
			perror("setsockopt");
			exit(1);	
		}
		
		// it will help us to bind to the port.
		if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			close(sockfd);
			perror("server: bind");
			continue;
		}
		break;
	}

	if(p == NULL){
		fprintf(stderr, "server: failed to bind\n");
		exit(1);	
	}
	

	// server will be listening with maximum simultaneos connections of BACKLOG
	if(listen(sockfd,BACKLOG) == -1){ 
		perror("listen");
		exit(1); 
	} 
	return sockfd;
}

//connection establishment with the client
//return connection descriptor to the calling function
int connection_accepting(int sockfd){
	int connfd;
	struct sockaddr_storage their_addr;
	char s[INET6_ADDRSTRLEN];
	socklen_t sin_size;
	
	sin_size = sizeof(their_addr); 
	connfd=accept(sockfd,(SA*)&their_addr,&sin_size); 
	if(connfd == -1){ 
		perror("\naccept error\n");
		return -1;
	} 
	//printing the client name
	inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof(s));
	printf("\nserver: got connection from %s\n", s);
	
	return connfd;
}

// reap all dead processes that are created as child processes
void signal_handler(){
	struct sigaction sa;
	sa.sa_handler = sigchld_handler; 
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
}



int main(){ 
	int sockfd,connfd; 
	
	//server creation .
	sockfd = server_creation();
	
	signal_handler();	

	printf("server: waiting for connections...\n");
	 
	while(1){ 

		connfd = connection_accepting(sockfd);
			
		if(connfd == -1){
			continue;
		}

		// fork is used for concurrent server.
		// here fork is used to create child process to handle single client connection because if two clients needs to 
		// connect to the server simultaneously if we do the client acceptence without fork if some client got connected then until 
		// the client releases the server no one can able to connect to the server.
		// to avoid this , used fork, that creates child process to handle the connection.
  
		int fk=fork(); 
		if (fk==0){ 
			close(sockfd);
			
			int c = 0;
			char buff[1024];
			// receiving the message from the client either get request or post request
			if((c = recv(connfd,buff,sizeof(buff),0)) == -1){
				printf("msg not received\n"); 
				exit(0); 
			}
			
			buff[c] = '\0';
 
 			char method[10];
			char fileName[100] = "output.txt";
			char route[100];
			char queryData[1024];
			int query = 0;
			printf("%s",buff);
        		sscanf(buff, "%s /%s", method,route);
				

			if(strcmp(route,"HTTP/1.1")){
				char* queryPointer = strstr(route,"?");
				if(queryPointer == NULL){
					strcpy(fileName,route);
				}
				else{
					sscanf(route, "%[^?]s", fileName);
					query = 1;
					if (queryPointer != NULL) {
						// Move to the actual query string
						queryPointer++;

						// Parse query parameters
						parse_query_parameters(queryPointer,queryData);
					 }
				
				}
			}
			else if(strcmp(route,"?")==0){
				char* queryPointer = strstr(route,"?");
				query = 1;
				if (queryPointer != NULL) {
					// Move to the actual query string
					queryPointer++;

					// Parse query parameters
					parse_query_parameters(queryPointer,queryData);
				}
			}
			
					
			if(strstr(route,".css") != NULL){
				send_css(connfd,fileName);	
			}
			else if(strcmp(method,"GET") == 0 && query == 1){
				handle_post_request(connfd,queryData,fileName);			
			}			
			else if(strcmp(method,"GET") == 0){
				handle_get_request(connfd,fileName);			
			}else if(strcmp(method,"POST") == 0){			
				char* content_ptr = strstr(buff,"Content-Length:");	
				if(content_ptr != NULL){
					int content_length;
					sscanf(content_ptr, "Content-Length: %d", &content_length);
					content_ptr = strstr(buff,"\r\n\r\n");
					
					char content[content_length+1];
					sscanf(content_ptr, "\r\n\r\n%[^\n]s", content);
					content[content_length] = '\0';
					url_decode(content);
					handle_post_request(connfd, content,fileName);
				}
				else {
					// Handle POST request without Content-Length header
					send_response(connfd, "411 Length Required", "text/plain", "Content-Length header is required for POST");
	    			}		
			}
			else{
				send_response(connfd, "501 Not Implemented", "text/plain", "Method Not Implemented");
			}
		
			close(connfd); 
			exit(0);
		} 
		close(connfd);  
	} 
	close(sockfd); 
	return 0;
} 

