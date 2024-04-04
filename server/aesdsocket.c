
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
#include <pthread.h>
#include <sys/ioctl.h>      
#include <sys/queue.h>
#include <stdbool.h>


// ToDO:
// 1. Open a stream socket bound to port 9000, falling and returning -1
// if any connection steps fails
// 2. Listen for and accept connection
// 3. Logs message to the syslog "Accepted connection from xxx"
// 4. Recieve data over the connection  and append to file /var/tmp/aesdsocketdata

/*
Continuation of Assignment 5:

Modify your socket based program to accept multiple simultaneous connections, with each connection spawning a new thread to handle the connection.

a. Writes to /var/tmp/aesdsocketdata should be synchronized between threads using a mutex, to ensure data written by synchronous connections is not intermixed,
     and not relying on any file system synchronization.
    i. For instance, if one connection writes “12345678” and another connection writes “abcdefg” it should not be possible for 
    the resulting /var/tmp/aesdsocketdata file to contain a mix like “123abcdefg456”, 
    the content should always be “12345678”, followed by “abcdefg”.  However for any simultaneous connections, 
    it's acceptable to allow packet writes to occur in any order in the socketdata file.
b. The thread should exit when the connection is closed by the client or when an error occurs in the send or receive steps.
c. Your program should continue to gracefully exit when SIGTERM/SIGINT is received, after requesting an exit from each thread and waiting for threads to complete execution.
d. Use the singly linked list APIs discussed in the video (or your own implementation if you prefer) to manage threads.
    i. Use pthread_join() to join completed threads, do not use detached threads for this assignment.
*/

#define RX_BUFF_SIZE (128 * 1024)


static struct sockaddr_in client_addr;
static char *client_ip = NULL;
//static char rx_buff[RX_BUFF_SIZE];
static int bytes_read = 0;

static pthread_mutex_t lock;

int conn_fd = 0;
int socket_fd = 0;
char* data_file = "/var/tmp/aesdsocketdata";
char* port_addr = "9000";


// Define the structure for the list elements
struct ConnWorkItem {
    int work_compleated;
    pthread_t ptid;
    SLIST_ENTRY(ConnWorkItem) entries; // Define the list entry
};
SLIST_HEAD(ListHead, ConnWorkItem) task_list = SLIST_HEAD_INITIALIZER(task_list); // Define the list head




void update_data_file(char* buff, int size)
{
    int file_fd = 0;

    if ((file_fd = open(data_file, O_CREAT|O_WRONLY|O_APPEND, 0662)) != -1)
    {
        write(file_fd, buff, size);
        close(file_fd);
    }
    else
    {
        syslog(LOG_ERR, "Error opening the file %s\n", data_file);
    }
}

void signal_handler(int signo)
{
   syslog(LOG_INFO, "Recieved signal with id %d\n", signo);
   if ((signo == SIGINT) || (signo == SIGTERM))
   {
        syslog(LOG_INFO, "Caught signal, exiting\n");

        syslog(LOG_INFO, "Closing connection\n");
        close(conn_fd);

        syslog(LOG_INFO, "Closing socket\n");
        close(socket_fd);

        syslog(LOG_INFO, "Removing %s file...\n", data_file);
        remove(data_file);

        syslog(LOG_INFO, "Destroy the mutex\n");
        pthread_mutex_destroy(&lock);

        syslog(LOG_INFO, "Clean up task queue\n");
        
        struct ConnWorkItem *iter, *prev_node = NULL;
        
        SLIST_FOREACH(iter, &task_list, entries) 
        {
            if (prev_node == NULL) {
                SLIST_REMOVE_HEAD(&task_list, entries);
            } else {
                SLIST_REMOVE(&task_list, prev_node, ConnWorkItem, entries);
            }
            free(iter);
            prev_node = iter;
        }
   }

//    if (signo == SIGINT)
//        exit(SIGINT);
//    if (signo == SIGINT) 
//        exit(SIGTERM);
    _exit(-1);
}

int init_signal_actions(void)
{
    struct sigaction action;
    memset(&action, 0, sizeof(struct sigaction));
    action.sa_handler = signal_handler;

    if(sigaction(SIGTERM, &action, NULL) != 0)
    {
        syslog(LOG_PERROR, "Error registering for SIGTERM\n");
        return 1;
    }

    if(sigaction(SIGINT, &action, NULL) != 0)
    {
       syslog(LOG_PERROR, "Error registering for SIGINT\n");
       return 2;
    }

    return 0;
}

void* connection_hadler(void* arg)
{
    int conn = *(int*)arg;
    static char rx_buff[RX_BUFF_SIZE];
    //int buf_size = 0;

    pthread_t tid = pthread_self();

    client_ip = inet_ntoa(((struct sockaddr_in*)&client_addr)->sin_addr);
    syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);
    
    bytes_read = recv(conn_fd, rx_buff, sizeof(rx_buff), 0);
    syslog(LOG_INFO, "Read  %d bytes read from the socket: \n", bytes_read);

    if(bytes_read > 0)
    {
        for (int i=0; i<bytes_read; i++)
            syslog(LOG_INFO, "%c", rx_buff[i]);
        //write(conn_fd, rx_buff, buf_size);

        pthread_mutex_lock(&lock);
        update_data_file(rx_buff, bytes_read);
        pthread_mutex_unlock(&lock);
    }
    syslog(LOG_INFO, "Reading from socked compleated\n");


    close(conn);
    syslog(LOG_INFO, "Closed connection from %s\n", client_ip);

    struct ConnWorkItem *work_item;
    SLIST_FOREACH(work_item,  &task_list, entries)  {
        if (work_item->ptid == tid) {
            syslog(LOG_DEBUG, "Taks with tid: %ld finished\n", tid);
            work_item->work_compleated = true;
        }
    }

    pthread_exit(NULL); 
}


int main(int argc, char **argv)
{
    struct addrinfo hints;
    struct addrinfo *res;
    
    pthread_t ptid;

    SLIST_INIT(&task_list);

    if (0 != init_signal_actions())
    	exit(EXIT_FAILURE);

    openlog("Logs", LOG_PID, LOG_USER);
    syslog(LOG_INFO, "Start logging");
    closelog();

    syslog(LOG_ERR, "Socket application is started\n");

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = PF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if (0 != getaddrinfo(NULL, port_addr, &hints, &res))
    {
        syslog(LOG_ERR, "Listen function failed\n");
	    exit(EXIT_FAILURE);
    }

    socket_fd = socket(PF_INET, SOCK_STREAM, 0);
    if (-1 == socket_fd)
    {
        syslog(LOG_ERR, "socket function failed\n");
        exit(EXIT_FAILURE);
    }


    int optval = 1;
    setsockopt(socket_fd, SOL_SOCKET, SO_REUSEPORT, &optval, sizeof(optval));

    if(-1 == bind(socket_fd, res->ai_addr, res->ai_addrlen))
    {
        syslog(LOG_ERR, "bind function failed\n");
        exit(EXIT_FAILURE);
    }

    if ((argc > 1) && (strcmp(argv[1], "-d") == 0))
    {
        int pid = fork();
        if ( 0 == pid) {
            printf("Working in child proccess\n");
            syslog(LOG_INFO, "Working in child proccess\n");
        }
        else if (pid > 0)
            exit(EXIT_FAILURE);
    }

    // ToDo: Free serviceinfo
    if (-1 == listen(socket_fd, SOMAXCONN))
    {
       syslog(LOG_ERR, "Listen function failed\n");
       exit(EXIT_FAILURE);
    }

    socklen_t client_addr_size = sizeof(struct sockaddr_in);

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        syslog(LOG_ERR, "Failed to initialize mutex\n");
        exit(EXIT_FAILURE);
    }

    while(1)
    {      
        if ((conn_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_addr_size)) != -1)
        {
            syslog(LOG_INFO, "The new connection wid ID: %d is accepted\n", conn_fd);
        }
        else
	    {
            syslog(LOG_ERR, "accept function failed\n");
            exit(EXIT_FAILURE);
        }

        pthread_create(&ptid, NULL, &connection_hadler, &conn_fd);

        // process_connection(new_fd);


        struct ConnWorkItem *node;

        /* Allocate memory for the new node */
        node = (struct ConnWorkItem*)malloc(sizeof(struct ConnWorkItem)); 
        node->ptid = ptid;
        node->work_compleated = false;

        syslog(LOG_DEBUG, "Add task with ID: %ld to the task queue\n", node->ptid);
        SLIST_INSERT_HEAD(&task_list, node, entries); 
        
        /* Remove finished task from the task queue */
        struct ConnWorkItem *iter, *prev_node = NULL;

        syslog(LOG_DEBUG, "Iteration over task_list\n");
        SLIST_FOREACH(iter, &task_list, entries) 
        {
            
            if (iter->work_compleated)
            {
                if (prev_node == NULL) {
                    SLIST_REMOVE_HEAD(&task_list, entries);
                } else {
                    SLIST_REMOVE(&task_list, prev_node, ConnWorkItem, entries);
                }
                syslog(LOG_INFO, "Remove task from queue with ID: %ld\n\n", iter->ptid);
                pthread_join(iter->ptid, NULL); 
                free(iter);
            }
            prev_node = iter;
        }
    }
    return 0;
}
