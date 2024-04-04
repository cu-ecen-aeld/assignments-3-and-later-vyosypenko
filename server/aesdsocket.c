
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
#include <signal.h>


//#include "task_queue.h"
//#include <asm-generic/socket.h>

// ToDo:
// Add conn to task queue

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
static int bytes_read = 0;

static pthread_mutex_t lock;

int conn_fd = 0;
int socket_fd = 0;
char* data_file = "/var/tmp/aesdsocketdata";
char* port_addr = "9000";

static char rfc2822_string[50];
static time_t current_time;
static struct tm * time_info;

//static struct TaskQueue* task_queue;

struct thead_queue_t {
    pthread_t ptid;
    int thead_finished;
    SLIST_ENTRY(thead_queue_t) entries;
};

SLIST_HEAD(thead_queue_head, thead_queue_t) head = SLIST_HEAD_INITIALIZER(head);



void update_data_file(char* buff, int size)
{
    int file_fd = 0;

    if ((file_fd = open(data_file, O_CREAT|O_WRONLY|O_APPEND, 0662)) != -1)
    {
        syslog(LOG_DEBUG, "Data file %s is opened successfully\n", data_file);
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
        close(conn_fd); // check to close all conections, iterate to close all of them

        syslog(LOG_INFO, "Closing socket\n");
        close(socket_fd);

        syslog(LOG_INFO, "Removing %s file...\n", data_file);
        remove(data_file);

        syslog(LOG_INFO, "Destroy the mutex\n");
        pthread_mutex_destroy(&lock);

        syslog(LOG_INFO, "Clean up task queue\n");
        
        //struct ConnWorkItem *iter, *prev_node = NULL;
        
        // SLIST_FOREACH(iter, &task_list, entries) 
        // {
        //     if (prev_node == NULL) {
        //         SLIST_REMOVE_HEAD(&task_list, entries);
        //     } else {
        //         SLIST_REMOVE(&task_list, prev_node, ConnWorkItem, entries);
        //     }
        //     free(iter);
        //     prev_node = iter;
        // }
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



    client_ip = inet_ntoa(((struct sockaddr_in*)&client_addr)->sin_addr);
    syslog(LOG_INFO, "Accepted connection from %s\n", client_ip);
    
    bytes_read = recv(conn, rx_buff, sizeof(rx_buff), 0);
    syslog(LOG_INFO, "Read  %d bytes read from the socket: \n", bytes_read);

    if(bytes_read > 0)
    {
        syslog(LOG_DEBUG, "%s", rx_buff);
        printf("conn: %d rx_buff: %s\n", conn, rx_buff);



        pthread_mutex_lock(&lock);

        // write back to socket 
        write(conn, rx_buff, bytes_read);

        update_data_file(rx_buff, bytes_read);
        pthread_mutex_unlock(&lock);
    }
    syslog(LOG_INFO, "Reading from socked compleated\n");
    printf("DEBUG: Reading from socked compleated\n");


    pthread_t tid = pthread_self();
    syslog(LOG_DEBUG,"tid: %ld \n", tid);
    printf("DEBUG tid: %ld \n", tid);

    struct thead_queue_t *iter = NULL;
    SLIST_FOREACH(iter, &head, entries)
    {
        if (iter->ptid == tid) {
            syslog(LOG_DEBUG, "Taks with tid: %ld finished\n", tid);
            iter->thead_finished = 1; // true;
            break; // tid found no reason to iterate
        }
    }

    close(conn);
    syslog(LOG_INFO, "Closed connection from %s\n", client_ip);

    pthread_exit(NULL); 
}


#define errExit(msg)    do { perror(msg); exit(EXIT_FAILURE); \
                               } while (0)




static void timer_handler(int sig, siginfo_t *si, void *uc)
{
    //UNUSED(sig);
    //UNUSED(uc);

    time(&current_time);
    time_info = localtime(&current_time);

    strftime(rfc2822_string, sizeof(rfc2822_string), "%a, %d %b %Y %H:%M:%S %z\n", time_info);

    //printf("%s", rfc2822_string);
    syslog(LOG_INFO, "%s", rfc2822_string);

    pthread_mutex_lock(&lock);
    update_data_file(rfc2822_string, strlen(rfc2822_string));
    pthread_mutex_unlock(&lock);
}


void timer_init()
{
    timer_t            timerid = 0;
    struct itimerspec  its;
    struct sigevent    sev = { 0 };
    struct sigaction   sa = { 0 };

    /* Create the timer. */
    sev.sigev_notify = SIGEV_SIGNAL;
    sev.sigev_signo = SIGRTMIN;
    sev.sigev_value.sival_ptr = &timerid;
    if (timer_create(CLOCK_REALTIME, &sev, &timerid) == -1)
        errExit("timer_create");
    
    /* Start the timer. */
    its.it_value.tv_sec = 10;
    its.it_value.tv_nsec = 0;
    its.it_interval.tv_sec = its.it_value.tv_sec;
    its.it_interval.tv_nsec = its.it_value.tv_nsec;

    if (timer_settime(timerid, 0, &its, NULL) == -1)
        errExit("timer_settime");

    /* Establish handler for timer signal. */
    printf("Establishing handler for signal %d\n", SIGRTMIN);
    sa.sa_flags = SA_SIGINFO;
    sa.sa_sigaction = timer_handler; 
    sigemptyset(&sa.sa_mask);

    if (sigaction(SIGRTMIN, &sa, NULL) == -1)
        errExit("sigaction");

}

void task_queue_append(pthread_t ptid, int thead_finished)
{
    struct thead_queue_t *new_element = malloc(sizeof(struct thead_queue_t)); 
    if (new_element == NULL)
    {
        syslog(LOG_ERR, "Memory allocation failed for new element\n");
    }

    new_element->ptid = ptid;
    new_element->thead_finished = thead_finished;

    // Add the new element to the end of the list
    SLIST_INSERT_HEAD(&head, new_element, entries); 
}

void task_queue_remove_and_free_element(struct thead_queue_t *element) 
{
    if (element != NULL) {
        SLIST_REMOVE(&head, element, thead_queue_t, entries);
        free(element);
    }
}


int main(int argc, char **argv)
{
    struct addrinfo hints;
    struct addrinfo *res;
    
    pthread_t ptid;

    //SLIST_INIT(&head);


    //timer_init(); // DEBUG, temporary disabled

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

    struct thead_queue_t *iter = NULL;
    // task_queue_append(10, 2);
    // task_queue_append(10, 4);
    // task_queue_append(10, 8);
    // task_queue_append(10, 12);

    // SLIST_FOREACH(iter, &head, entries)
    // {
    //         //iter->thead_finished;
    //     printf("thead_finished: %d\r\n", iter->thead_finished);  

    //     printf("remove and free element\n");
    //     task_queue_remove_and_free_element(iter);
    // }

    // task_queue_append(10, 11);
    // task_queue_append(10, 23);

    // task_queue_append(10, 90);

    // SLIST_FOREACH(iter, &head, entries)
    // {
    //         //iter->thead_finished;
    //     printf("thead_finished: %d\r\n", iter->thead_finished);  

    //     printf("remove and free element\n");
    //     task_queue_remove_and_free_element(iter);
    // }



    // while(1)
    // {
    //     //task_queue_append(10, 2);
    //     //task_queue_append(10, 20);
    //     //task_queue_append(10, 30);

        
       
    //     printf("sleep\n");
    //     usleep(2*1000000);
    // }


    while(1)
    {      
        /* Accept connection */
        printf("DEBUG: Waiting for new connection...\n");
        if((conn_fd = accept(socket_fd, (struct sockaddr*)&client_addr, &client_addr_size)) != -1)
        {
            syslog(LOG_INFO, "The new connection wid ID: %d is accepted\n", conn_fd);
        
        
            pthread_create(&ptid, NULL, &connection_hadler, &conn_fd);
            //pthread_join(ptid, NULL);  //debug

            // process_connection(new_fd);

            /* add new task  to task list */
            syslog(LOG_DEBUG, "Add task with ID: %ld to the task queue\n", ptid);
            printf("DEBUG: Add task with ID: %ld to the task queue\n", ptid);
            //enqueue(task_queue, ptid, false);

            //displayQueue(task_queue);
            task_queue_append(ptid, 0);        

            /* Remove finished task from the task queue */
            //struct ConnWorkItem *iter, *prev_node = NULL;

            

            /* */
            // while (!isEmpty(task_queue))
            // {
            //     //pthread_join(iter->ptid, NULL); 
            //     //free(iter);
            // } 

            
            //task_queue_append(1, 20);
            //task_queue_append(4, 200);



            


            // struct thead_queue_t *iter = NULL;
            // printf("thead_finished: %d\r\n", iter->thead_finished);  
            // SLIST_FOREACH(iter, &head, entries)
            // {
            //     //iter->thead_finished;
            //     printf("thead_finished: %d\r\n", iter->thead_finished);  
            // }
            // {
                
            //     if (iter->work_compleated)
            //     {
            //         if (prev_node == NULL) {
            //             SLIST_REMOVE_HEAD(&task_list, entries);
            //         } else {
            //             SLIST_REMOVE(&task_list, prev_node, ConnWorkItem, entries);
            //         }
            //         syslog(LOG_INFO, "Remove task from queue with ID: %ld\n\n", iter->ptid);
            //         pthread_join(iter->ptid, NULL); 
            //         free(iter);
            //     }
            //     prev_node = iter;
            // }
        }  

        /* Iterate over the list */
        syslog(LOG_DEBUG, "Iteration over task_list\n");
        iter = NULL;
        SLIST_FOREACH(iter, &head, entries)
        {
            if (iter->thead_finished)
            {
                printf("join thread with tid: %ld\n", iter->ptid); 
                pthread_join(iter->ptid, NULL);

                printf("remove and free element\n"); 
                task_queue_remove_and_free_element(iter);
            }
            
            //iter->thead_finished;
            //printf("thead_finished: %d\r\n", iter->thead_finished);  

            
            //task_queue_remove_and_free_element(iter);
        }
    }
    return 0;
}
