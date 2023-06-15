#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <pthread.h>

#define MAX_CLIENTS 100
#define MAX_ROOMS 10
#define MAX_USERNAME_LENGTH 20
#define MAX_MESSAGE_LENGTH 1000

//Odaların yapısını oluşturan struct
struct Room {
    int occupied;
    char name[20];
    pthread_t thread;
    pthread_mutex_t mutex;
    int clients[MAX_CLIENTS];
};

struct Room rooms[MAX_ROOMS];
pthread_mutex_t rooms_mutex;

//Sunucunun mesajlarını odada ki kullanıcılara gönderir
void send_message_to_room(char* message, int room_index, int sender_socket) {
    pthread_mutex_lock(&rooms[room_index].mutex);

    for (int i = 0; i < MAX_CLIENTS; i++) {
        int client_socket = rooms[room_index].clients[i];

        if (client_socket != 0 && client_socket != sender_socket) {
            send(client_socket, message, strlen(message), 0);
        }
    }

    pthread_mutex_unlock(&rooms[room_index].mutex);
}

void* handle_client(void* arg) {
    int client_socket = *(int*)arg;
    char client_username[MAX_USERNAME_LENGTH];
    char client_message[MAX_MESSAGE_LENGTH];

    // Receive username from client
    memset(client_username, 0, sizeof(client_username));
    recv(client_socket, client_username, sizeof(client_username), 0);

    printf("New connection: %s\n", client_username);

    while (1) {
        memset(client_message, 0, sizeof(client_message));

        int message_size = recv(client_socket, client_message, sizeof(client_message), 0);

        if (message_size <= 0) {
            printf("Disconnected: %s\n", client_username);
            close(client_socket);
            break;
        }

        if (strncmp(client_message, "/logout", 7) == 0) {
            printf("Disconnected: %s\n", client_username);
            close(client_socket);
            break;
        }
        else if (strncmp(client_message, "/help", 5) == 0) {
            char help_message[] = "Available commands:\n"
                                  "/help - Show available commands\n"
                                  "/list - List available rooms\n"
                                  "/create <room_name> - Create a new room\n"
                                  "/enter <room_name> - Enter a room\n"
                                  "/listusers - List users in the current room\n"
                                  "/turnlobby - Return to the default lobby room\n"
                                  "/logout - Disconnect from the server\n";
            send(client_socket, help_message, strlen(help_message), 0);
        }
        else if (strncmp(client_message, "/list", 5) == 0) {
            char room_list[200] = "Available rooms:\n";
            pthread_mutex_lock(&rooms_mutex);

            for (int i = 0; i < MAX_ROOMS; i++) {
                if (rooms[i].occupied == 1) {
                    strcat(room_list, "- ");
                    strcat(room_list, rooms[i].name);
                    strcat(room_list, "\n");
                }
            }

            pthread_mutex_unlock(&rooms_mutex);

            send(client_socket, room_list, strlen(room_list), 0);
        }
        else if (strncmp(client_message, "/create", 7) == 0) {
            char* room_name = strtok(client_message, " ");
            room_name = strtok(NULL, " ");

            if (room_name != NULL) {
                pthread_mutex_lock(&rooms_mutex);

                int room_index = -1;

                for (int i = 0; i < MAX_ROOMS; i++) {
                    if (rooms[i].occupied == 0) {
                        room_index = i;
                        break;
                    }
                }

                if (room_index != -1) {
                    strcpy(rooms[room_index].name, room_name);
                    rooms[room_index].occupied = 1;
                    pthread_mutex_init(&rooms[room_index].mutex, NULL);

                    char success_message[100];
                    sprintf(success_message, "Room '%s' created.\n", room_name);
                    send(client_socket, success_message, strlen(success_message), 0);
                }
                else {
                    char error_message[] = "Maximum room limit reached. Unable to create a new room.\n";
                    send(client_socket, error_message, strlen(error_message), 0);
                }

                pthread_mutex_unlock(&rooms_mutex);
            }
        }
        else if (strncmp(client_message, "/enter", 6) == 0) {
            char* room_name = strtok(client_message, " ");
            room_name = strtok(NULL, " ");

            if (room_name != NULL) {
                pthread_mutex_lock(&rooms_mutex);

                int room_index = -1;

                for (int i = 0; i < MAX_ROOMS; i++) {
                    if (rooms[i].occupied == 1 && strcmp(rooms[i].name, room_name) == 0) {
                        room_index = i;
                        break;
                    }
                }

                if (room_index != -1) {
                    pthread_mutex_lock(&rooms[room_index].mutex);
                    rooms[room_index].clients[client_socket] = client_socket;
                    pthread_mutex_unlock(&rooms[room_index].mutex);

                    char success_message[100];
                    sprintf(success_message, "Entered room '%s'.\n", room_name);
                    send(client_socket, success_message, strlen(success_message), 0);
                }
                else {
                    char error_message[100];
                    sprintf(error_message, "Room '%s' does not exist.\n", room_name);
                    send(client_socket, error_message, strlen(error_message), 0);
                }

                pthread_mutex_unlock(&rooms_mutex);
            }
        }
        else if (strncmp(client_message, "/listusers", 10) == 0) {
            pthread_mutex_lock(&rooms_mutex);

            int current_room_index = -1;

            for (int i = 0; i < MAX_ROOMS; i++) {
                if (rooms[i].occupied == 1 && rooms[i].clients[client_socket] != 0) {
                    current_room_index = i;
                    break;
                }
            }

            if (current_room_index != -1) {
                pthread_mutex_lock(&rooms[current_room_index].mutex);

                char user_list[200] = "Users in the current room:\n";

                for (int i = 0; i < MAX_CLIENTS; i++) {
                    int client = rooms[current_room_index].clients[i];

                    if (client != 0) {
                        strcat(user_list, "- ");
                        strcat(user_list, client_username);
                        strcat(user_list, "\n");
                    }
                }

                send(client_socket, user_list, strlen(user_list), 0);

                pthread_mutex_unlock(&rooms[current_room_index].mutex);
            }
            else {
                char error_message[] = "You are not in any room.\n";
                send(client_socket, error_message, strlen(error_message), 0);
            }

            pthread_mutex_unlock(&rooms_mutex);
        }
        else if (strncmp(client_message, "/turnlobby", 10) == 0) {
            pthread_mutex_lock(&rooms_mutex);

            for (int i = 0; i < MAX_ROOMS; i++) {
                if (rooms[i].occupied == 1 && rooms[i].clients[client_socket] != 0) {
                    pthread_mutex_lock(&rooms[i].mutex);
                    rooms[i].clients[client_socket] = 0;
                    pthread_mutex_unlock(&rooms[i].mutex);
                    break;
                }
            }

            pthread_mutex_unlock(&rooms_mutex);

            char success_message[] = "Returned to the default lobby room.\n";
            send(client_socket, success_message, strlen(success_message), 0);
        }
        else {
            pthread_mutex_lock(&rooms_mutex);

            int current_room_index = -1;

            for (int i = 0; i < MAX_ROOMS; i++) {
                if (rooms[i].occupied == 1 && rooms[i].clients[client_socket] != 0) {
                    current_room_index = i;
                    break;
                }
            }

            if (current_room_index != -1) {
                char message_with_username[MAX_MESSAGE_LENGTH + MAX_USERNAME_LENGTH + 4];
                sprintf(message_with_username, "%s: %s\n", client_username, client_message);
                send_message_to_room(message_with_username, current_room_index, client_socket);
            }
            else {
                char error_message[] = "You are not in any room. Use the /enter <room_name> command to enter a room.\n";
                send(client_socket, error_message, strlen(error_message), 0);
            }

            pthread_mutex_unlock(&rooms_mutex);
        }
    }

    return NULL;
}

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_address, client_address;
    int address_length = sizeof(client_address);
    int thread_count = 0;

    // Initialize rooms
    for (int i = 0; i < MAX_ROOMS; i++) {
        rooms[i].occupied = 0;
        memset(rooms[i].name, 0, sizeof(rooms[i].name));
        memset(rooms[i].clients, 0, sizeof(rooms[i].clients));
        pthread_mutex_init(&rooms[i].mutex, NULL);
    }

    pthread_mutex_init(&rooms_mutex, NULL);

    // Create server socket
    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket == -1) {
        perror("Failed to create server socket");
        exit(EXIT_FAILURE);
    }

    // Set server address
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(12345);

    // Bind the server socket to the specified address and port
    if (bind(server_socket, (struct sockaddr*)&server_address, sizeof(server_address)) == -1) {
        perror("Failed to bind server socket");
        exit(EXIT_FAILURE);
    }

    // Listen for incoming connections
    if (listen(server_socket, 5) == -1) {
        perror("Failed to listen for connections");
        exit(EXIT_FAILURE);
    }

    printf("Server started. Listening on port 12345...\n");

    while (1) {
        // Accept incoming connection
        client_socket = accept(server_socket, (struct sockaddr*)&client_address, (socklen_t*)&address_length);
        if (client_socket == -1) {
            perror("Failed to accept connection");
            exit(EXIT_FAILURE);
        }

        // Create a new thread to handle the client
        pthread_t client_thread;
        pthread_create(&client_thread, NULL, handle_client, (void*)&client_socket);

        thread_count++;
    }

    // Close server socket
    close(server_socket);

    return 0;
}

