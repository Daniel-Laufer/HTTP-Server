
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


/*
Responsible for communicating with client through a non-persistent HTTP connection
*/
void non_persistent_communication_with_client(int connfd);




// Driver function
int main()
{
    int listenfd, connfd;

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

    // fixing the socket bind failed issue
    int yes = 1;
    if((setsockopt(listenfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int))) == -1) {
        perror("setsockopt");
    }
    bzero(&servaddr, sizeof(servaddr));

    // assign IP, NON_PERSIS_SERVER_PORT
    servaddr.sin_family = AF_INET;

    // binding the socket to any possible IP address
    servaddr.sin_addr.s_addr = htonl(INADDR_ANY);

    // setting up the port the server socket will be listening to
    servaddr.sin_port = htons(NON_PERSIS_SERVER_PORT);

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
        non_persistent_communication_with_client(connfd);
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
void non_persistent_communication_with_client(int connfd)
{
    FILE *fp;
    char *fname;
    char *fpath;
    char buff[MAX];
    char buff2[MAX];
    char writebuff[MAX];
    int n;
    bzero(buff, MAX);
    time_t now = time(NULL);
    char content_length_header[MAX];
    char date_header[32];
    long file_size;
    char response[1000];
    char *website_dir = "website/";
    int website_dir_length = strlen(website_dir);
    char *connection_close_header = "Connection: close";

    int check = 0;
    // read the message from client and copy it in buffer
    while ((n = read(connfd, buff2, MAX - 1)) > 0)
    {
        // hacky way to detect the end of the message.
        printf("%s\n", buff);
        if (is_http_method_get(buff2))
        {
            fname = extract_fname(buff2);
            check = 1;
            memcpy(buff, buff2, MAX);
        }

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


    if (fname == NULL)
    {
        free(fname);
        close(connfd);
        return;
    }

    if (strlen(fname) == 0)
    {
        free(fname);
        fname = malloc(sizeof(char) * 11);
        memcpy(fname, "index.html", 11);
        fname[10] = '\0';
    }

    //appending path with fname
    fpath = malloc(sizeof(char) * (website_dir_length + strlen(fname) + 1));
    memcpy(fpath, website_dir, website_dir_length);
    memcpy(fpath + website_dir_length, fname, strlen(fname));
    fpath[website_dir_length + strlen(fname)] = '\0';
    // free(fname);

    // opening file for reading
    if (!(fp = fopen(fpath, "rb")))
    {
        free(fpath);

        perror("error in reading file");
        // generating response headers
        // char status_code_header[] = "HTTP/1.0 404 Not Found";
        // char content_type_header[] = "Content-type: text/html";
        char body[] = "<h1 style='text-align: center;'>File not found</h1>";

        char *http_status_code = "HTTP/1.0 404 Not found";

        strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
        date_header[31] = '\0';

        int num_printed = sprintf(response, "%s\r\n%s\r\n\r\n%s", http_status_code, date_header, body);
        response[num_printed] = '\0';
        send_response(connfd, writebuff, response);
        close(connfd);
        return;
    }

    // if an If-Modified-Since header is provided then conditionally return a 304 Not Modified status with no body.
    if (!has_requested_file_been_modified_since(fpath, buff))
    {
        free(fpath);
        // generating response headers
        char *http_status_code = "HTTP/1.0 304 Not Modified";

        strftime(date_header, 32, "Date: %a, %d %b %Y %H:%M:%S", gmtime(&now));
        date_header[31] = '\0';

        sprintf(content_length_header, "Content-length: 0");

        int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n%s\r\n\r\n", http_status_code, connection_close_header, content_length_header, date_header); // no body sent for 304 response
        response[num_printed] = '\0';
        // end generating response headers
        send_response(connfd, writebuff, response);
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

        send_response(connfd, writebuff, response);
        // Sending the body of the request
        if (send_file(fp, fpath, connfd) < 0)
        {
            free(fpath);
            return;
        }
         close(connfd);
    }

    free(fpath);
}