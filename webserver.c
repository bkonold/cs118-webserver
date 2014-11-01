/* A simple server in the internet domain using TCP
The port number is passed as an argument 
This version runs forever, forking off a separate 
process for each connection
*/
#include <stdio.h>
#include <sys/types.h>   // definitions of a number of data types used in socket.h and netinet/in.h
#include <sys/socket.h>  // definitions of structures needed for sockets, e.g. sockaddr
#include <netinet/in.h>  // constants and structures needed for internet domain addresses, e.g. sockaddr_in
#include <stdlib.h>
#include <strings.h>
#include <string.h>
#include <sys/wait.h> /* for the waitpid() system call */
#include <signal.h> /* signal name macros, and the kill() prototype */
#include <unistd.h>
#include <time.h>

void sigchld_handler(int s)
{
    while(waitpid(-1, NULL, WNOHANG) > 0);
}

void handle_request(int); /* function prototype */
void error(char *msg)
{
    perror(msg);
    exit(1);
}

int main(int argc, char *argv[])
{
    int sockfd, newsockfd, portno, pid;
    socklen_t clilen;
    struct sockaddr_in serv_addr, cli_addr;
    struct sigaction sa;          // for signal SIGCHLD

    if (argc < 2) {
        fprintf(stderr,"ERROR, no port provided\n");
        exit(1);
    }
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        error("ERROR opening socket");
    }
    bzero((char *) &serv_addr, sizeof(serv_addr));
    portno = atoi(argv[1]);
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = INADDR_ANY;
    serv_addr.sin_port = htons(portno);

    if (bind(sockfd, (struct sockaddr *) &serv_addr, sizeof(serv_addr)) < 0) {
        error("ERROR on binding");
    }

    listen(sockfd,5);

    clilen = sizeof(cli_addr);

    /****** Kill Zombie Processes ******/
    sa.sa_handler = sigchld_handler; // reap all dead processes
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART;
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }
    /*********************************/

    while (1) {
        newsockfd = accept(sockfd, (struct sockaddr *) &cli_addr, &clilen);

        if (newsockfd < 0) {
            error("ERROR on accept");
        }

        pid = fork(); //create a new process
        if (pid < 0) {
            error("ERROR on fork");
        }

        if (pid == 0)  { // fork() returns a value of 0 to the child process
            close(sockfd);
            handle_request(newsockfd);
            exit(0);
        }
        else {//returns the process ID of the child process to the parent
            close(newsockfd); // parent doesn't need this 
        }
    } /* end of while */
    return 0; /* we never get here */
}

unsigned char*
file_to_str(FILE* file, unsigned int* len) {

    fseek(file, 0L, SEEK_END);
    *len = ftell(file);
    fseek(file, 0L, SEEK_SET);

    unsigned char* fileContents = malloc(*len+1);
    bzero(fileContents, *len+1);

    fread(fileContents, 1, *len, file); 

    return fileContents;
}

void handle_request(int sock)
{
    int n;
    char* request = malloc(256);
    size_t requestBufSize = 256;
    bzero(request, 256);
    int numChars = 0;
    char buffer[256];

    n = read(sock, buffer, 256);
    while (n > 0) {
        // resize string buffer if necessary
        if (numChars + n > requestBufSize) {
            request = realloc(request, 2*requestBufSize);
            // zero out new bytes
            bzero(request+requestBufSize, requestBufSize);
            requestBufSize *= 2;
        }

        // copy newly read bytes into request string
        memcpy(request+numChars, buffer, n);
        numChars += n;

        // check for end of request
        if (numChars > 3 && request[numChars-4] == '\r' && request[numChars-3] == '\n'
            && request[numChars-2] == '\r' && request[numChars-1] == '\n') {
            break;
        }

        bzero(buffer, 256);
        n = read(sock, buffer, 256);
    }

    if (n < 0) {
        free(request);
        error("ERROR reading from socket");
    }
    
    // add null byte to and print request (for part A)
    request = realloc(request, ++requestBufSize);
    request[requestBufSize-1] = 0;
    printf("%s\n", request);

    // grab length of request line
    int requestLineLength = strstr(request, "\r\n") - request;
    int fileNameLength = requestLineLength - 14;
    
    // extract file name
    char* fileName = malloc(fileNameLength+1);
    int i;
    for (i = 5; i < fileNameLength+5; i++) {
        fileName[i-5] = request[i];
    }
    fileName[i-5] = 0;
    
    // open file for reading
    FILE* file = fopen(fileName, "rb");
    
    // if file could not be opened, return 404
    if (!file) {
        n = write(sock, "HTTP/1.1 404 Not Found\r\n\r\n", 30);
        free(request);
        free(fileName);
        if (n < 0) {
            error("ERROR writing to socket");
        }
        return;
    }
    
    // read file into unsigned char array
    unsigned int fileLength;
    unsigned char* fileContents = file_to_str(file, &fileLength);
    char contentLength[11];
    bzero(contentLength, 11);
    sprintf(contentLength, "%u", fileLength);
    fclose(file);
    
    // get the MIME type of the file (for Content-Type header)
    char mimeTypeBuf[512];
    sprintf(mimeTypeBuf, "file -b --mime %s", fileName);
    FILE *mimeType = popen(mimeTypeBuf, "r");
 
    char contentType[256];
    fscanf(mimeType, "%[^\n]s", contentType);
    pclose(mimeType);
    
    // read current date into date buffer
    char date[512];
    bzero(date, 512);
    
    time_t epochTime = time(0);
    struct tm currTime = *gmtime(&epochTime);
    strftime(date, sizeof(date), "%a, %d %b %Y %H:%M:%S %Z", &currTime);

    // calulate the total number of bytes to be written to the socket
    unsigned int responseLength = 80 + strlen(date) + strlen(contentType) + strlen(contentLength) + fileLength;
    char* responseMSG = malloc(responseLength);
    bzero(responseMSG, responseLength);
    sprintf(responseMSG, "HTTP/1.1 200 OK\r\nDate: %s\r\nConnection: close\r\nContent-Type: %s\r\nContent-Length: %s\r\n\r\n", date, contentType, 
            contentLength);

    // copy message body into response buffer
    memcpy(responseMSG + (responseLength - fileLength), fileContents, fileLength);

    // write response to socket
    n = write(sock, responseMSG, responseLength);
    
    free(request);
    free(fileName);
    free(fileContents);
    free(responseMSG);

    if (n < 0) {
        error("ERROR writing to socket");
    }
    
}