/*
 * proxy.c - A Simple Sequential Web proxy
 *
 * Course Name: 14:332:456-Network Centric Programming
 * Assignment 2
 * Student Name:Arya Shetty
 * 
 * IMPORTANT: Give a high level description of your code here. You
 * must also provide a header comment at the beginning of each
 * function that describes what that function does.
 *
 * This program creates a proxy for you to use in your browser
 * This allows you to only go through HTTP requests only
 * Having a manual proxy allows more functionality 
 * The program basically works as a server first allowing the browser connect to it from the port
 * Then once it recieves the request the proxy acts as a client and connects to the requested server
 * Next it reads the bytes from the server, and then finally writes back to the browser
 * It makes sure to keep the requests in the proxy.log
 */ 

#include "csapp.h"
#include <netinet/in.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * format_log_entry - Create a formatted log entry in logstring. 
 * 
 * The inputs are the socket address of the requesting client
 * (sockaddr), the URI from the request (uri), and the size in bytes
 * of the response from the server (size).
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, 
		      char *uri, int size)
{
    time_t now;
    char time_str[MAXLINE];
    unsigned long host;
    unsigned char a, b, c, d;

    /* Get a formatted time string */
    now = time(NULL);
    strftime(time_str, MAXLINE, "%a %d %b %Y %H:%M:%S %Z", localtime(&now));

    /* 
     * Convert the IP address in network byte order to dotted decimal
     * form. Note that we could have used inet_ntoa, but chose not to
     * because inet_ntoa is a Class 3 thread unsafe function that
     * returns a pointer to a static variable (Ch 13, CS:APP).
     */
    host = ntohl(sockaddr->sin_addr.s_addr);
    a = host >> 24;
    b = (host >> 16) & 0xff;
    c = (host >> 8) & 0xff;
    d = host & 0xff;


    /* Return the formatted log entry string */
    sprintf(logstring, "%s: %d.%d.%d.%d %s %d", time_str, a, b, c, d, uri, size);
}


/*
 * Function prototypes
 */
void format_log_entry(char *logstring, struct sockaddr_in *sockaddr, char *uri, int size);
char *parseClientRequest(char *clientInfo, char *method, char *uri, char *version, char hostName[1024], char pathName[1024]);

//This function parses the browser request
//We are able to retireve the URI, method, hostname, and pathName
char *parseClientRequest(char *clientInfo, char *method, char *uri, char *version, char hostName[1024], char pathName[1024]) {
    char delim[] = " "; 
    char *rem;
    hostName[0] = '\0';
    pathName[0] = '\0';

    method = strtok_r(clientInfo, delim, &rem);
    uri = strtok_r(NULL, delim, &rem);
    version = strtok_r(NULL, delim, &rem);

    sscanf(uri, "http://%[^/]%s", hostName, pathName);

    if (pathName[0] == '\0') {
        strcpy(pathName, "/");
    }

    printf("Uri Request: %s\n", uri);
    return uri;
}


/* 
 * main - Main routine for the proxy program 
 */
int main(int argc, char **argv)
{

    /* Check arguments */
    if (argc != 2) {
	fprintf(stderr, "Usage: %s <port number>\n", argv[0]);
	exit(0);
    }
    
    //Initalize the server, port, socket, and buffer
    int PORT = atoi(argv[1]);    
    int server_fd, server_request, client_socket;
    rio_t server_rio;
    struct sockaddr_in address;
    int address_len = sizeof(address);
    char buffer[1024];
    
    //This is to create the proxy server socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        perror("Socket Creation Error");
        exit(EXIT_FAILURE);
    }

    //Sets the address struct parts 
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_ANY);
    address.sin_port = htons(PORT);

    //binds server address and socket 
    if (bind(server_fd, (struct sockaddr*) &address, address_len) < 0) {
        perror("Binding Error");
        exit(EXIT_FAILURE);
    }

    //Listens for client connections 
    if (listen(server_fd, 1024) < 0) {
        perror("Listening Error");
        exit(EXIT_FAILURE);
    }
    
    //Create a loop to keep listening for browser requests until error occurs
    while (1) {

        //Accept the browser requests that connects to proxy client 
        if ((client_socket = accept(server_fd, (struct sockaddr*) &address, (socklen_t*) &address_len) < 0)) {
            perror("Could not connect to client");
            exit(EXIT_FAILURE);
        }
        
        //Get the client info, which includes uri and host
        recv(client_socket, buffer, sizeof(buffer), 0);

        //Make sure the request is a GET
        char delimiter[] = " ";
        char *firstWord, *context;
        char *bufferCopy = (char*) calloc(strlen(buffer) + 1, sizeof(char));
        strncpy(bufferCopy, buffer, strlen(buffer));
        firstWord = strtok_r(bufferCopy, delimiter, &context);

        if (strcmp(firstWord, "GET") == 0) {
            char *method = malloc(1024);
            char *uri = malloc(1024);
            char *logURI = malloc(1024);
            char *version = malloc(1024);
            char *pathName = malloc(1024);
            char *hostName = malloc(1024);

            //Call the parse function to get the uri and host
            logURI = parseClientRequest(buffer, method, uri, version, hostName, pathName);
            
            //CREATE THE REQUEST FOR THE SERVER
            int request_len = snprintf(NULL, 0, "GET %s HTTP/1.0\r\nHost: %s:%d\r\n\r\n", pathName, hostName, PORT) + 1;  
            char *request = (char *)malloc(request_len);
            if (request == NULL) {
                perror("Memory allocation failed for request");
            }
            snprintf(request, request_len, "GET %s HTTP/1.0\r\nHost: %s:%d\r\n\r\n", pathName, hostName, PORT);
            
            //Set the port for HTTP and request the server as a client proxy
            int serverPort = 80;
            server_request = Open_clientfd(hostName, serverPort);

            if (server_request < 0) {
                perror("Failed to connect to server request");
                free(request);
                exit(EXIT_FAILURE);
            }
            
            char buff[1024];

            //Read the server info
            Rio_readinitb(&server_rio, server_request);
            Rio_writen(server_request, request, strlen(request));
            free(request);

            //Get the response for the server to send back to browser
            ssize_t n;
            int bytes = 0;
            while ((n = rio_readlineb(&server_rio, buff, sizeof(buff))) != 0) {
                bytes += n;
                Rio_writen(client_socket, buff, n);
            }
            close(server_request);

            //Log the request with the uri, and bytes read
            char log[2048];
            printf("%d\n", bytes);
            format_log_entry(log, &address, logURI, bytes);
            FILE *logFp = Fopen("proxy.log", "a+");
            if (logFp) {
                fprintf(logFp, "%s\n", log);
                fflush(logFp);
                Fclose(logFp);
            }
            else {
                perror("Could not open file");
            }
        }

        //Close client to start new requested one
        close(client_socket);
    }

    exit(0);
}
