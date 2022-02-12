

#include <stdio.h>
#include <netdb.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include "server_helpers.h"
#define __USE_XOPEN
#include <time.h>
#include <pthread.h>
#include <poll.h>
#include <signal.h>

/* implementation of the helper functions */

void log_to_file(char *message)
{
    FILE *logger = fopen("logfile.txt", "a");
    fputs(message, logger);
    fclose(logger);
}


// do nothing if the client disconnects 
void sig_handler(int signum){
    printf("SIGPIPE, Client disconnected\n");
}



/**
 * Return 1 if the client sent a GET HTTP request, 0 otherwise.
 */
int is_http_method_get(char *request)
{
    char request_type[4];
    memcpy(request_type, &request[0], 3);
    request_type[3] = '\0';

    if ((strncmp(request_type, "GET", 3)) == 0)
        return 1;

    return 0; // unsupported HTTP method;
}

/**
 * Returns the value of the If-Modified-Since Header (a date represented by a string)
 */
struct tm *_extract_modified_since_value(char *request_info)
{
    char date_time_str[25];
    int modified_since_key_length = 17;
    int modified_since_value_length = 25;

    for (int i = 0; i < MAX - modified_since_key_length; i++)
    {
        if (strncmp(request_info + i, "If-Modified-Since", modified_since_key_length) == 0)
        {
            strncpy(date_time_str, request_info + i + modified_since_key_length + 2, modified_since_value_length);
            struct tm *tm = malloc(sizeof(struct tm));
            char *ret;
            if ((ret = strptime(date_time_str, "%a, %d %b %Y %H:%M:%S", tm)) == NULL)
            {
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
        // as soon as we get to a whitespace we know we finished parsing the filename
        if (request_info[i + 1] == ' ')
            break;

        i++;
        size++;
    }

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

    else if (strcmp(rest, ".jpg") == 0 || strcmp(rest, ".jpeg") == 0)
    {
        return "image/jpeg";
    }

    return "text/plain";
}

/*
Return the size of this file (number of bytes).
*/
long get_file_size(FILE *fp)
{
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    return size;
}

/**
 * Given two integers <a> and <b>, returns
 * the minimum of the two.
 */
int min(int a, int b)
{
    if (a > b)
        return b;
    return a;
}

/** 
 * Sends a file given a pointer to that file, with file name <fname> to target socket <tarsocket>.
 * Upon an error, -1 is returned. Otherwise, if the operation was successful, 
 * 0 is returned.
 */
int send_file(FILE *fp, char *fname, int tarsocket)
{
    signal(SIGPIPE,sig_handler);
    // determining the file's size first
    long bytes_left = get_file_size(fp);
    int read_bytes;
    char buff[512];
    int actual_sent = 0;

    // while there are no bytes left to be sent
    while (bytes_left > 0)
    {
        // reading and sending one byte a time to ensure
        // no data loss occurs
        int min_to_send = min(sizeof(buff), bytes_left);
        read_bytes = fread(buff, sizeof(char), min_to_send, fp);
        if (read_bytes <= 0 || read_bytes < min_to_send)
        {
            perror("send_file");
            return -1;
        }

        actual_sent = send(tarsocket, buff, read_bytes, 0);
        if (actual_sent == -1) {
            fclose(fp);
            return -1;
        }
        bytes_left -= actual_sent;

        // not everything was sent
        if (actual_sent < min_to_send)
        {
            // go back to retry send
            fseek(fp, -(min_to_send - actual_sent), SEEK_CUR);
        }
    }

    fclose(fp);
    return 0;
}

/*
Returns 1 if the file with the path <fname> has been modified since
the date-time specified in the If-Modified-Since Header *or* if the
If-Modified-Since Header isn't present *or* its value is incorrectly formatted
(failing silently). 
*/
int has_requested_file_been_modified_since(char *fname, char *buff)
{
    struct tm *if_modified_since_date_time = _extract_modified_since_value(buff);
    if (if_modified_since_date_time == NULL)
        return 1;

    struct stat attr;
    stat(fname, &attr);

    // creating a tm struct for the file's modification time and then convert it to unix time
    struct tm *file_last_modified_date = localtime(&(attr.st_ctime));
    time_t raw_file_last_modified_date = mktime(file_last_modified_date);

    // converting if_modified_since_date_time into unix time
    time_t raw_if_modified_since_date_time = mktime(if_modified_since_date_time);
    free(if_modified_since_date_time); // don't need this if_modified_since_date_time tm struct any longer

    // file HAS been modified since the value of  the If-Modified-Since header
    if (raw_file_last_modified_date >= raw_if_modified_since_date_time)
        return 1;

    return 0;
}

/*
Sends a response back to the client and conditionally drops the connection. If successful, return a 1, otherwise 0. 
*/
int send_response(int connfd, char *writebuff, char *message_to_write)
{
    printf("===================");
    printf("\n\nSending following response to client:\n%s", message_to_write);
    write(connfd, message_to_write, strlen((char *)message_to_write));
    
    printf("===================\n");
    return 1;
}
