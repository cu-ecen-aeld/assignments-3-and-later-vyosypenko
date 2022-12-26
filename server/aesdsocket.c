
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <string.h>
#include <arpa/inet.h>
#include <syslog.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <syslog.h>
#include <errno.h> 

// ToDO:
// 1. Open a stream socket bound to port 9000, falling and returning -1
// if any connection steps fails
// 2. Listen for and accept connection
// 3. Logs message to the syslog "Accepted connection from xxx"
// 4. Recieve data over the connection  and append to file /var/tmp/aesdsocketdata

#define RX_BUFF_SIZE (30720) // 30Kb

// ToDO: change print to syslog

int conn_fd = 0;
int socket_fd = 0;
char* data_file = "/var/tmp/aesdsocketdata";

void signal_handler(int signo)
{
   printf("Recieved signal with id %d\n", signo);
   if ((signo == SIGINT) || (signo == SIGTERM))
   {
       printf("Caught signal, exiting\n");

       printf("Closing connection ...\n");
       close(conn_fd);

       printf("Closing socket ...\n");
       close(socket_fd);

       printf("Removing %s file...\n", data_file);
       remove(data_file);

       _exit(1);

   }
}

int init_signal_actions(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;

    if(sigaction(SIGTERM, &action, NULL) != 0)
    {
        printf("Error registering for SIGTERM\n");
        return 1;
    }

    if(sigaction(SIGINT, &action, NULL) != 0)
    {
       printf("Error registering for SIGINT\n");
       return 2;
    }

    return 0;
}


int main(int argc, char **argv)
{
    int bytes_read = 0;
    int pfile = 0;
    char *client_ip = NULL;
    char rx_buff[RX_BUFF_SIZE];

    struct addrinfo hints;
    struct addrinfo *res;
    struct sockaddr_in client_addr;

    if (0 != init_signal_actions())
    	return -1;

    openlog("Logs", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Start logging");
    closelog();

    printf("Socket application is started...\n");

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (0 != getaddrinfo(NULL, "9000", &hints, &res))
    {
        printf("ERROR: getaddrinfo function failed\n");
	return -1;
    }

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == socket_fd)
    {
        printf("ERROR: socket function failed\n");
        return -1;
    }
    else
    {
        printf("INFO: socket function run return success, socket descriptor: %d\n", socket_fd);
    }

    int optval = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    if(-1 == bind(socket_fd, res->ai_addr, res->ai_addrlen))
    {
        printf("ERROR: bind function failed\n");
        printf("errno: %d\n", errno);
        return -1;
    }

    if ((argc > 1) && (strcmp(argv[1], "-d") == 0))
    {
        int pid = fork();
        if ( 0 == pid)
            printf("Working in child proccess\n");
        else if (pid > 0)
            exit(-1);
    }

    // ToDo: Free serviceinfo
    if (-1 == listen(socket_fd, 1))
    {
       printf("ERROR: listen function failed\n");
       return -1;
    }

    socklen_t client_addr_size = sizeof(struct sockaddr_in);

    while(1)
    {
        printf("INFO: waitig for connection...\n");
        printf("--------------------------------------------------\n");

        conn_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_addr_size); //location to save client
        if (-1 == conn_fd)
	    {
	        printf("ERROR accept function failed\n");
            return -1;
        }
        client_ip = inet_ntoa(((struct sockaddr_in*)&client_addr)->sin_addr);
        syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);

        printf("Accepted connection from %s\n", client_ip);

        printf("DEBUG: Reading data from socket to file...\n");

        /* Recive data from the socket */
        bytes_read = recv(conn_fd, rx_buff, sizeof(rx_buff), 0);
        printf("DEBUG: bytes read from the socket: %d\n", bytes_read);
        if(bytes_read > 0)
        {
            pfile = open(data_file, O_CREAT|O_WRONLY|O_APPEND, 0662); //check permission
            write(pfile, rx_buff, bytes_read);
            close(pfile);
        }

        printf("DEBUG Reading from socked compleated\n\n");

        pfile = open(data_file, O_RDONLY);

        int buf_size = 0;
        do
        {
            memset(rx_buff, 0, RX_BUFF_SIZE);

            buf_size = (int)read(pfile, rx_buff, bytes_read);
            printf("debug buf_size: %d\n", buf_size);

            write(conn_fd, rx_buff, buf_size);
            //send(conn_fd, rx_buff, buf_size, 0);
        }
        while(buf_size > 0);

    //write(conn_fd, "\n", 1);
    close(pfile);

    printf("Closed connection from %s\n\n", client_ip);
    close(conn_fd);
    }
    return 0;
}
