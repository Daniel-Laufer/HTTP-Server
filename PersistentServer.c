
#include <stdio.h>
#include <sys/stat.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <pthread.h>

#define MAX 200
#define SERVER_PORT 8099
#define MAX_CONNECTIONS 5
#define SA struct sockaddr

// A function to handle a single persistent HTTP connection 
void *handle_request(void *connfd);

/** 
 * Sends a file given a pointer to that file, with file name <fname> to target socket <tarsocket>.
 * Upon an error, -1 is returned. Otherwise, if the operation was successful, 
 * 0 is returned.
 */
int send_file(FILE *fp, char *fname, int tarsocket);

/**
 * Returns the content-type value based on the given
 * file name <fname>.
 */
const char *extract_ftype(char *fname);

/**
 * Returns the the file name given the <request_info>
 * sent from the client. Remember to free return value once
 * there is no use to it any longer.
 */
char *extract_fname(char *request_info);

/**
* Return the file size
*/ 
int get_size(char *fname);


// Driver function
int main()
{
    int listenfd, connfd;
    pthread_t id[5];

    // specifies a transport address and port for the AF_INET address family
    struct sockaddr_in servaddr;

    // creating a socket over domain: AF_INET (IPv4 protocol), SOCK_STREAM: TCP, protocol: 0 = IP
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        printf("socket creation failed...\n");
        exit(0);
    }
    printf("Socket successfully created..\n");

    bzero(&servaddr, sizeof(servaddr));

    // assign IP, SERVER_PORT
    servaddr.sin_family = AF_INET;

    // binding the socket to any possible IP address
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // setting up the port the server socket will be listening to
    servaddr.sin_port = htons(SERVER_PORT);

    // binding newly created socket to the given IP address and port
    if ((bind(listenfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
    {
        printf("socket bind failed...\n");
        exit(0);
    }

    // now server is ready to listen for new connections
    if ((listen(listenfd, 5)) != 0)
    {
        printf("Listen failed...\n");
        exit(0);
    }
    printf("Server listening..\n");

    // flush whatever is in the stdout buffer
    fflush(stdout);

    // Accept the data packet from client and verification
    while ((connfd = accept(listenfd, (SA *)NULL, NULL)) >= 0)
    {
        printf("server accept the client...\n");
        // Function for chatting between client and server
        pthread_create(&id[0], NULL, handle_request, &connfd);

    }

    if (connfd < 0)
    {
        printf("server accept failed...\n");
        exit(0);
    }

    // After chatting close the socket
    close(listenfd);
}

/* implementation of the helper functions */

/**
 * Function in charge of servicing the client based on its request
 */
void *handle_request(void *arg)
{
    int connfd = *(int *) arg;
    while (1) {
        printf("[LOG] %d In main loop\n", connfd);
        usleep(1000000);
        FILE *fp;
        char *fname;
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
                printf("[LOG] %d Detected end of line\n", connfd);
                break;
            }
            fname = extract_fname(buff);
            memset(buff, 0, MAX);

            if (!fname)
            {   
                // TODO: Handle invalid requests better. 
                // For now - ignore additional fields obtained from read. 
                continue;     
            }

            if (strlen(fname) > 0)
            {
                if (!(fp = fopen(fname, "rb")))
                {
                    free(fname);
                    printf("error in reading file");
                    snprintf((char *)writebuff, sizeof(writebuff),
                            "HTTP/1.0 404 NOT FOUND\r\n"
                            "Connection: close\r\n"
                            "Content-type: text/html\r\n\r\n"
                            "<h1 style='text-align: center;'>File  not found</h1>");
                    write(connfd, (char *)writebuff, strlen((char *)writebuff));
                    close(connfd);
                    pthread_exit(NULL);
                }

                // Sending the header of the request first
                snprintf((char *)writebuff, sizeof(writebuff),
                        "HTTP/1.1\r\n"
                        "Connection: keep-alive\r\n"
                        "Keep-Alive: timeout=300, max=1000\r\n"
                        "Content-Length: %d\r\n"
                        "Content-type: %s\r\n\r\n",
                        get_size(fname),
                        extract_ftype(fname));

                write(connfd, (char *)writebuff, strlen((char *)writebuff));
                printf("[LOG] Starting to send the body of the request\n");
                // Sending the body of the request
                if (send_file(fp, fname, connfd) < 0)
                {
                    free(fname); 
                }
                printf("[LOG] %d File send completed\n", connfd);
            }

            // requesting root
            else
            {
                int content_length = get_size("index.html");
                snprintf((char *)writebuff, sizeof(writebuff),
                        "HTTP/1.0 200 OK\r\n"
                        "Content-Length: %d\r\n"
                        "Content-type: text/html\r\n\r\n", content_length);
                write(connfd, (char *)writebuff, strlen((char *)writebuff));

                if (!(fp = fopen("index.html", "rb")) || send_file(fp, "index.html", connfd) < 0)
                {
                    // nothing to send (we can change it to something else later)
                    snprintf((char *)writebuff, sizeof(writebuff), "HTTP/1.0 200 OK\r\n\r\n");
                    write(connfd, (char *)writebuff, strlen((char *)writebuff));
                }
            }

            free(fname);
            
        }
    }

    // TODO: This is never reached at the moment. 
    // Instead, need to wait 10 seconds. 
    close(connfd);
    pthread_exit(NULL);
}

/** 
 * Sends a file given a pointer to that file, with file name <fname> to target socket <tarsocket>.
 * Upon an error, -1 is returned. Otherwise, if the operation was successful, 
 * 0 is returned.
 */
int send_file(FILE *fp, char *fname, int tarsocket)
{
    // determining the file's size first
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);

    char *writebuff = malloc(sizeof(char) * size);
    int read_bytes = fread(writebuff, sizeof(char), size, fp);
    if (read_bytes <= 0)
    {
        perror("send_file");
        return -1;
    }

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
 */
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

/**
* Get file size 
*/
int get_size(char *fname) {
    struct stat st;
    stat(fname, &st);
    return st.st_size; 
}

/**
 * Returns the the file name given the <request_info>
 * sent from the client. Remember to free return value once
 * there is no use to it any longer.
 */
char *extract_fname(char *request_info)
{
    // ensuring that the request is of type GET
    char request_type[4];
    memcpy(request_type, &request_info[0], 3);
    request_type[4] = '\0';
    if ((strcmp(request_type, "GET")) != 0)
    {
        return NULL;
    }
    else
    {
        // start from index 4 since the beginning will be "GET "
        int i = 4, size = 0;

        // loop until we reach " HTTP"
        while (1)
        {
            // as soon as we get to HTTP/ we know we finished parsing the filename
            if (request_info[i + 1] == '/')
                break;

            i++;
            size++;
        }

        // subtracting " HTTP" from file name
        size -= 5;

        char *fname = malloc(sizeof(char) * (size + 1));
        memcpy(fname, &request_info[5], size);
        fname[size] = '\0';
        return fname;
    }
}