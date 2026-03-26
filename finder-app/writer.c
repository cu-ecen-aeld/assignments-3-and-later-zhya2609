#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <string.h>
#include <syslog.h>


int main (int argc, char **args){
    printf("The argc is: %d\n",argc);
    if(argc != 3){
        printf("Incorrect arguments");
        openlog("writer.c:", LOG_PID | LOG_CONS, LOG_USER);
        syslog(LOG_ERR, "Incorrect arguments");
        closelog();
        return 1;
    }

    const char *writefile = args[1];
    const char *writestr = args[2];

    openlog("writer.c:", LOG_PID | LOG_CONS, LOG_USER);
    int fd = open(writefile, O_WRONLY | O_CREAT, S_IRWXU | S_IRWXG | S_IRWXO);
    if (fd == -1){
        perror("Failed to create the file");
        printf("Failed to create file %s \n", writestr);
        syslog(LOG_ERR, "Failed to create %s \n %m", writefile);
        closelog();
        return 1;
    }else{
        syslog(LOG_DEBUG,"%s has been created", writefile);

    }

    ssize_t bytes_written = write(fd, writestr, strlen(writestr));
    if (bytes_written == -1){
        perror("Failed to write");
        syslog(LOG_ERR, "Failed to write %s \n %m", writestr);
        closelog();
        return 1;
    }else{
        syslog(LOG_DEBUG,"%s has written into %s", writestr, writefile);

    }
    close(fd);
    closelog();
    return 0;
}