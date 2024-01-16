#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <openssl/ssl.h>
#include <openssl/err.h>

#define PORT "8050" 
#define MAX 100
#define SA struct sockaddr

void *get_in_addr(struct sockaddr *sa) {
    if (sa->sa_family == AF_INET) {
        return &(((struct sockaddr_in *)sa)->sin_addr);
    }
    return &(((struct sockaddr_in6 *)sa)->sin6_addr);
}

SSL_CTX *create_SSL_context() {
    SSL_CTX *ctx;

    // Initialize OpenSSL
    SSL_library_init();
    SSL_load_error_strings();

    // Create a new SSL context
    ctx = SSL_CTX_new(SSLv23_client_method());
    if (ctx == NULL) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    return ctx;
}

int client_creation(int argc, char *argv[]) {
    int sockfd;
    struct addrinfo hints, *servinfo, *p;
    char s[INET6_ADDRSTRLEN];
    int rv;

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo(argv[1], PORT, &hints, &servinfo)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(EXIT_FAILURE);
    }

    for (p = servinfo; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) {
            perror("client: socket");
            continue;
        }

        // Connect to the server
        if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            perror("client: connect");
            continue;
        }
        break;
    }

    if (p == NULL) {
        fprintf(stderr, "client: failed to connect\n");
        exit(EXIT_FAILURE);
    }

    // Print the IP address of the server
    inet_ntop(p->ai_family, get_in_addr((struct sockaddr *)p->ai_addr), s, sizeof(s));
    printf("client: connecting to %s\n", s);

    // Free the addrinfo structure
    freeaddrinfo(servinfo);

    return sockfd;
}

void send_request(SSL *ssl, char *host, char *fileName, char *body) {
    char request[1024];

    if (body == NULL) {
        sprintf(request, "GET /%s HTTP/1.1\r\nHost: %s\r\n\r\n", fileName, host);
    } else {
        sprintf(request, "POST /%s HTTP/1.1\r\nHost: %s\r\nContent-Length: %ld\r\n\r\n%s", fileName, host, strlen(body), body);
    }

    if (SSL_write(ssl, request, strlen(request)) == -1) {
        perror("send");
        exit(EXIT_FAILURE);
    }

    printf("%s request sent to the server\n", (body == NULL) ? "GET" : "POST");
}

void receive_message(SSL *ssl) {
    char buf[2048];
    int n;

    FILE *file = fopen("output.html", "w");
    if (file == NULL) {
        fprintf(stderr, "Can't open file output.html\n");
        exit(EXIT_FAILURE);
    }

    n = SSL_read(ssl, buf, sizeof(buf) - 1);
    buf[n] = '\0';
    char *content = strstr(buf, "\r\n\r\n");
    if (content != NULL) {
          fwrite(content + 4, 1, strlen(content + 4), file);
    }

    printf("Client: received message\n%s\n",buf);
}

void message_handler(SSL *ssl, char *host, char *fileName, char *body) {
    int req_method;
    printf("Enter Request method\n0.GET\n1.POST\n");
    scanf("%d", &req_method);

    if (!req_method) {
        send_request(ssl, host, fileName, NULL);
        receive_message(ssl);
    } else {
        char *post_body = (body == NULL) ? "name=ajay" : body;
        send_request(ssl, host, fileName, post_body);
        receive_message(ssl);
    }
}

int main(int argc, char *argv[]) {
    int sockfd;
    SSL_CTX *ctx;
    SSL *ssl;

    if (argc != 2) {
        fprintf(stderr, "usage: client hostname\n");
        exit(EXIT_FAILURE);
    }

    // Initialize SSL context
    ctx = create_SSL_context();

    // Create an SSL connection object
    ssl = SSL_new(ctx);

    // Create a TCP connection
    sockfd = client_creation(argc, argv);

    // Attach the SSL connection object to the socket file descriptor
    SSL_set_fd(ssl, sockfd);

    // Initiate SSL handshake
    if (SSL_connect(ssl) == -1) {
        ERR_print_errors_fp(stderr);
        exit(EXIT_FAILURE);
    }

    message_handler(ssl, argv[1], "file.txt", NULL);

    // Clean up
    SSL_free(ssl);
    close(sockfd);
    SSL_CTX_free(ctx);

    return 0;
}

