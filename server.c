
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>

#define MAX 200
#define SERVER_PORT 8090
#define SA struct sockaddr

// change the file name to what you would like
#define FILENAME "index.html"

// Helper function designed for chat between client and server.
void func(int connfd);

/** 
 * Sends a given file with file name <fname> to socket <tarsocket>.
 * Upon an error, -1 is returned. Otherwise, if the operation was successful, 
 * 0 is returned.
 */
int send_file(char *fname, int tarsocket);

/**
 * Returns the content-type value based on the given
 * file name <fname>.
 */
const char *extract_ftype(char *fname);

// Driver function
int main()
{
    int listenfd, connfd, len;

    // specifies a transport address and port for the AF_INET address family
    struct sockaddr_in servaddr;

    // creating a socket over domain: AF_INET (IPv4 protocol), SOCK_STREAM: TCP, protocol: 0 = IP
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, SERVER_PORT
    servaddr.sin_family = AF_INET;

    // this is an IP address that is used when we don't want to bind a socket to any specific IP
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // setting up the port the server socket will be listening to
    servaddr.sin_port = htons(SERVER_PORT);

    // Binding newly created socket to given IP and port
    if ((bind(listenfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("socket bind failed...\n");
        exit(0);
    }

    // Now server is ready to listen for new connections
    if ((listen(listenfd, 5)) != 0)
    {
        printf("Listen failed...\n");
        exit(0);
    }
    printf("Server listening..\n");

    // flush whatever is in the stdout buffer
    fflush(stdout);

    // Accept the data packet from client and verification
    connfd = accept(listenfd, (SA *)NULL, NULL);
    if (connfd < 0)
    {
        printf("server accept failed...\n");
        exit(0);
    }
    printf("server accept the client...\n");

    // Function for chatting between client and server
    func(connfd);

    // After chatting close the socket
    close(listenfd);
}

/* implementation of the helper function */
void func(int connfd)
{
    char buff[MAX];
    char writebuff[MAX];
    int n;
    bzero(buff, MAX);

    // read the message from client and copy it in buffer
    while ((n = read(connfd, buff, MAX - 1)) > 0)
    {
        // hacky way to detect the end of the message.
        printf("%s\n", buff);
        if (buff[n - 1] == '\n')
        {
            break;
        }
        memset(buff, 0, MAX);

        // Sending the header of the request first
        snprintf((char *)writebuff, sizeof(writebuff),
                 "HTTP/1.0\r\n"
                 "Content-type: %s\r\n\r\n",
                 extract_ftype(FILENAME));

        write(connfd, (char *)writebuff, strlen((char *)writebuff));

        // Sending the body of the request
        if (send_file(FILENAME, connfd) < 0)
        {
            perror("could not send file");
            exit(1);
        }

        close(connfd);
    }
}

/** 
 * Sends a given file with file name <fname> to socket <tarsocket>.
 * Upon an error, -1 is returned. Otherwise, if the operation was successful, 
 * 0 is returned.
 **/
int send_file(char *fname, int tarsocket)
{
    FILE *fp = fopen(fname, "rb");
    if (fp == NULL)
    {
        perror("error in reading file");
        return -1;
    }

    // determining the file's size first
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *writebuff = malloc(sizeof(char) * size);
    int read_bytes = fread(writebuff, sizeof(char), size, fp);
    if (read_bytes <= 0)
        return -1;

    // sending the contents of the file to the client socket
    send(tarsocket, writebuff, read_bytes, 0);

    // cleaning up
    free(writebuff);
    fclose(fp);
    return 0;
}

/**
 * Returns the content-type value based on the given
 * file name <fname>.
 **/
const char *extract_ftype(char *fname)
{
    char *rest = strrchr(fname, '.');

    if (strcmp(rest, ".html") == 0)
    {
        return "text/html";
    }

    else if (strcmp(rest, ".css") == 0)
    {
        return "text/css";
    }

    else if (strcmp(rest, ".js") == 0)
    {
        return "text/javascript";
    }

    else if (strcmp(rest, ".jpg") == 0)
    {
        return "image/jpeg";
    }

    return "text/plain";
}