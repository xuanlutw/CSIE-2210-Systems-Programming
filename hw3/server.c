/* B05202043 呂佳軒 */
#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <sys/select.h>
#include <time.h>
#include <fcntl.h>
#include <ctype.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <limits.h>
#include <signal.h>
#include <sys/wait.h>

#define TIMEOUT_SEC 5		// timeout in seconds for wait for a connection 
#define MAXBUFSIZE  1024	// timeout in seconds for wait for a connection 

#define NO_USE      0	    // status od a http request
#define ERROR	    -1	
#define READING     1		
#define WRITING     2		
#define CGI         3
#define LISTENING   4
#define INFO        5
#define WAITING     6

#define ERR_EXIT(a) { perror(a); exit(1); }
#define ERR_RET( error ) { *errP = error;printf("err_ret\n"); return -1; }
// return 0: success, file is buffered in retP->buf with retP->buf_len bytes
// return -1: error, check error code (*errP)
// return 1: read more, continue until return -1 or 0
// error code: 
// 1: client connection error 
// 2: bad request, cannot parse request
// 3: method not implemented 
// 4: illegal filename
// 5: illegal query
// 6: file not found
// 7: file is protected

typedef struct {
    char hostname[512];		// hostname
    unsigned short port;	// port to listen
    int listen_fd;		    // fd to wait for a new connection
} http_server;

typedef struct {
    int conn_fd;		    // fd to talk with client
    int pipe_read;          // fd read from CGI
    pid_t CGI_pid;          // CGI pid
    int status;			    // not used, error, reading (from client)
    // writing (to client)
    char file[MAXBUFSIZE];	// requested file
    char query[MAXBUFSIZE];	// requested query
    char host[MAXBUFSIZE];	// client host
    // head
    char* buf;  			// data sent by/to client
    size_t buf_len;	        // bytes used by buf
    size_t buf_size;     	// bytes allocated for buf
    // content
    char* buf_content;		// data sent by/to client
    size_t buf_len_content;	// bytes used by buf
    size_t buf_size_content;// bytes allocated for buf
    size_t but_done_content;
    size_t buf_idx; 		// offset for reading and writing
} http_request;

// Signal
void sig_chld(int signo);
void sig_usr1(int signo);

// my_fun
int valid_file(char* name);         // 0 : valid
int valid_query(char* name);        // -1: err
int multiplexing(fd_set* to_read, fd_set* to_write);

// control flow
void initialize();
void listening();
void info(http_request* reqP);
void reading(http_request* reqP);
void cgi(http_request* reqP);
void waiting(http_request* reqP);
void writing(http_request* reqP);
void error(http_request* reqP);

// modify net
int read_header(http_request* reqP);
void add_to_buf_head( http_request *reqP, char* str, size_t len );
void add_to_buf_content( http_request *reqP, char* str, size_t len );
void set_header(http_request* reqP, int code, char* add_mes);

// net
static void strdecode( char* to, char* from );
static int hexit( char c );
static char* get_request_line( http_request *reqP );
static void* e_malloc( size_t size );
static void* e_realloc( void* optr, size_t size );
static void init_http_server( http_server *svrP,  unsigned short port );    // initailize a http_request instance, exit for error
static void init_request( http_request* reqP );                             // initailize a http_request instance
static void free_request( http_request* reqP );                             // free resources used by a http_request instance
static void set_ndelay( int fd );                                           // Set NDELAY mode on a socket

static char* logfilenameP;	    // log file name
char* log_CGI;                  // log pointer
char* log_time;
char* log_file;
char* log_status;
int port;
http_request* requestP;         // pointer to http requests from client
http_server server;		        // http server
int maxfd;                      // size of open file descriptor table
int CGI_counter;
int* errP;

int main(int argc, char** argv) {
    if (argc != 3) {    // Parse args. 
        fprintf(stderr, "usage:  %s port# logfile\n", argv[0]);
        exit(1);
    }
    port = atoi(argv[1]);
    logfilenameP = argv[2];

    signal(SIGCHLD, sig_chld);
    signal(SIGUSR1, sig_usr1);
    initialize();

    while (1) {         // Main loop. 
        fd_set to_read;
        fd_set to_write;
        if (multiplexing(&to_read, &to_write) == -1) continue;                          // I/O multiplexing
        if (FD_ISSET(server.listen_fd, &to_read)) listening();                          // Acceptt new connection.
        for (int i = 0;i < maxfd;++i) {                                                 // Hadle request 
            int status = requestP[i].status;
            if (status == READING && FD_ISSET(i, &to_read)) reading(requestP + i);
            if (status == WRITING && FD_ISSET(i, &to_write)) writing(requestP + i);
            if (status == CGI && FD_ISSET(requestP[i].pipe_read, &to_read)) cgi(requestP + i);
            if (status == WAITING) waiting(requestP + i);
            if (status == ERROR) error(requestP + i);
        }
    }
    free(requestP);
    munmap(log_CGI, sizeof(char) * 301); 
    printf("umap ok \n");
    return 0;
}

// ======================================================================================================
// signal

void sig_chld(int signo){
    signal(SIGCHLD, sig_chld);
    int wstatus, ret = -1;
    pid_t pid = wait(&wstatus);
    for (int i = 0;i < maxfd;++i){
        if (pid == requestP[i].CGI_pid) ret = i;
    }
    if (ret == -1) return;
    http_request* reqP = requestP + ret;
    reqP->CGI_pid = 0;
    if (WEXITSTATUS(wstatus) != 2){
        ++CGI_counter;       //2 CGI not exist
        time_t current_time;
        char c_time_string[100];
        current_time = time(NULL);
        strcpy(c_time_string, ctime(&current_time));
        c_time_string[strlen(c_time_string) - 1] = '\0';
        memcpy(log_CGI, reqP->file, sizeof(char) * 100);
        memcpy(log_time, c_time_string, sizeof(char) * 100);
        memcpy(log_file, reqP->query + 9, sizeof(char) * 100);
        if (WEXITSTATUS(wstatus)) memcpy(log_status, "Fail", sizeof(char) * 100);
        else memcpy(log_status, "Success", sizeof(char) * 100);
        //printf("%s %s %s %s\n", log_CGI, log_time, log_file, log_status);
    }

    //printf("%d %d %d\n", ret, requestP[ret].CGI_pid, requestP[ret].status);
    if (WEXITSTATUS(wstatus) != 0){
        close(reqP->pipe_read);
        if (WEXITSTATUS(wstatus) == 1) set_header(reqP, 404, "file not found");
        if (WEXITSTATUS(wstatus) == 2) set_header(reqP, 404, "CGI file not found");
        reqP->status = WRITING;
        fprintf(stderr, "status of fd: %d change into WRITING (404 Not Found)\n", ret);
    }
    return;
}

void sig_usr1(int signo){
    signal(SIGUSR1, sig_usr1);
    //printf("i got siguse1\n");
    for (int i = 0;i < maxfd;++i){
        if (requestP[i].status != INFO) continue;
        info(requestP + i);
    }
    return;
}

// ======================================================================================================
// my_fun

int valid_file(char* name){
    while (*name != '\0'){
        char tmp = *name;
        if (!((tmp >= '0' && tmp <= '9') || (tmp >= 'a' && tmp <= 'z') || (tmp >= 'A' && tmp <= 'Z') || tmp == '_')) return -1;
        ++name;
    }
    return 0;
}

int valid_query(char* name){
    char t = name[9];
    name[9] = '\0';
    if (strcmp(name, "filename=")) return -1;
    name[9] = t;
    return valid_file(name + 9);
}

int multiplexing(fd_set* to_read, fd_set* to_write){
    struct timeval tv;
    tv.tv_sec = 1;
    tv.tv_usec = 0;
    FD_ZERO(to_read);
    FD_ZERO(to_write);
    FD_SET(server.listen_fd, to_read);
    for (int i = 0;i < maxfd;++i){
        int status = requestP[i].status;
        if (status == READING) FD_SET(i, to_read);
        if (status == WRITING) FD_SET(i, to_write);
        if (status == CGI) FD_SET(requestP[i].pipe_read, to_read);
    }
    int ret = select(maxfd, to_read, to_write, NULL, &tv);
    return ret;
}

// ======================================================================================================
// control_flow

void initialize(){
    init_http_server(&server, (unsigned short) port);
    maxfd = getdtablesize();
    CGI_counter = 0;
    errP = (int*)malloc(sizeof(int));

    requestP = (http_request*)malloc(sizeof(http_request) * maxfd);
    if (requestP == (http_request*)0){
        fprintf(stderr, "out of memory allocating all http requests\n");
        exit(1);
    }
    for (int i = 0; i < maxfd; i ++) init_request(&requestP[i]);
    requestP[server.listen_fd].conn_fd = server.listen_fd;
    requestP[server.listen_fd].status = LISTENING;

    int fd = open(logfilenameP, O_RDWR | O_TRUNC | O_CREAT, 0600); 
    if (fd<0) fprintf(stderr, "open log fail");
    lseek(fd, sizeof(char) * 400, SEEK_SET);        // CGI, time, file, status
    write(fd,"",1);                                 // status 0: success
    //        1: fail
    log_CGI = (char*) mmap(0, sizeof(char) * 400, PROT_READ|PROT_WRITE, MAP_SHARED, fd, 0);
    log_time = log_CGI + 100;
    log_file = log_CGI + 200;
    log_status = log_CGI + 300;
    printf("\nmmap address:%p\n",log_CGI); // 0x00000
    close(fd);

    fprintf(stderr, "starting on %.80s, port %d, fd %d, maxconn %d, logfile %s...\n", server.hostname, server.port, server.listen_fd, maxfd, logfilenameP);
}

void listening(){
    struct sockaddr_in cliaddr; // used by accept()
    int clilen;
    clilen = sizeof(cliaddr);
    int ret = accept(server.listen_fd, (struct sockaddr *) &cliaddr, (socklen_t *) &clilen);
    if (ret < 0) {
        if (errno == EINTR || errno == EAGAIN ) return; // try again 
        if (errno == ENFILE ) {
            (void) fprintf(stderr, "out of file descriptor table ... (maxconn %d)\n", maxfd);
            return;
        }	
        ERR_EXIT("accept")
    }
    free_request(requestP + ret);
    requestP[ret].conn_fd = ret;
    requestP[ret].status = READING;
    strcpy(requestP[ret].host, inet_ntoa(cliaddr.sin_addr));
    set_ndelay(ret);
    fprintf(stderr, "status of fd: %d change into READING (getting a new request... from %s)\n", ret, requestP[ret].host);
    return;
}

void info(http_request* reqP){
    reqP->buf_len_content = 0;
    char buf[10000];
    int buflen;
    buflen = snprintf(buf, sizeof(buf), "<html><head></head><body><h1>CGI Info</h1>%d processes died previously.<br>PIDs of Running Processes: ", CGI_counter);
    add_to_buf_content(reqP, buf, buflen);
    for (int i = 0;i < maxfd;++i){
        if (requestP[i].CGI_pid == 0) continue;
        buflen = snprintf(buf, sizeof(buf), "%d, ", requestP[i].CGI_pid);
        add_to_buf_content(reqP, buf, buflen);
    }
    buflen = snprintf(buf, sizeof(buf), "<br>Last Exit CGI: %s, %s<br>Open file: %s, %s.</body></html>", log_CGI, log_time, log_file, log_status);
    add_to_buf_content(reqP, buf, buflen);
    set_header(reqP, 200, NULL);
    reqP->status = WRITING;
    fprintf(stderr, "status of fd: %d change into WRITING (200 OK, info)\n", reqP->conn_fd);
}

void reading(http_request* reqP){
    int ret = read_header(reqP);
    if (ret > 0) return;            // still reading
    if (ret < 0) {                  // error for reading http header or requested file
        fprintf(stderr, "status of fd: %d change into ERROR (read header err)\n", reqP->conn_fd);
        reqP->status = ERROR;
        return;
    }

    // Info
    if (!strcmp(reqP->file, "info")){ 
        pid_t do_info;
        if ((do_info = fork()) < 0){
            fprintf(stderr, "fd %d fork err\n", reqP->conn_fd);
            reqP->status = ERROR;
            return;
        }
        if (do_info == 0){
            int k = 1;
            while (k) k = kill(getppid(), SIGUSR1);
            exit(0);
        }
        reqP->status = INFO;
        fprintf(stderr, "status of fd: %d change into INFO\n", reqP->conn_fd);
        return;
    }

    // Bad request
    if (valid_query(reqP->query) || valid_file(reqP->file)){
        if (valid_query(reqP->query)) set_header(reqP, 400, "file invalid");
        if (valid_file(reqP->file)) set_header(reqP, 400, "CGI invalid");
        reqP->status = WRITING;
        fprintf(stderr, "status of fd: %d change into WRITING (Bad Request)\n", reqP->conn_fd);
        return;
    }

    // Multiprocess
    int pipe_read[2];
    int pipe_write[2];
    if (pipe(pipe_read) < 0){
        fprintf(stderr, "fd %d pipe err\n", reqP->conn_fd);
        reqP->status = ERROR;
        return;
    }
    if (pipe(pipe_write) < 0){
        fprintf(stderr, "fd %d fork err\n", reqP->conn_fd);
        fprintf(stderr, "pipe err\n");
        reqP->status = ERROR;
        return;
    }
    if ((reqP->CGI_pid = fork()) < 0){
        fprintf(stderr, "fd %d fork err\n", reqP->conn_fd);
        reqP->status = ERROR;
        reqP->CGI_pid = 0;
        return;
    }
    reqP->status = CGI;
    if (reqP->CGI_pid == 0){
        dup2(pipe_write[0], STDIN_FILENO);
        dup2(pipe_read[1], STDOUT_FILENO);
        close(pipe_write[0]);
        close(pipe_write[1]);
        close(pipe_read[0]);
        close(pipe_read[1]);
        execl(reqP->file, reqP->file, NULL);
        fprintf(stderr, "GCI: %s, PID: %d, fail\n", reqP->file, getpid());
        exit(2);
    }
    else {
        close(pipe_write[0]);
        close(pipe_read[1]);
        write(pipe_write[1], reqP->query + 9, strlen(reqP->query) - 8);
        close(pipe_write[1]);
        reqP->pipe_read = pipe_read[0];
    }
    fprintf(stderr, "status of fd: %d change into CGI\n", reqP->conn_fd);
}

void cgi(http_request* reqP){
    char buf[PIPE_BUF];
    int buflen = read(reqP->pipe_read, buf, PIPE_BUF);
    if (buflen > 0) add_to_buf_content(reqP, buf, buflen);
    else if (buflen < 0){
        fprintf(stderr, "status of fd: %d change into ERROR (pipe err)\n", reqP->conn_fd);
        close(reqP->pipe_read);
        reqP->status = ERROR;
    }
    else if (buflen == 0){
        fprintf(stderr, "status of fd: %d change into WAITING (pipe EOF)\n", reqP->conn_fd);
        close(reqP->pipe_read);
        reqP->status = WAITING;
    }
    return;
}

void writing(http_request* reqP){
    // write once only and ignore error
    int nheader = write(reqP->conn_fd, reqP->buf, reqP->buf_len);
    int ncontent = write(reqP->conn_fd, reqP->buf_content, reqP->buf_len_content);
    fprintf(stderr, "status of fd: %d change into NO_USE (writing %d bytes header, %d bytes content)\n", reqP->conn_fd, nheader, ncontent);
    close(reqP->conn_fd);
    free_request(reqP);
    return;
}

void waiting(http_request* reqP){
    if (reqP->CGI_pid != 0) return;
    set_header(reqP, 200, NULL);
    fprintf(stderr, "status of fd: %d change into WRITING (200 OK)\n", reqP->conn_fd);
    reqP->status = WRITING;
    return;
}

void error(http_request* reqP){
    fprintf(stderr, "status of fd: %d change into NO_USE (from ERROR)\n", reqP->conn_fd);
    if (reqP->conn_fd > 2) close(reqP->conn_fd);
    if (reqP->pipe_read > 2) close(reqP->pipe_read);
    free_request(reqP);
    return;
}

// ======================================================================================================
// modify net

int read_header(http_request* reqP) {
    // Request variables
    char* file = (char *) 0;
    char* path = (char *) 0;
    char* query = (char *) 0;
    char* protocol = (char *) 0;
    char* method_str = (char *) 0;
    int r;
    char buf[10000];

    // Read in request from client
    while (1) {
        r = read( reqP->conn_fd, buf, sizeof(buf) );
        if ( r < 0 && ( errno == EINTR || errno == EAGAIN ) ) return 1;
        if ( r <= 0 ) ERR_RET( 1 )
            add_to_buf_head( reqP, buf, r );
        if ( strstr( reqP->buf, "\015\012\015\012" ) != (char*) 0 ||
                strstr( reqP->buf, "\012\012" ) != (char*) 0 ) break;
    }
    // fprintf( stderr, "header: %s\n", reqP->buf );

    // Parse the first line of the request.
    method_str = get_request_line( reqP );
    if ( method_str == (char*) 0 ) ERR_RET( 2 )
        path = strpbrk( method_str, " \t\012\015" );
    if ( path == (char*) 0 ) ERR_RET( 2 )
        *path++ = '\0';
    path += strspn( path, " \t\012\015" );
    protocol = strpbrk( path, " \t\012\015" );
    if ( protocol == (char*) 0 ) ERR_RET( 2 )
        *protocol++ = '\0';
    protocol += strspn( protocol, " \t\012\015" );
    query = strchr( path, '?' );
    if ( query == (char*) 0 )
        query = "";
    else
        *query++ = '\0';
    if ( strcasecmp( method_str, "GET" ) != 0 ) ERR_RET( 3 )
    else {
        strdecode( path, path );
        if ( path[0] != '/' ) ERR_RET( 4 )
        else file = &(path[1]);
    }
    if ( strlen( file ) >= MAXBUFSIZE-1 ) ERR_RET( 4 )
        if ( strlen( query ) >= MAXBUFSIZE-1 ) ERR_RET( 5 )

            strcpy( reqP->file, file );
    strcpy( reqP->query, query );
    return 0;
}

void add_to_buf_head(http_request *reqP, char* str, size_t len) { 
    char** bufP = &(reqP->buf);
    size_t* bufsizeP = &(reqP->buf_size);
    size_t* buflenP = &(reqP->buf_len);

    if ( *bufsizeP == 0 ) {
        *bufsizeP = len + 500;
        *buflenP = 0;
        *bufP = (char*) e_malloc( *bufsizeP );
    } else if ( *buflenP + len >= *bufsizeP ) {
        *bufsizeP = *buflenP + len + 500;
        *bufP = (char*) e_realloc( (void*) *bufP, *bufsizeP );
    }
    (void) memmove( &((*bufP)[*buflenP]), str, len );
    *buflenP += len;
    (*bufP)[*buflenP] = '\0';
}

void add_to_buf_content( http_request *reqP, char* str, size_t len ) { 
    char** bufP = &(reqP->buf_content);
    size_t* bufsizeP = &(reqP->buf_size_content);
    size_t* buflenP = &(reqP->buf_len_content);

    if ( *bufsizeP == 0 ) {
        *bufsizeP = len + 500;
        *buflenP = 0;
        *bufP = (char*) e_malloc( *bufsizeP );
    } else if ( *buflenP + len >= *bufsizeP ) {
        *bufsizeP = *buflenP + len + 500;
        *bufP = (char*) e_realloc( (void*) *bufP, *bufsizeP );
    }
    (void) memmove( &((*bufP)[*buflenP]), str, len );
    *buflenP += len;
    (*bufP)[*buflenP] = '\0';
}

void set_header(http_request* reqP, int code, char *add_mes){
    char timebuf[100];
    int buflen;
    char buf[10000];
    time_t now;

    reqP->buf_len = 0;
    if (code == 200){
        buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 200 OK\015\012Server: SP b05202043\015\012" );
        add_to_buf_head(reqP, buf, buflen);
    }
    if (code == 400){
        buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 400 Bad Request\015\012Server: SP b05202043\015\012" );
        add_to_buf_head(reqP, buf, buflen);
        reqP->buf_len_content = 0;
        char *message = "<html><head></head><body><h1>400 Bad Request</h1><br><br>%s<br>&#9698;&#9606;&#9605;&#9604;&#9603; &#23849;&#9584;(&#12306;&#30399;&#12306;)&#9583;&#28528; &#9603;&#9604;&#9605;&#9606;&#9699;</body></html>";
        buflen = snprintf(buf, sizeof(buf), message, add_mes);
        add_to_buf_content(reqP, buf, buflen);
    }
    if (code == 404){
        buflen = snprintf( buf, sizeof(buf), "HTTP/1.1 404 Not Found\015\012Server: SP b05202043\015\012" );
        add_to_buf_head(reqP, buf, buflen);
        reqP->buf_len_content = 0;
        char *message = "<html><head></head><body> <h1>404 Not Found</h1><br><br>%s<br>&#9698;&#9606;&#9605;&#9604;&#9603; &#23849;&#9584;(&#12306;&#30399;&#12306;)&#9583;&#28528; &#9603;&#9604;&#9605;&#9606;&#9699;</body></html>";
        buflen = snprintf(buf, sizeof(buf), message, add_mes);
        add_to_buf_content(reqP, buf, buflen);
    }
    now = time( (time_t*) 0 );
    (void) strftime( timebuf, sizeof(timebuf), "%a, %d %b %Y %H:%M:%S GMT", gmtime( &now ) );
    buflen = snprintf( buf, sizeof(buf), "Date: %s\015\012", timebuf );
    add_to_buf_head( reqP, buf, buflen );
    buflen = snprintf(buf, sizeof(buf), "Content-Length: %ld\015\012", reqP->buf_len_content);
    add_to_buf_head( reqP, buf, buflen );
    buflen = snprintf( buf, sizeof(buf), "Connection: close\015\012\015\012" );
    add_to_buf_head( reqP, buf, buflen );
}

// ======================================================================================================
// net
// You don't need to know how the following codes are working

static void init_request( http_request* reqP ) {
    reqP->conn_fd = -1;
    reqP->pipe_read = 0;
    reqP->CGI_pid = 0;
    reqP->status = NO_USE;
    reqP->file[0] = (char) 0;
    reqP->query[0] = (char) 0;
    reqP->host[0] = (char) 0;
    reqP->buf = NULL;
    reqP->buf_size = 0;
    reqP->buf_len = 0;
    reqP->buf_content = NULL;
    reqP->buf_size_content = 0;
    reqP->buf_len_content = 0;
    reqP->buf_idx = 0;
}

static void free_request( http_request* reqP ) {
    if (reqP->buf != NULL) {
        free(reqP->buf);
        reqP->buf = NULL;
    }
    if (reqP->buf_content != NULL) {
        free(reqP->buf_content);
        reqP->buf_content = NULL;
    }
    init_request(reqP);
}


static char* get_request_line( http_request *reqP ) { 
    int begin;
    char c;

    char *bufP = reqP->buf;
    int buf_len = reqP->buf_len;

    for ( begin = reqP->buf_idx ; reqP->buf_idx < buf_len; ++reqP->buf_idx ) {
        c = bufP[ reqP->buf_idx ];
        if ( c == '\012' || c == '\015' ) {
            bufP[reqP->buf_idx] = '\0';
            ++reqP->buf_idx;
            if ( c == '\015' && reqP->buf_idx < buf_len && 
                    bufP[reqP->buf_idx] == '\012' ) {
                bufP[reqP->buf_idx] = '\0';
                ++reqP->buf_idx;
            }
            return &(bufP[begin]);
        }
    }
    fprintf( stderr, "http request format error\n" );
    exit(1);
}

static void init_http_server( http_server *svrP, unsigned short port ) {
    struct sockaddr_in servaddr;
    int tmp;

    gethostname( svrP->hostname, sizeof( svrP->hostname) );
    svrP->port = port;

    svrP->listen_fd = socket( AF_INET, SOCK_STREAM, 0 );
    if ( svrP->listen_fd < 0 ) ERR_EXIT( "socket" )

        bzero( &servaddr, sizeof(servaddr) );
    servaddr.sin_family = AF_INET;
    servaddr.sin_addr.s_addr = htonl( INADDR_ANY );
    servaddr.sin_port = htons( port );
    tmp = 1;
    if ( setsockopt( svrP->listen_fd, SOL_SOCKET, SO_REUSEADDR, (void*) &tmp, sizeof(tmp) ) < 0 ) 
        ERR_EXIT ( "setsockopt " )
            if ( bind( svrP->listen_fd, (struct sockaddr *) &servaddr, sizeof(servaddr) ) < 0 ) ERR_EXIT( "bind" )

                if ( listen( svrP->listen_fd, 1024 ) < 0 ) ERR_EXIT( "listen" )
}

// Set NDELAY mode on a socket.
static void set_ndelay( int fd ) {
    int flags, newflags;
    flags = fcntl( fd, F_GETFL, 0 );
    if ( flags != -1 ) {
        newflags = flags | (int) O_NDELAY; // nonblocking mode
        if ( newflags != flags )
            (void) fcntl( fd, F_SETFL, newflags );
    }
}   

static void strdecode( char* to, char* from ) {
    for ( ; *from != '\0'; ++to, ++from ) {
        if ( from[0] == '%' && isxdigit( from[1] ) && isxdigit( from[2] ) ) {
            *to = hexit( from[1] ) * 16 + hexit( from[2] );
            from += 2;
        } else {
            *to = *from;
        }
    }
    *to = '\0';
}

static int hexit( char c ) {
    if ( c >= '0' && c <= '9' )
        return c - '0';
    if ( c >= 'a' && c <= 'f' )
        return c - 'a' + 10;
    if ( c >= 'A' && c <= 'F' )
        return c - 'A' + 10;
    return 0;           // shouldn't happen
}

static void* e_malloc( size_t size ) {
    void* ptr;

    ptr = malloc( size );
    if ( ptr == (void*) 0 ) {
        (void) fprintf( stderr, "out of memory\n" );
        exit( 1 );
    }
    return ptr;
}

static void* e_realloc( void* optr, size_t size ) {
    void* ptr;

    ptr = realloc( optr, size );
    if ( ptr == (void*) 0 ) {
        (void) fprintf( stderr, "out of memory\n" );
        exit( 1 );
    }
    return ptr;
}
