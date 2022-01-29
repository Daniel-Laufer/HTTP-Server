
#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h> 
#include <sys/stat.h>
#include <string.h>
#include "server_helpers.h"

/* implementation of the helper functions */

/**
 * Function responsible for communicating with client
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
            
            return;
        }

        if (strlen(fname) > 0)
        {
            
            // opening file for reading
            if (!(fp = fopen(fname, "rb")))
            {
                free(fname);
                
                perror("error in reading file");
                char status_code_header[] = "HTTP/1.0 404 Not Found";
                char content_type_header[] = "Content-type: text/html";
                char body[] = "<h1 style='text-align: center;'>File not found</h1>";
                char response[MAX];
                char *http_status_code = "HTTP/1.0 404 Not found";
                 time_t now = time(&now);
                 struct tm *current_time = gmtime(&now);
                 char *date_header = asctime(current_time);
                int num_printed = sprintf(response, "%s\r\n%s\r\n\r\n%s", http_status_code, date_header, body);
                response[num_printed] = '\0';
                send_response(connfd, writebuff, response, 1);
                return;
            }

            // if an If-Modified-Since header is provided then conditionally return a 304 Not Modified status with no body.
            printf("\n\n%s\n", fname);
            if(!has_requested_file_been_modified_since(fname, buff)){
                free(fname);
                char response[MAX];
                char *http_status_code = "HTTP/1.0 304 Not Modified";
                 time_t now = time(&now);
                 struct tm *current_time = gmtime(&now);
                 char *date_header = asctime(current_time);
                int num_printed = sprintf(response, "%s\r\n%s\r\n\r\n", http_status_code, date_header); // no body sent for 304 response
                response[num_printed] = '\0';
                
                send_response(connfd, writebuff, response, 1) ; // 1 => close the connection 
                return;
            }
            else{
                char response[MAX];
                char *http_status_code = "HTTP/1.0 200 Success";
                 time_t now = time(&now);
                 struct tm *current_time = gmtime(&now);
                 char *date_header = asctime(current_time);
                 char content_type_header[30];
                 sprintf(content_type_header, "Content-type: %s", extract_ftype(fname));
                int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n\r\n", http_status_code, date_header, content_type_header); 
                response[num_printed] = '\0';
                
                send_response(connfd, writebuff, response, 0); // 0 => don't close the connection yet

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
                free(fname);
                char response[MAX];
                char *http_status_code = "HTTP/1.0 304 Not Modified";
                 time_t now = time(&now);
                 struct tm *current_time = gmtime(&now);
                 char *date_header = asctime(current_time);
                int num_printed = sprintf(response, "%s\r\n%s\r\n\r\n", http_status_code, date_header); // no body sent for 304 response
                response[num_printed] = '\0';
                
                send_response(connfd, writebuff, response, 1) ; // 1 => close the connection 
                return;
            }
            
            char response[MAX];
                char *http_status_code = "HTTP/1.0 200 Success";
                 time_t now = time(&now);
                 struct tm *current_time = gmtime(&now);
                 char *date_header = asctime(current_time);
                 char content_type_header[] = "Content-type: text/html";
                int num_printed = sprintf(response, "%s\r\n%s\r\n%s\r\n\r\n", http_status_code, date_header, content_type_header); 
                response[num_printed] = '\0';
                
                send_response(connfd, writebuff, response, 0); // 0 => don't close the connection yet

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
    int modified_since_key_length = 17;
    int modified_since_value_length = 25;


    for(int i=0; i< MAX - modified_since_key_length; i++){
        if(strncmp(request_info + i, "If-Modified-Since",  modified_since_key_length) == 0){
            strncpy(date_time_str, request_info + i + modified_since_key_length + 2, modified_since_value_length); 
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


/*
Sends a response back to the client and conditionally drops the connection. If successfull, return a 1, otherwise 0. 
*/
int send_response(int connfd, char *writebuff, char *message_to_write, int close_connection){
    printf("===================");
    printf("\n\nSending following response to client:\n%s\n", message_to_write);
    int num_written = write(connfd, message_to_write, strlen((char *)message_to_write));
    printf("\n%d bytes were written to the client.\n", num_written);
    printf("===================\n");
    if(close_connection)
        close(connfd);
    return 1;
}
