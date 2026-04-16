#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <syslog.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <signal.h>
#include <fcntl.h>
#include <stdbool.h>
#include <errno.h>

#define PORT 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

volatile sig_atomic_t caught_signal = 0;

void signal_handler(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) {
        caught_signal = 1;
    }
}

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    struct sigaction new_action;
    int server_fd, client_fd, file_fd, dev_null_fd;
    int opt = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];
    pid_t pid;
    
    size_t total_received, buffer_capacity;
    char *rx_buffer, *new_buffer;
    ssize_t bytes_received, bytes_read;
    char tx_buffer[BUFFER_SIZE];

    // Correctly check argv[1] instead of the whole argv array
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    }

    openlog("aesdsocket", 0, LOG_USER);

    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;
    sigemptyset(&new_action.sa_mask); 

    if (sigaction(SIGINT, &new_action, NULL) != 0) {
        syslog(LOG_ERR, "Error %d registering for SIGINT", errno);
        return -1;
    }
    if (sigaction(SIGTERM, &new_action, NULL) != 0) {
        syslog(LOG_ERR, "Error %d registering for SIGTERM", errno);
        return -1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) return -1;

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) return -1;

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) return -1;

    // Manual Daemonization
    if (daemon_mode) {
        pid = fork();
        if (pid == -1) {
            syslog(LOG_ERR, "Failed to fork");
            return -1;
        } else if (pid != 0) {
            exit(EXIT_SUCCESS); // Parent exits
        }

        if (setsid() == -1) return -1;

        if (chdir("/") == -1) return -1;

        dev_null_fd = open("/dev/null", O_RDWR);
        if (dev_null_fd != -1) {
            dup2(dev_null_fd, STDIN_FILENO);
            dup2(dev_null_fd, STDOUT_FILENO);
            dup2(dev_null_fd, STDERR_FILENO);
            close(dev_null_fd);
        }
    }

    if (listen(server_fd, 10) == -1) return -1;

    while (!caught_signal) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_fd == -1) {
            if (caught_signal) break; 
            continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        total_received = 0;
        buffer_capacity = BUFFER_SIZE;
        rx_buffer = malloc(buffer_capacity);
        
        if (!rx_buffer) {
            close(client_fd);
            continue;
        }

        while (1) {
            bytes_received = recv(client_fd, rx_buffer + total_received, buffer_capacity - total_received, 0);
            if (bytes_received <= 0) break;
            
            total_received += bytes_received;

            if (memchr(rx_buffer + total_received - bytes_received, '\n', bytes_received) != NULL) {
                break; 
            }

            if (total_received >= buffer_capacity) {
                buffer_capacity += BUFFER_SIZE;
                new_buffer = realloc(rx_buffer, buffer_capacity);
                if (!new_buffer) {
                    free(rx_buffer);
                    rx_buffer = NULL;
                    break;
                }
                rx_buffer = new_buffer;
            }
        }
        
        if (rx_buffer) {
            file_fd = open(FILE_PATH, O_CREAT | O_APPEND | O_RDWR, 0644);
            if (file_fd != -1) {
                if (write(file_fd, rx_buffer, total_received) != -1) {
                    lseek(file_fd, 0, SEEK_SET);
                    while ((bytes_read = read(file_fd, tx_buffer, BUFFER_SIZE)) > 0) {
                        send(client_fd, tx_buffer, bytes_read, 0);
                    }
                }
                close(file_fd);
            }
            free(rx_buffer);
        }

        close(client_fd);
        syslog(LOG_INFO, "Closed connection from %s", client_ip);
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    remove(FILE_PATH);
    close(server_fd);
    closelog();

    return 0;
}