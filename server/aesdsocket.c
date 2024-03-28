
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

#define RX_BUFF_SIZE (30720) // 30Kb

// ToDO: change print to syslog

static struct sockaddr_in client_addr;
static char *client_ip = NULL;
static char rx_buff[RX_BUFF_SIZE];
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

//struct ListHead task_list;

// Define the queue head
//LIST_HEAD(SocketWorkList, SocketWorkItem) socket_work_list = LIST_HEAD_INITIALIZER(&socket_work_list);


void update_data_file(char* buff, int size)
{
    int file_fd = 0;

    //pthread_mutex_lock(&lock);
    if ((file_fd = open(data_file, O_CREAT|O_WRONLY|O_APPEND, 0662)) != -1)
    {
        write(file_fd, buff, size);
        close(file_fd);
    }
    else
    {
        //perror("Error opening the file %s\n", data_file);
        syslog(LOG_ERR, "Error opening the file %s\n", data_file);
    }

    
    //pthread_mutex_unlock(&lock);
}

void signal_handler(int signo)
{
   printf("Recieved signal with id %d\n", signo);
   syslog(LOG_INFO, "Recieved signal with id %d\n", signo);
   if ((signo == SIGINT) || (signo == SIGTERM))
   {
        printf("Caught signal, exiting\n");
        syslog(LOG_INFO, "Caught signal, exiting\n");

        printf("Closing connection ...\n");
        syslog(LOG_INFO, "Closing connection\n");
        close(conn_fd);

        printf("Closing socket ...\n");
        syslog(LOG_INFO, "Closing socket\n");
        close(socket_fd);

        printf("Removing %s file...\n", data_file);
        syslog(LOG_INFO, "Removing %s file...\n", data_file);
        remove(data_file);

        syslog(LOG_INFO, "Clean up task queue");
        
        struct ConnWorkItem *iter, *prev_node = NULL;
        
        SLIST_FOREACH(iter, &task_list, entries) 
        {
            if (prev_node == NULL) {
                SLIST_REMOVE_HEAD(&task_list, entries);
            } else {
                SLIST_REMOVE(&task_list, prev_node, ConnWorkItem, entries);
            }
                
            pthread_join(iter->ptid, NULL); 
            free(iter);
            prev_node = iter;
        }

       //_exit(1);
   }

   if (signo == SIGINT)
       exit(SIGINT);
   if (signo == SIGINT) 
       exit(SIGTERM);
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

void* connection_hadler(void* arg)
{
    int conn = *(int*)arg;
    //int data_len = 0;
    int buf_size = 0;

    pthread_t tid = pthread_self();

    
    client_ip = inet_ntoa(((struct sockaddr_in*)&client_addr)->sin_addr);

    syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);

    printf("connection_hadler: Accepted connection from %s\n", client_ip);
    

    /* Recive data from the socket */

    
    //ioctl(conn, FIONREAD, &data_len);
    //printf("Amount of data %d!!!!!!\n", data_len);

    //work_compleated

    
    // while ((data_read = recv(socket_fd, &data, 1, 0)) > 0 && data != '\n')
    //     *message++ = data;
      
    // if (data_read == -1) {
    //     perror("CLIENT: ERROR recv()");
    //     exit(EXIT_FAILURE);
    // }

    //while bytes_read = recv(conn_fd, rx_buff, sizeof(rx_buff), 0)

    bytes_read = recv(conn_fd, rx_buff, sizeof(rx_buff), 0);
    printf("connection_hadler: bytes read from the socket: %d\n", bytes_read);
    printf("connection_hadler: rx_buff: ");
    if(bytes_read > 0)
    {
        for (int i=0; i<bytes_read; i++)
            printf("%c", rx_buff[i]);
        write(conn_fd, rx_buff, buf_size);
    }
    printf("connection_hadler: Reading from socked compleated\n\n");

    //pthread_mutex_lock(&lock);

    //write_to_file

    //pthread_mutex_unlock(&lock);


    struct ConnWorkItem *work_item;
    //SLIST_FOREACH(work_item,  &task_list, entries)  {
    SLIST_FOREACH(work_item,  &task_list, entries)  {
        if (work_item->ptid == tid) {
            // Found the element with ptid equal to 100
            // Perform operations on the found element
            printf("connection_hadler: tid %ld\n", tid);
            work_item->work_compleated = 1;
        }
    }

    close(conn);
    printf("connection_hadler:Closed connection from %s\n\n", client_ip);

    pthread_exit(NULL); 
}




int main(int argc, char **argv)
{
    struct addrinfo hints;
    struct addrinfo *res;
    
    pthread_t ptid;

    //struct ListHead task_list;
    SLIST_INIT(&task_list); // Initialize the list

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

    if (0 != getaddrinfo(NULL, port_addr, &hints, &res))
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
    if (-1 == listen(socket_fd, SOMAXCONN))
    {
       printf("ERROR: listen function failed\n");
       return -1;
    }

    socklen_t client_addr_size = sizeof(struct sockaddr_in);

    if (pthread_mutex_init(&lock, NULL) != 0)
    {
        printf("\nERROR: mutex init failed\n");
        return 1;
    }

    //struct ConnWorkItem *iter;

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

        pthread_create(&ptid, NULL, &connection_hadler, &conn_fd);

        // process_connection(new_fd);


        struct ConnWorkItem *node;
        // ToDo: Not forget to free
        node = (struct ConnWorkItem*)malloc(sizeof(struct ConnWorkItem)); // Allocate memory for the new node
        node->ptid = ptid;
        node->work_compleated = 0;

        SLIST_INSERT_HEAD(&task_list, node, entries); // Insert the node at the head of the list

        // Add ptid to list
        // put item->work_compleated = False
        
        //pthread_mutex_destroy(&lock);

        
        //struct ConnWorkItem *iter;
        // // // Iterate over the list

        struct ConnWorkItem *iter, *prev_node = NULL;

        SLIST_FOREACH(iter, &task_list, entries) 
        {
            if (iter->work_compleated)
            {
                if (prev_node == NULL) {
                    SLIST_REMOVE_HEAD(&task_list, entries);
                } else {
                    SLIST_REMOVE(&task_list, prev_node, ConnWorkItem, entries);
                }
                pthread_join(iter->ptid, NULL); 
                free(iter);
            }
            prev_node = iter;
        }

        printf("\n*************** HERE !!!!!!!!!!!!!\n");

    }
    return 0;
}
