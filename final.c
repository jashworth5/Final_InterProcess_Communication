#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <errno.h>

#define BUFFER_SIZE 1024
#define READ_END 0
#define WRITE_END 1
#define MAX_USERS 10

typedef struct {
    int read_fd;
    int* write_fds;  // Array of write file descriptors
    char* username;
    int num_users;
} ChatClient;

void handle_error(const char* message) {
    fprintf(stderr, "Error: %s - %s\n", message, strerror(errno));
}

ChatClient* create_client(const char* username, int num_users) {
    ChatClient* client = malloc(sizeof(ChatClient));
    if (!client) return NULL;
    
    client->username = strdup(username);
    client->write_fds = malloc(num_users * sizeof(int));
    client->num_users = num_users;
    
    if (!client->username || !client->write_fds) {
        free(client->username);
        free(client->write_fds);
        free(client);
        return NULL;
    }
    
    return client;
}

void destroy_client(ChatClient* client) {
    if (client) {
        free(client->username);
        free(client->write_fds);
        free(client);
    }
}

int broadcast_message(ChatClient* client, const char* message) {
    char formatted_msg[BUFFER_SIZE];
    int written = snprintf(formatted_msg, BUFFER_SIZE, "%s: %s", client->username, message);
    if (written < 0 || written >= BUFFER_SIZE) return -1;

    for (int i = 0; i < client->num_users; i++) {
        if (write(client->write_fds[i], formatted_msg, written + 1) <= 0) {
            return -1;
        }
    }
    return 0;
}

char* receive_message(ChatClient* client) {
    char* buffer = malloc(BUFFER_SIZE);
    if (!buffer) return NULL;
    
    ssize_t bytes_read = read(client->read_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        free(buffer);
        return NULL;
    }
    buffer[bytes_read] = '\0';
    return buffer;
}

int main() {
    int num_users;
    printf("Enter number of users (2-%d): ", MAX_USERS);
    scanf("%d", &num_users);
    getchar();

    if (num_users < 2 || num_users > MAX_USERS) {
        printf("Invalid number of users\n");
        return 1;
    }

    int pipes[num_users][2];
    for (int i = 0; i < num_users; i++) {
        if (pipe(pipes[i]) == -1) {
            handle_error("Pipe creation failed");
            return 1;
        }
    }

    ChatClient* clients[num_users];
    for (int i = 0; i < num_users; i++) {
        char username[BUFFER_SIZE];
        printf("Enter username for user %d: ", i + 1);
        fgets(username, BUFFER_SIZE, stdin);
        username[strcspn(username, "\n")] = 0;
        
        clients[i] = create_client(username, num_users - 1);
        if (!clients[i]) return 1;
        
        clients[i]->read_fd = pipes[i][READ_END];
        int write_idx = 0;
        for (int j = 0; j < num_users; j++) {
            if (i != j) {
                clients[i]->write_fds[write_idx++] = pipes[j][WRITE_END];
            }
        }
    }

    printf("Commands: \n/broadcast <message> - Send to all users\n/quit - Exit chat\n\n");

    for (int i = 0; i < num_users - 1; i++) {
        if (fork() == 0) {
            ChatClient* client = clients[i];
            char input[BUFFER_SIZE];
            
            while (1) {
                printf("%s> ", client->username);
                fflush(stdout);
                if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
                input[strcspn(input, "\n")] = 0;
                
                if (strcmp(input, "/quit") == 0) break;
                
                if (strncmp(input, "/broadcast ", 10) == 0) {
                    broadcast_message(client, input + 10);
                } else {
                    broadcast_message(client, input);
                }
                
                char* received_msg = receive_message(client);
                if (received_msg) {
                    printf("%s\n", received_msg);
                    free(received_msg);
                }
            }
            exit(0);
        }
    }

    ChatClient* client = clients[num_users - 1];
    char input[BUFFER_SIZE];
    while (1) {
        printf("%s> ", client->username);
        if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "/quit") == 0) break;
        
        if (strncmp(input, "/broadcast ", 10) == 0) {
            broadcast_message(client, input + 10);
        } else {
            broadcast_message(client, input);
        }
        
        char* received_msg = receive_message(client);
        if (received_msg) {
            printf("%s\n", received_msg);
            free(received_msg);
        }
    }

    for (int i = 0; i < num_users - 1; i++) {
        wait(NULL);
    }

    for (int i = 0; i < num_users; i++) {
        destroy_client(clients[i]);
        close(pipes[i][READ_END]);
        close(pipes[i][WRITE_END]);
    }

    return 0;
}