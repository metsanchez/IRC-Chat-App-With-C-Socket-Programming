#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_MESSAGE_LENGTH 1000

void* receive_messages(void* arg) {
    int socket = *(int*)arg;
    char server_message[MAX_MESSAGE_LENGTH];

    while (1) {
        memset(server_message, 0, sizeof(server_message));
        int message_size = recv(socket, server_message, sizeof(server_message), 0);

        if (message_size <= 0) {
            printf("Disconnected from server.\n");
            exit(EXIT_SUCCESS);
        }

        printf("%s", server_message);
    }

    return NULL;
}

int main() {
    int client_socket;
    struct sockaddr_in server_address;

    // Create client socket
    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket == -1) {
        perror("Failed to create client socket");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = inet_addr("127.0.0.1");
    server_address.sin_port = htons(12345);

    // Connect to the server
    if (connect(client_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Failed to connect to the server");
        exit(EXIT_FAILURE);
    }

    printf("Connected to the server.\n");

    // Receive and print server messages in a separate thread
    pthread_t receive_thread;
    pthread_create(&receive_thread, NULL, receive_messages, (void*)&client_socket);

    // Read user input and send messages to the server
    char client_message[MAX_MESSAGE_LENGTH];
    char username[20];

    printf("Enter your username: ");
    fgets(username, sizeof(username), stdin);
    username[strcspn(username, "\n")] = '\0';

    send(client_socket, username, strlen(username), 0);

    while (1) {
        memset(client_message, 0, sizeof(client_message));
        fgets(client_message, sizeof(client_message), stdin);

        // Remove newline character from the message
        client_message[strcspn(client_message, "\n")] = '\0';

        // Send the message to the server
        send(client_socket, client_message, strlen(client_message), 0);

        if (strncmp(client_message, "/logout", 7) == 0) {
            printf("Disconnected from server.\n");
            break;
        }
    }

    // Close client socket
    close(client_socket);

    return 0;
}

