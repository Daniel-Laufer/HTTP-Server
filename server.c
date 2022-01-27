
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h> 
#include <ctype.h>
#include  <string.h>
#include <stdlib.h>
#include <sys/stat.h>


#define MAX 200
#define SERVER_PORT 8090
#define SA struct sockaddr



// Helper function designed for chat between client and server.
void communicate_with_client(int connfd);



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
 * Return 1 if the client sent a GET HTTP request, 0 otherwise.
 */
int is_http_method_get(char *request);



/*
Returns 1 if the file with the path <fname> has been modified since
the date-time specified in the If-Modified-Since Header *or* if the
If-Modified-Since Header isn't present *or* its value is incorrectly formatted
(failing silently). 
*/
int has_requested_file_been_modified_since(char *fname, char* buff );


/**
 * Returns the value of the If-Modified-Since Header (a date represented by a string)
 */
struct tm* _extract_modified_since_value(char *request_info);

/*
Converts a string that has the format Wed, 9 Sep 2015 19:03:54
into a struct date_time.
*/
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
        communicate_with_client(connfd);
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
void communicate_with_client(int connfd)
{
    FILE *fp;
    char *fname;
    char *http_method;
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
            break;
        
        fname = extract_fname(buff);

        // invalid HTTP method provided
        if(!is_http_method_get(buff)){
            perror("unrecognized HTTP method specified in request.");
            snprintf((char *)writebuff, sizeof(writebuff), "HTTP/1.0 405 Method Not Allowed\r\n\r\n");
            write(connfd, (char *)writebuff, strlen((char *)writebuff));
            close(connfd);
            return;
        }

        if (strlen(fname) > 0)
        {
            // opening file for reading
            if (!(fp = fopen(fname, "rb")))
            {
                free(fname);
                perror("error in reading file");
                snprintf((char *)writebuff, sizeof(writebuff),
                         "HTTP/1.0 404 NOT FOUND\r\n"
                         "Content-type: text/html\r\n\r\n"
                         "<h1 style='text-align: center;'>File  not found</h1>");
                write(connfd, (char *)writebuff, strlen((char *)writebuff));
                close(connfd);
                return;
            }

            // if an If-Modified-Since header is provided then conditionally return a 304 Not Modified status with no body.
            if(!has_requested_file_been_modified_since(fname, buff)){
                free(fname);
 
                snprintf((char *)writebuff, sizeof(writebuff),
                        "HTTP/1.0 304 Not Modified\r\n"
                        "Date: Sat, 10 Oct 2015 15:39:29\r\nServer: Apache/1.3.0 (Unix)\r\n\r\n"
                        "<h1 style='text-align: center;'>304 Not Modified</h1>");
                write(connfd, (char *)writebuff, strlen((char *)writebuff));
                close(connfd);
                return;
            }
            else{
                snprintf((char *)writebuff, sizeof(writebuff),
                        "HTTP/1.0\r\n"
                        "Content-type: %s\r\n\r\n",
                        extract_ftype(fname));

                write(connfd, (char *)writebuff, strlen((char *)writebuff));

                // Sending the body of the request
                if (send_file(fp, fname, connfd) < 0)
                {
                    free(fname);
                    return;
                }
            }
            
        }

        // requesting the root file
        else
        {
            char *root_fname = "index.html";
            // if an If-Modified-Since header is provided then conditionally return a 304 Not Modified status with no body.
            if(!has_requested_file_been_modified_since(root_fname, buff)){
                // Don't send the file.
                    free(fname);
                    snprintf((char *)writebuff, sizeof(writebuff),
                            "HTTP/1.0 304 Not Modified\r\n"
                            "Date: Sat, 10 Oct 2015 15:39:29\r\nServer: Apache/1.3.0 (Unix)\r\n\r\n"
                            "<h1 style='text-align: center;'>304 Not Modified</h1>");

                    write(connfd, (char *)writebuff, strlen((char *)writebuff));
                    close(connfd);
                    return;
            }
            
            snprintf((char *)writebuff, sizeof(writebuff),
                     "HTTP/1.0 200 OK\r\n"
                     "Content-type: text/html\r\n\r\n");

            write(connfd, (char *)writebuff, strlen((char *)writebuff));

            if (!(fp = fopen(root_fname, "rb")) || send_file(fp, root_fname, connfd) < 0)
            {
                // nothing to send (we can change it to something else later)
                snprintf((char *)writebuff, sizeof(writebuff), "HTTP/1.0 200 OK\r\n\r\n");
                write(connfd, (char *)writebuff, strlen((char *)writebuff));
            }
        }

        free(fname);
        close(connfd);
    }
}

/**
 * Return 1 if the client sent a GET HTTP request, 0 otherwise.
 */
int is_http_method_get(char *request){
    char request_type[4];
    int num_http_methods = 9;
    memcpy(request_type, &request[0], 3);
    request_type[3] = '\0';

    if ((strncmp(request_type, "GET", 3)) == 0) 
        return 1;
    
    
    return 0; // unsupported HTTP method;
 
}


/**
 * Returns the value of the If-Modified-Since Header (a date represented by a string)
 */
struct tm* _extract_modified_since_value(char *request_info)
{
    char *modified_since_date;
    char date_time_str[25];


    for(int i=0; i<MAX; i++){
        if(strncmp(request_info + i, "If-Modified-Since",  sizeof(char) * 17) == 0){
            strncpy(date_time_str, request_info + i + 19, sizeof(char) * 25); // plus 19 to go past the "If-Modified-Since: " header key
            struct tm *tm = malloc(sizeof(struct tm));
            char* ret;
            if((ret = strptime(date_time_str, "%a, %d %b %Y %H:%M:%S", tm)) == NULL){
                free(tm);
                return NULL; // a return of NULL means a conversion failed. Fail silently.
            } 
            return tm;
        }
            
    }
    return NULL;

        // loop through request_info until we reach If-Modified-Since
        // if our index >= MAX => return NULL
        // otherwise, use regex to check if the string start at index + 1 matches the format Wed, 09 Sep 2015 09:23:24
        //  compare this date to the modified date of the file we are sending. If the file has indeed been modified any time >= than  Wed, 9 Sep 2015 09:23:24 (in this example)
        //  send the image to the user. Otherwise, send a message along the lines of:
        // HTTP/1.1 304 Not Modified
        // Date: Sat, 10 Oct 2015 15:39:29
        // Server: Apache/1.3.0 (Unix)
    
}






/**
 * Returns the the file name given the <request_info>
 * sent from the client. Remember to free return value once
 * there is no use to it any longer.
 */
char *extract_fname(char *request_info)
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


/*
Returns 1 if the file with the path <fname> has been modified since
the date-time specified in the If-Modified-Since Header *or* if the
If-Modified-Since Header isn't present *or* its value is incorrectly formatted
(failing silently). 
*/
int has_requested_file_been_modified_since(char *fname, char* buff){
    struct tm* if_modified_since_date_time = _extract_modified_since_value(buff);
    if(if_modified_since_date_time == NULL) return 1;

    struct stat attr;
    stat(fname, &attr);

    // creating a tm struct for the file's modification time and then convert it to unix time
    struct tm *file_last_modified_date = localtime(&(attr.st_ctime));
    time_t raw_file_last_modified_date =  mktime(file_last_modified_date);
    
    // converting if_modified_since_date_time into unix time
    time_t raw_if_modified_since_date_time = mktime(if_modified_since_date_time);
    free(if_modified_since_date_time); // don't need this if_modified_since_date_time tm struct any longer


    // file HAS been modified since the value of  the If-Modified-Since header
    if(raw_file_last_modified_date >= raw_if_modified_since_date_time)
        return 1;

    

    return 0;
}
