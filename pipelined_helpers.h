
#define MAX 256
#define PIPELINED_SERVER_PORT 8099
#define SA struct sockaddr
#define MAX_CONNECTIONS 5


/*
Responsible for communicating with client through a pipelined HTTP connection
*/
void *pipelined_communication_with_client(void *connfd);

/*
* Responsible for handling a single request and sending a response
*/
void *handle_request(void *arg);

/*
Return the size of this file (number of bytes).
*/
long get_file_size(FILE *fp);

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
Sends a response back to the client and conditionally drops the connection. If successfull, return a 1, otherwise 0. 
*/
int send_response(int connfd, char *writebuff, char *message_to_write);