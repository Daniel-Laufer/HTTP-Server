
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include "server_helpers.h"
#include <time.h>
#include <errno.h>

/*
Responsible for communicating with client through a non-persistent HTTP connection
*/
void non_persistent_communication_with_client(int connfd, char *root_path);

// Driver function
int main(int argc, char *argv[])
{
    // defining all required variables for the connection
    int listenfd, connfd;
    char *ptr;
    int16_t server_port;
    char *root_path;
    errno = 0;

    // specifies a transport address and port for the AF_INET address family
    struct sockaddr_in servaddr;

    if (argc != 3)
    {
        printf("Usage: ./reg_server <port> <root_path>\n");
        exit(0);
    }
    else
    {
        server_port = strtol(argv[1], &ptr, 10);
        root_path = argv[2];

        // if an invalid port or root_path was passed
        if (errno != 0)
        {
            perror("strtol");
            exit(0);
        }
    }

    // creating a socket over domain: AF_INET (IPv4 protocol), SOCK_STREAM: TCP, protocol: 0 = IP
    listenfd = socket(AF_INET, SOCK_STREAM, 0);
    if (listenfd == -1)
    {
        fprintf(stderr, "Socket creation failed...\n");
        exit(0);
    }
    else
        printf("Socket successfully created..\n");

    // Set socket to re-usable, so the server can be re-launched 
    // immediately after closing the socket. 
    int yes = 1;
    if ((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1)
        perror("setsockopt");
    bzero(&servaddr, sizeof(servaddr));

    // setting up the connection
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);
    servaddr.sin_port = htons(server_port);

    // binding newly created sockets to the given IP address and port
    if ((bind(listenfd, (SA *)&servaddr, sizeof(servaddr))) != 0)
    {
        fprintf(stderr, "Socket bind failed...\n");
        exit(0);
    }

    // now server is ready to listen for new connections
    if ((listen(listenfd, 5)) != 0)
    {
        fprintf(stderr, "Listen failed...\n");
        exit(0);
    }

    printf("Server listening..\n");

    // flush whatever is in the stdout buffer
    fflush(stdout);

    // Accept the data packet from client and verification
    while ((connfd = accept(listenfd, (SA *)NULL, NULL)) >= 0)
    {
        printf("Server accept the client...\n");

        // Function for chatting between client and server
        non_persistent_communication_with_client(connfd, root_path);
    }

    if (connfd < 0)
    {
        printf("server accept failed...\n");
        exit(0);
    }

    // After chatting close the socket
    close(listenfd);
}

/**
 * Function responsible for communicating with client
 */
void non_persistent_communication_with_client(int connfd, char *root_path)
{
    FILE *fp;
    char *fname, *fpath;
    char buff[MAX], buff2[MAX], writebuff[MAX], content_length_header[MAX];
    int n;
    bzero(buff, MAX);
    time_t now = time(NULL);
    char date_header[32], response[1000];
    long file_size;

    int root_path_length = strlen(root_path);
    char *connection_close_header = "Connection: close";

    int check = 0;

    // read the message from client and copy it to buffer
    while ((n = read(connfd, buff2, MAX - 1)) > 0)
    {
        if (is_http_method_get(buff2))
        {
            fname = extract_fname(buff2);
            check = 1;
            memcpy(buff, buff2, MAX);
        }

        // Detect the end of the message.
        if (buff2[n - 1] == '\n')
        {
            // not GET request was found yet end of request was reached
            if (check == 0)
            {
                close(connfd);
                return;
            }
            break;
        }
    }

    // if fname was not malloc'ed properly
    if (fname == NULL)
    {
        free(fname);
        close(connfd);
        return;
    }

    // if accessing index.html
    if (strlen(fname) == 0)
    {
        free(fname);
        fname = malloc(sizeof(char) * 11);
        memcpy(fname, "index.html", 11);
        fname[10] = '\0';
    }

    // appending path with fname
    if (root_path[strlen(root_path) - 1] != '/') // adding a slash at the end
    {
        fpath = malloc(sizeof(char) * (root_path_length + 1 + strlen(fname) + 1));
        memcpy(fpath, root_path, root_path_length);
        memcpy(fpath + root_path_length, "/", 1);

        memcpy(fpath + root_path_length + 1, fname, strlen(fname));
        fpath[root_path_length + 1 + strlen(fname)] = '\0';
    }
    else // not need to append slash at the end
    {
        fpath = malloc(sizeof(char) * (root_path_length + strlen(fname) + 1));
        memcpy(fpath, root_path, root_path_length);
        memcpy(fpath + root_path_length, fname, strlen(fname));
        fpath[root_path_length + strlen(fname)] = '\0';
    }

    // opening file for reading
    if (!(fp = fopen(fpath, "rb")) || fpath[strlen(fpath) - 1] == '/')
    {
        perror("error in reading file");

        // generating response headers
        char body[] = "<h1 style='text-align: center;'>File not found</h1>";

        char *http_status_code = "HTTP/1.0 404 Not found";

        strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
        date_header[31] = '\0';

        int num_printed = sprintf(response, "%s\r\n%s\r\n\r\n%s", http_status_code, date_header, body);
        response[num_printed] = '\0';

        // sending the response headers
        send_response(connfd, writebuff, response);

        // tidying up before closing socket
        free(fpath);
        free(fname);
        close(connfd);
        return;
    }

    // if an If-Modified-Since header is provided then conditionally return a 304 Not Modified status with no body.
    if (!has_requested_file_been_modified_since(fpath, buff))
    {
        // generating response headers
        char *http_status_code = "HTTP/1.0 304 Not Modified";

        strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
        date_header[31] = '\0';

        sprintf(content_length_header, "Content-length: 0");

        int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n%s\r\n\r\n", http_status_code, connection_close_header, content_length_header, date_header); // no body sent for 304 response
        response[num_printed] = '\0';
        // end generating response headers

        // sending the response headers
        send_response(connfd, writebuff, response);

        // tidying up before closing socket
        free(fpath);
        free(fname);
        close(connfd);
        return;
    }
    else
    {
        // generating response headers
        char *http_status_code = "HTTP/1.0 200 Success";

        strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
        date_header[31] = '\0';

        char content_type_header[30];
        sprintf(content_type_header, "Content-type: %s", extract_ftype(fpath));

        char content_length_header[MAX];

        file_size = get_file_size(fp);
        sprintf(content_length_header, "Content-length: %ld", file_size);

        int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n%s\r\n%s\r\n\r\n", http_status_code, date_header, content_length_header, connection_close_header, content_type_header);
        response[num_printed] = '\0';
        // end generating response headers

        // sending the response headers
        send_response(connfd, writebuff, response);

        // Sending the body of the request
        if (send_file(fp, fpath, connfd) < 0)
            return;
        close(connfd);
    }

    // there is no use for the path any longer
    free(fpath);
}