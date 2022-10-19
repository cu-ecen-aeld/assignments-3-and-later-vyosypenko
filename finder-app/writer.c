
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <syslog.h>

int main ( int argc, char **argv )
{
     int fd = 0;
     char* writestr = NULL;
     int writestr_len = 0;

     openlog("Writer", LOG_PID, LOG_USER);

     if (argc < 3)
     {
	 printf("No command line argument specified\n");
         syslog(LOG_DEBUG, "No command line argument specified");
         closelog();
	 return 1;
     }

     fd = open(argv[1], O_CREAT | O_RDWR, S_IRWXU);
     if (fd != -1)
     {
         writestr_len = strlen(argv[2]) + 1;
         writestr = (char*)malloc(sizeof(char) * writestr_len);
         if (writestr == NULL)
         {
             syslog(LOG_ERR, "Can't allocate memory for input string");
         }
         strncpy(writestr, argv[2], writestr_len);
         syslog(LOG_DEBUG, "Writing %s to %s", writestr, argv[1]);
         write(fd, writestr, writestr_len);
         close(fd);
     }
     closelog ();
     return 0;
}

