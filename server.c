#include <winsock2.h>       // Windows sockets
#include <ws2tcpip.h>       // For getaddrinfo
#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#pragma comment(lib, "ws2_32.lib")  // Link the Winsock library

#define MAX_QUEUE_SIZE 100
#define BUFFER_SIZE 4096

// prioritized request
typedef struct {
    int client_sock;
    int priority; 
    char command[BUFFER_SIZE];
} Request;

// Array-based priority queue
Request requestQueue[MAX_QUEUE_SIZE];
int queueSize = 0;
pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;

// add a request to the priority queue
void enqueue(Request req) {
    pthread_mutex_lock(&queue_lock);
    if (queueSize >= MAX_QUEUE_SIZE) {
        printf("Queue is full. Dropping request.\n");
        pthread_mutex_unlock(&queue_lock);
        return;
    }
    requestQueue[queueSize++] = req;

    // Sort the queue based on priority (simple insertion sort)
    for (int i = queueSize - 1; i > 0; i--) {
        if (requestQueue[i].priority < requestQueue[i - 1].priority) {
            Request temp = requestQueue[i];
            requestQueue[i] = requestQueue[i - 1];
            requestQueue[i - 1] = temp;
        } else {
            break;
        }
    }
    pthread_mutex_unlock(&queue_lock);
}

// Function to remove a request from the priority queue
Request dequeue() {
    pthread_mutex_lock(&queue_lock);
    Request req = {0, 0, ""};
    if (queueSize > 0) {
        req = requestQueue[0];
        for (int i = 1; i < queueSize; i++) {
            requestQueue[i - 1] = requestQueue[i];
        }
        queueSize--;
    }
    pthread_mutex_unlock(&queue_lock);
    return req;
}

// Function to process commands
void process_command(Request req, char* response, size_t response_size) {
    char *command = req.command;
    if (strncmp(command, "help", 4) == 0) {
        snprintf(response, response_size, "Available commands:\n"
                 "1. +: Add numbers (e.g., + 3 4)\n"
                 "2. -: Subtract numbers (e.g., - 5 2)\n"
                 "3. *: Multiply numbers (e.g., * 3 4)\n"
                 "4. /: Divide numbers (e.g., / 8 2)\n"
                 "5. list: List commands\n"
                 "6. exit: Close connection\n"
                 "7. Plain text is echoed back.");
    } else if (strncmp(command, "+", 1) == 0) {
        int a, b;
        if (sscanf(command, "+ %d %d", &a, &b) == 2) {
            snprintf(response, response_size, "Result: %d", a + b);
        } else {
            snprintf(response, response_size, "Invalid format. Use: + <num1> <num2>");
        }
    } else if (strncmp(command, "-", 1) == 0) {
        int a, b;
        if (sscanf(command, "- %d %d", &a, &b) == 2) {
            snprintf(response, response_size, "Result: %d", a - b);
        } else {
            snprintf(response, response_size, "Invalid format. Use: - <num1> <num2>");
        }
    } else if (strncmp(command, "*", 1) == 0) {
        int a, b;
        if (sscanf(command, "* %d %d", &a, &b) == 2) {
            snprintf(response, response_size, "Result: %d", a * b);
        } else {
            snprintf(response, response_size, "Invalid format. Use: * <num1> <num2>");
        }
    } else if (strncmp(command, "/", 1) == 0) {
        int a, b;
        if (sscanf(command, "/ %d %d", &a, &b) == 2) {
            if (b != 0) {
                snprintf(response, response_size, "Result: %d", a / b);
            } else {
                snprintf(response, response_size, "Error: Division by zero is not allowed.");
            }
        } else {
            snprintf(response, response_size, "Invalid format. Use: / <num1> <num2>");
        }
    } else if (strncmp(command, "list", 4) == 0) {
        snprintf(response, response_size, "Commands: help, +, -, *, /, list, exit.");
    } else if (strncmp(command, "exit", 4) == 0) {
        snprintf(response, response_size, "Goodbye!");
    } else {
        snprintf(response, response_size, "Echo: %s", command);
    }
}

// Function to process a request
void process_request(Request req) {
    char send_data[BUFFER_SIZE];
    memset(send_data, 0, sizeof(send_data)); // Clear buffer

    process_command(req, send_data, sizeof(send_data));

    send(req.client_sock, send_data, strlen(send_data), 0);
}

// Thread to handle the priority queue
void* process_queue(void* arg) {
    while (1) {
        Request req = dequeue();
        if (req.client_sock != 0) { // Valid request
            process_request(req);
        } else {
            Sleep(100); // Sleep to avoid busy waiting
        }
    }
    return NULL;
}

// Thread to handle client connections
void* client_handler(void* socket_desc) {
    int sock = *(int*)socket_desc;
    char recv_data[BUFFER_SIZE];
    int bytes_received;

    while ((bytes_received = recv(sock, recv_data, sizeof(recv_data) - 1, 0)) > 0) {
        recv_data[bytes_received] = '\0';

        // Parse priority and command (format: "PRIORITY:COMMAND")
        int priority = 2; // Default priority (Medium)
        char command[BUFFER_SIZE];
        sscanf(recv_data, "%d:%[^\n]", &priority, command);

        // Add request to priority queue
        Request req = {sock, priority, ""};
        strncpy(req.command, command, sizeof(req.command) - 1);

        enqueue(req);
    }

    closesocket(sock);  // Use closesocket in Windows
    return NULL;
}

int main() {
    WSADATA wsaData;
    int server_sock, client_sock, sin_size;
    struct sockaddr_in server_addr, client_addr;

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        printf("WSAStartup failed\n");
        return 1;
    }

    // Create socket
    if ((server_sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Setup server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(36000);
    server_addr.sin_addr.s_addr = INADDR_ANY;
    memset(&(server_addr.sin_zero), 0, 8);

    // Bind socket
    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("Bind failed");
        exit(1);
    }

    // Listen for connections
    if (listen(server_sock, 5) == -1) {
        perror("Listen failed");
        exit(1);
    }

    printf("Server listening on port 36000\n");

    // Create a thread to process the queue
    pthread_t queue_thread;
    pthread_create(&queue_thread, NULL, process_queue, NULL);

    // Accept client connections
    while (1) {
        sin_size = sizeof(struct sockaddr_in);
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, (socklen_t*)&sin_size);

        if (client_sock == -1) {
            perror("Accept failed");
            continue;
        }

        printf("Client connected\n");

        pthread_t client_thread;
        pthread_create(&client_thread, NULL, client_handler, &client_sock);
    }

    // Cleanup Winsock
    WSACleanup();

    return 0;
}
