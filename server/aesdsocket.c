#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <syslog.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define PORT 9000
#define DATA_FILE "/var/tmp/aesdsocketdata"
#define BUF_SIZE 1024

int server_fd = -1;
int data_fd = -1;

void signal_handler(int sig) {
    syslog(LOG_INFO, "Caught signal, exiting");
    if (server_fd != -1) close(server_fd);
    if (data_fd != -1) close(data_fd);
    unlink(DATA_FILE);
    closelog();
    exit(0);
}

int main() {
    int client_fd;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size = sizeof(client_addr);

    openlog("aesdsocket", LOG_PID, LOG_USER);
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    data_fd = open(DATA_FILE, O_RDWR | O_CREAT | O_APPEND, 0644);
    if (data_fd == -1) {
        perror("Could not open/create data file");
        syslog(LOG_ERR, "File open error: %m");
        return -1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        return -1;
    }

    if (listen(server_fd, 10) < 0) {
        perror("Listen failed");
        return -1;
    }

    while (1) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &addr_size);
        if (client_fd == -1) continue;

        syslog(LOG_INFO, "Accepted connection from %s", inet_ntoa(client_addr.sin_addr));

        char buffer[BUF_SIZE];
        ssize_t bytes_recv;

        while ((bytes_recv = recv(client_fd, buffer, BUF_SIZE, 0)) > 0) {
            if (write(data_fd, buffer, bytes_recv) == -1) {
                syslog(LOG_ERR, "Write to file failed");
            }

            if (memchr(buffer, '\n', bytes_recv)) {
                
                lseek(data_fd, 0, SEEK_SET);

                char send_buf[BUF_SIZE];
                ssize_t bytes_read;
                while ((bytes_read = read(data_fd, send_buf, BUF_SIZE)) > 0) {
                    send(client_fd, send_buf, bytes_read, 0);
                }

                lseek(data_fd, 0, SEEK_END);
                break; 
            }
        }

        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", inet_ntoa(client_addr.sin_addr));
    }

    return 0;
}