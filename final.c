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
    int write_fd;
    char* username;
} ChatClient;

void handle_error(const char* message) {
    fprintf(stderr, "Error: %s - %s\n", message, strerror(errno));
}

ChatClient* create_client(const char* username) {
    if (!username) return NULL;
    ChatClient* client = malloc(sizeof(ChatClient));
    if (!client) {
        handle_error("Failed to allocate client");
        return NULL;
    }
    client->username = strdup(username);
    if (!client->username) {
        free(client);
        handle_error("Failed to allocate username");
        return NULL;
    }
    client->read_fd = -1;
    client->write_fd = -1;
    return client;
}

void destroy_client(ChatClient* client) {
    if (client) {
        if (client->read_fd >= 0) close(client->read_fd);
        if (client->write_fd >= 0) close(client->write_fd);
        free(client->username);
        free(client);
    }
}

int send_message(ChatClient* client, const char* message) {
    if (!client || !message) return -1;
    char formatted_msg[BUFFER_SIZE];
    int written = snprintf(formatted_msg, BUFFER_SIZE, "%s: %s", client->username, message);
    if (written < 0 || written >= BUFFER_SIZE) return -1;
    return write(client->write_fd, formatted_msg, written + 1) > 0 ? 0 : -1;
}

char* receive_message(ChatClient* client) {
    if (!client) return NULL;
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

    int pipes[MAX_USERS][2];
    ChatClient* clients[MAX_USERS];
    memset(pipes, -1, sizeof(pipes));
    memset(clients, 0, sizeof(clients));

    // Create pipes
    for (int i = 0; i < num_users; i++) {
        if (pipe(pipes[i]) == -1) {
            handle_error("Pipe creation failed");
            return 1;
        }
    }

    // Create clients
    for (int i = 0; i < num_users; i++) {
        char username[BUFFER_SIZE];
        printf("Enter username for user %d: ", i + 1);
        if (!fgets(username, BUFFER_SIZE, stdin)) break;
        username[strcspn(username, "\n")] = 0;
        
        clients[i] = create_client(username);
        if (!clients[i]) return 1;
        
        // Set up circular communication
        clients[i]->write_fd = pipes[i][WRITE_END];
        clients[i]->read_fd = pipes[(i + 1) % num_users][READ_END];
    }

    // Create child processes
    pid_t child_pids[MAX_USERS - 1];
    for (int i = 0; i < num_users - 1; i++) {
        pid_t pid = fork();
        if (pid < 0) {
            handle_error("Fork failed");
            return 1;
        }
        if (pid == 0) {  // Child process
            ChatClient* client = clients[i];
            char input[BUFFER_SIZE];
            
            while (1) {
                char* received_msg = receive_message(client);
                if (received_msg) {
                    printf("%s\n", received_msg);
                    free(received_msg);
                }
                
                printf("%s> ", client->username);
                fflush(stdout);
                if (!fgets(input, BUFFER_SIZE, stdin)) break;
                input[strcspn(input, "\n")] = 0;
                
                if (strcmp(input, "quit") == 0) break;
                send_message(client, input);
            }
            exit(0);
        }
        child_pids[i] = pid;
    }

    // Parent process handles last user
    ChatClient* client = clients[num_users - 1];
    char input[BUFFER_SIZE];
    while (1) {
        printf("%s> ", client->username);
        if (!fgets(input, BUFFER_SIZE, stdin)) break;
        input[strcspn(input, "\n")] = 0;
        
        if (strcmp(input, "quit") == 0) break;
        send_message(client, input);
        
        char* received_msg = receive_message(client);
        if (received_msg) {
            printf("%s\n", received_msg);
            free(received_msg);
        }
    }

    // Cleanup
    for (int i = 0; i < num_users - 1; i++) {
        if (child_pids[i] > 0) {
            kill(child_pids[i], SIGTERM);
            waitpid(child_pids[i], NULL, 0);
        }
    }

    for (int i = 0; i < num_users; i++) {
        if (clients[i]) destroy_client(clients[i]);
        close(pipes[i][READ_END]);
        close(pipes[i][WRITE_END]);
    }

    return 0;
}