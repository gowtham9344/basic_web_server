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


// defining functions for post and get send response
void send_response(int client_socket,const char* status, const char* content_type,const char* content){
	char response[2048]; 
	sprintf(response, "HTTP/1.1 %s\r\nContent-Type: %s\r\n\r\n%s", status, content_type, content);
	send(client_socket, response, strlen(response), 0);
}

// handle saving of data into the file 
void store_data(int client_socket,char content[1024]){
	    // Open a file for writing
	    FILE *file = fopen("output.txt", "a"); // Open in 'a' (append) mode to append new data
	    if (file == NULL) {
		perror("Error opening file");
		send_response(client_socket, "500 Internal Server Error", "text/plain", "Error opening file");
		return;
	    }

	    // Write the decoded data to the file
	    fprintf(file, "%s\n", content);

	    // Close the file
	    fclose(file);
}

// retrieve all data from the file
void getalldata(int client_socket,char content[1024]){
	    // Open the file for reading
	    FILE *file = fopen("output.txt", "r");
	    if (file == NULL) {
		perror("Error opening file");
		send_response(client_socket, "500 Internal Server Error", "text/plain", "Error opening file");
		return;
	    }
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

void handle_get_request(int client_socket) {
    char buff[1024];
    getalldata(client_socket,buff); 
    char html_content[1024];
    sprintf(html_content,"<html><body>%s</body></html>",buff);
    // HTML content for browser request

    send_response(client_socket, "200 OK", "text/html", html_content);
}

void handle_post_request(int client_socket, char content[]) {
    // HTML  content for Post request
    char* p;
    while((p = strstr(content,"=")) != NULL){
    	*p = ':';
    }
    char response_content[1024];
    store_data(client_socket,content);
    sprintf(response_content, "<html><body>Data is posted : </br>%s</body></html>", content);
    send_response(client_socket, "200 OK", "text/html", response_content);
}


// Function to decode URL-encoded string
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





void sigchld_handler(int s){
	int saved_errno = errno;
	while(waitpid(-1,NULL,WNOHANG) > 0);
	errno = saved_errno;
}

void *get_in_addr(struct sockaddr *sa){
	if(sa->sa_family == AF_INET){
		return &(((struct sockaddr_in*)sa)->sin_addr);	
	}
	return &(((struct sockaddr_in6*)sa)->sin6_addr);
}


int main(){ 
	int sockfd,connfd; 
	struct addrinfo hints,*servinfo,*p;
	struct sockaddr_storage their_addr;
	socklen_t sin_size;
	struct sigaction sa;
	int yes = 1;
	char s[INET6_ADDRSTRLEN];
	int rv;
	
	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;// my ip
	
	if((rv = getaddrinfo(NULL,PORT,&hints,&servinfo)) != 0){
		fprintf(stderr, "getaddrinfo: %s\n",gai_strerror(rv));	
		return 1;
	}
	
	// loop through all the results and bind to the first we can
	for(p = servinfo; p!= NULL; p=p->ai_next){
		sockfd=socket(p->ai_family,p->ai_socktype,p->ai_protocol);
		if(sockfd==-1){ 
			perror("server: socket\n"); 
			continue; 
		} 
		
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int)) == -1){
			perror("setsockopt");
			exit(1);	
		}
		
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
	

	if(listen(sockfd,BACKLOG) == -1){ 
		perror("listen");
		exit(1); 
	} 
	sa.sa_handler = sigchld_handler; // reap all dead processes
	sigemptyset(&sa.sa_mask);
	sa.sa_flags = SA_RESTART;
	if (sigaction(SIGCHLD, &sa, NULL) == -1) {
		perror("sigaction");
		exit(1);
	}
	printf("server: waiting for connections...\n");
	 
	while(1){ 
		sin_size = sizeof(their_addr); 
		connfd=accept(sockfd,(SA*)&their_addr,&sin_size); 
		if(connfd == -1){ 
			perror("\naccept error\n");
			continue;
		} 
		inet_ntop(their_addr.ss_family,get_in_addr((struct sockaddr *)&their_addr),s, sizeof(s));
		printf("\nserver: got connection from %s\n", s);
  
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
			printf("%s",buff);
        		sscanf(buff, "%s", method);
						
			
			if(strcmp(method,"GET") == 0){
				handle_get_request(connfd);			
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
					handle_post_request(connfd, content);
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

