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
#include <pthread.h>
#include <sys/queue.h>
#include <time.h>

#define PORT 9000
#define FILE_PATH "/var/tmp/aesdsocketdata"
#define BUFFER_SIZE 1024

volatile sig_atomic_t caught_signal = 0;
pthread_mutex_t aesd_file_mutex = PTHREAD_MUTEX_INITIALIZER;

typedef struct slist_data_s {
    pthread_t thread;
    int client_fd;
    char client_ip[INET_ADDRSTRLEN];
    bool thread_complete_flag;
    SLIST_ENTRY(slist_data_s) entries;
} slist_data_t;

void signal_handler(int signal_number) {
    if (signal_number == SIGINT || signal_number == SIGTERM) {
        caught_signal = 1;
    }
}

void *handle_connection(void *thread_param) {
    slist_data_t *thread_data = (slist_data_t *)thread_param;
    int client_fd = thread_data->client_fd;
    
    size_t total_received = 0;
    size_t buffer_capacity = BUFFER_SIZE;
    char *rx_buffer = malloc(buffer_capacity);
    char *new_buffer;
    ssize_t bytes_received, bytes_read;
    char tx_buffer[BUFFER_SIZE];
    
    if (!rx_buffer) {
        close(client_fd);
        thread_data->thread_complete_flag = true;
        return NULL;
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
        pthread_mutex_lock(&aesd_file_mutex);
        int file_fd = open(FILE_PATH, O_CREAT | O_APPEND | O_RDWR, 0644);
        if (file_fd != -1) {
            if (write(file_fd, rx_buffer, total_received) != -1) {
                lseek(file_fd, 0, SEEK_SET);
                while ((bytes_read = read(file_fd, tx_buffer, BUFFER_SIZE)) > 0) {
                    send(client_fd, tx_buffer, bytes_read, 0);
                }
            }
            close(file_fd);
        }
        pthread_mutex_unlock(&aesd_file_mutex);
        free(rx_buffer);
    }

    close(client_fd);
    syslog(LOG_INFO, "Closed connection from %s", thread_data->client_ip);
    thread_data->thread_complete_flag = true;
    return NULL;
}

void *timestamp_thread_func(void *arg) {
    while (!caught_signal) {
        int ms_waited = 0;
        while (!caught_signal && ms_waited < 10000) {
            struct timeval tv;
            tv.tv_sec = 0;
            tv.tv_usec = 100000;
            select(0, NULL, NULL, NULL, &tv);
            ms_waited += 100;
        }
        
        if (caught_signal) break;
        
        time_t t = time(NULL);
        struct tm *tmp = localtime(&t);
        if (tmp == NULL) continue;
        
        char outstr[200];
        if (strftime(outstr, sizeof(outstr), "timestamp:%a, %d %b %Y %T %z\n", tmp) == 0) {
            continue;
        }
        
        pthread_mutex_lock(&aesd_file_mutex);
        int file_fd = open(FILE_PATH, O_CREAT | O_APPEND | O_RDWR, 0644);
        if (file_fd != -1) {
            write(file_fd, outstr, strlen(outstr));
            close(file_fd);
        }
        pthread_mutex_unlock(&aesd_file_mutex);
    }
    return NULL;
}

int main(int argc, char *argv[]) {
    bool daemon_mode = false;
    struct sigaction new_action;
    int server_fd, client_fd, dev_null_fd;
    int opt = 1;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_size = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];
    pid_t pid;
    
    if (argc == 2 && strcmp(argv[1], "-d") == 0) {
        daemon_mode = true;
    }

    openlog("aesdsocket", 0, LOG_USER);

    memset(&new_action, 0, sizeof(struct sigaction));
    new_action.sa_handler = signal_handler;
    sigemptyset(&new_action.sa_mask); 

    if (sigaction(SIGINT, &new_action, NULL) != 0) {
        syslog(LOG_ERR, "Error %d registering for SIGINT", errno);
        perror("sigaction SIGINT");
        return -1;
    }
    if (sigaction(SIGTERM, &new_action, NULL) != 0) {
        syslog(LOG_ERR, "Error %d registering for SIGTERM", errno);
        perror("sigaction SIGTERM");
        return -1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (server_fd == -1) { perror("socket"); return -1; }

    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) { perror("setsockopt"); return -1; }

    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_fd, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) { perror("bind"); return -1; }

    if (daemon_mode) {
        pid = fork();
        if (pid == -1) {
            syslog(LOG_ERR, "Failed to fork");
            perror("fork");
            return -1;
        } else if (pid != 0) {
            exit(EXIT_SUCCESS); 
        }
        if (setsid() == -1) { perror("setsid"); return -1; }
        if (chdir("/") == -1) { perror("chdir"); return -1; }
        dev_null_fd = open("/dev/null", O_RDWR);
        if (dev_null_fd != -1) {
            dup2(dev_null_fd, STDIN_FILENO);
            dup2(dev_null_fd, STDOUT_FILENO);
            dup2(dev_null_fd, STDERR_FILENO);
            close(dev_null_fd);
        }
    }

    if (listen(server_fd, 10) == -1) { perror("listen"); return -1; }

    SLIST_HEAD(slisthead, slist_data_s) head;
    SLIST_INIT(&head);
    
    pthread_t timestamp_thread;
    if (pthread_create(&timestamp_thread, NULL, timestamp_thread_func, NULL) != 0) {
        syslog(LOG_ERR, "Failed to create timestamp thread");
    }

    while (!caught_signal) {
        client_fd = accept(server_fd, (struct sockaddr *)&client_addr, &client_addr_size);
        if (client_fd == -1) {
            if (caught_signal) break; 
            continue;
        }

        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, INET_ADDRSTRLEN);
        syslog(LOG_INFO, "Accepted connection from %s", client_ip);

        slist_data_t *new_node = malloc(sizeof(slist_data_t));
        if (new_node) {
            new_node->client_fd = client_fd;
            strcpy(new_node->client_ip, client_ip);
            new_node->thread_complete_flag = false;
            
            if (pthread_create(&new_node->thread, NULL, handle_connection, new_node) == 0) {
                SLIST_INSERT_HEAD(&head, new_node, entries);
            } else {
                syslog(LOG_ERR, "Failed to create connection thread");
                close(client_fd);
                free(new_node);
            }
        } else {
            close(client_fd);
        }

        slist_data_t *elem = SLIST_FIRST(&head);
        while (elem != NULL) {
            slist_data_t *next = SLIST_NEXT(elem, entries);
            if (elem->thread_complete_flag) {
                pthread_join(elem->thread, NULL);
                SLIST_REMOVE(&head, elem, slist_data_s, entries);
                free(elem);
            }
            elem = next;
        }
    }

    syslog(LOG_INFO, "Caught signal, exiting");
    
    pthread_join(timestamp_thread, NULL);
    
    slist_data_t *elem;
    while (!SLIST_EMPTY(&head)) {
        elem = SLIST_FIRST(&head);
        pthread_cancel(elem->thread); 
        pthread_join(elem->thread, NULL);
        SLIST_REMOVE_HEAD(&head, entries);
        free(elem);
    }
    
    remove(FILE_PATH);
    close(server_fd);
    closelog();
    pthread_mutex_destroy(&aesd_file_mutex);

    return 0;
}