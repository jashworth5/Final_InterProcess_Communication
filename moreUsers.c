// chat.h
#ifndef CHAT_H
#define CHAT_H

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

typedef struct {
    int read_fd;
    int write_fd;
    char* username;
} ChatClient;

ChatClient* create_client(const char* username);
void destroy_client(ChatClient* client);
int send_message(ChatClient* client, const char* message);
char* receive_message(ChatClient* client);
void handle_error(const char* message);

#endif

// chat.c

ChatClient* create_client(const char* username) {
    if (!username) return NULL;

    ChatClient* client = (ChatClient*)malloc(sizeof(ChatClient));
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

    size_t username_len = strlen(client->username);
    size_t message_len = strlen(message);
    
    if (username_len + message_len + 3 > BUFFER_SIZE) {
        handle_error("Message too long");
        return -1;
    }

    char formatted_msg[BUFFER_SIZE];
    int written = snprintf(formatted_msg, BUFFER_SIZE, "%s: %s", client->username, message);
    
    if (written < 0 || written >= BUFFER_SIZE) {
        handle_error("Message formatting failed");
        return -1;
    }

    ssize_t bytes_written = write(client->write_fd, formatted_msg, written + 1);
    return bytes_written > 0 ? 0 : -1;
}

char* receive_message(ChatClient* client) {
    if (!client) return NULL;

    char* buffer = (char*)malloc(BUFFER_SIZE);
    if (!buffer) {
        handle_error("Failed to allocate receive buffer");
        return NULL;
    }

    ssize_t bytes_read = read(client->read_fd, buffer, BUFFER_SIZE - 1);
    if (bytes_read <= 0) {
        free(buffer);
        return NULL;
    }

    buffer[bytes_read] = '\0';
    return buffer;
}

void handle_error(const char* message) {
    fprintf(stderr, "Error: %s - %s\n", message, strerror(errno));
}

// main.c
int main() {
    int pipe1[2], pipe2[2], pipe3[2], pipe4[2];
    
    if (pipe(pipe1) == -1 || pipe(pipe2) == -1 || pipe(pipe3) == -1 || pipe(pipe4) == -1) {
        handle_error("Pipe creation failed");
        return 1;
    }

    ChatClient* client1 = create_client("User1");
    ChatClient* client2 = create_client("User2");
    ChatClient* client3 = create_client("User3");
    ChatClient* client4 = create_client("User4");

    if (!client1 || !client2 || !client3 || !client4) {
        handle_error("Failed to create clients");
        if (client1) destroy_client(client1);
        if (client2) destroy_client(client2);
        if (client3) destroy_client(client3);
        if (client4) destroy_client(client4);
        return 1;
    }

    client1->write_fd = pipe1[WRITE_END];
    client1->read_fd = pipe2[READ_END];
    
    client2->write_fd = pipe2[WRITE_END];
    client2->read_fd = pipe1[READ_END];
    
    client3->write_fd = pipe3[WRITE_END];
    client3->read_fd = pipe4[READ_END];
    
    client4->write_fd = pipe4[WRITE_END];
    client4->read_fd = pipe3[READ_END];

    pid_t pid1 = fork();
    pid_t pid2 = fork();
    pid_t pid3 = fork();
    
    
    if (pid1 < 0 || pid2 < 0 || pid3 < 0) {
        handle_error("Fork failed");
        destroy_client(client1);
        destroy_client(client2);
        destroy_client(client3);
        destroy_client(client4);
        return 1;
    }

    char input[BUFFER_SIZE];
    if (pid1 == 0) {  // Child process (Client 2)
        close(pipe1[WRITE_END]);
        close(pipe2[READ_END]);
        close(pipe3[WRITE_END]);
        close(pipe4[READ_END]);
        
        while (1) {
            printf("User3> ");
            if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "quit") == 0) break;
            send_message(client3, input);
            
            char* received_msg = receive_message(client3);
            if (received_msg) {
                printf("User3 received: %s\n", received_msg);
                free(received_msg);
            }
        }
    }
    else if (pid2 == 0) {  // Client 2 process
        close(pipe1[WRITE_END]);
        close(pipe2[READ_END]);
        close(pipe3[READ_END]);
        close(pipe4[WRITE_END]);

        while (1) {
            printf("User2> ");
            if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "quit") == 0) break;
            send_message(client2, input);
            
            char* received_msg = receive_message(client2);
            if (received_msg) {
                printf("User2 received: %s\n", received_msg);
                free(received_msg);
            }
        }
    } else if (pid3 == 0) {  // Client 3 process
        close(pipe1[READ_END]);
        close(pipe2[WRITE_END]);
        close(pipe3[WRITE_END]);
        close(pipe4[READ_END]);

        while (1) {
            printf("User3> ");
            if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "quit") == 0) break;
            send_message(client3, input);
            
            char* received_msg = receive_message(client3);
            if (received_msg) {
                printf("User3 received: %s\n", received_msg);
                free(received_msg);
            }
        }
    } else {  // Parent process (Client 4)
        close(pipe1[READ_END]);
        close(pipe2[WRITE_END]);
        close(pipe3[WRITE_END]);
        close(pipe4[READ_END]);

        while (1) {
            printf("User4> ");
            if (fgets(input, BUFFER_SIZE, stdin) == NULL) break;
            input[strcspn(input, "\n")] = 0;

            if (strcmp(input, "quit") == 0) break;
            send_message(client4, input);
            
            char* received_msg = receive_message(client4);
            if (received_msg) {
                printf("User4 received: %s\n", received_msg);
                free(received_msg);
            }
        }

        wait(NULL);
        wait(NULL);
        wait(NULL);
    }

    destroy_client(client1);
    destroy_client(client2);
    destroy_client(client3);
    destroy_client(client4);
    return 0;
}
