#include <winsock2.h>       // Windows sockets
#include <ws2tcpip.h>       // For getaddrinfo
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <ctype.h>
#include <time.h>

#pragma comment(lib, "ws2_32.lib")  // Link the Winsock library

FILE* log_file;

void log_message(const char* message) {
    time_t now = time(NULL);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));
    fprintf(log_file, "[%s] %s\n", timestamp, message);
    fflush(log_file); // Ensure the log is written immediately
}

void trim_newline(char* str) {
    size_t len = strlen(str);
    if (len > 0 && str[len - 1] == '\n') {
        str[len - 1] = '\0';
    }
}

int is_valid_priority(const char* str) {
    return strlen(str) == 1 && isdigit(str[0]) && (str[0] >= '1' && str[0] <= '3');
}

int main(int argc, char *argv[]) {
    WSADATA wsaData;
    SOCKET sock;
    int bytes_received;
    char send_data[4096], recv_data[4096];
    struct sockaddr_in server_addr;

    // Open log file
    log_file = fopen("client_log.txt", "a");
    if (!log_file) {
        perror("Failed to open log file");
        exit(1);
    }

    log_message("Client started");

    // Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        log_message("WSAStartup failed");
        perror("WSAStartup failed");
        exit(1);
    }

    // Create socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == INVALID_SOCKET) {
        log_message("Socket creation failed");
        perror("Socket creation failed");
        WSACleanup();
        exit(1);
    }
    log_message("Socket created successfully");

    // Set up server address
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(36000);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Using IP directly
    memset(&(server_addr.sin_zero), 0, 8);

    // Connect to server
    if (connect(sock, (struct sockaddr*)&server_addr, sizeof(struct sockaddr)) == SOCKET_ERROR) {
        log_message("Connection to server failed");
        perror("Connection to server failed");
        closesocket(sock);
        WSACleanup();
        exit(1);
    }
    log_message("Connected to server");

    // Main communication loop
    while (1) {
        printf("Enter Priority (1 = High, 2 = Medium, 3 = Low) and Command (format: PRIORITY:COMMAND):\n");
        fgets(send_data, sizeof(send_data), stdin);
        trim_newline(send_data);

        // Log user input
        char log_input[4200];
        snprintf(log_input, sizeof(log_input), "User input: %s", send_data);
        log_message(log_input);

        // Check for exit command
        if (strcmp(send_data, "exit") == 0) {
            log_message("Sending exit command to server");
            send(sock, "2:EXIT", strlen("2:EXIT"), 0);
            printf("Closing connection...\n");
            closesocket(sock);
            log_message("Connection closed");
            break;
        }

        // Validate input format (PRIORITY:COMMAND)
        char priority[2], command[4096];
        if (sscanf(send_data, "%1[0-9]:%[^\n]", priority, command) != 2 || !is_valid_priority(priority)) {
            printf("Invalid input format. Please use the format: PRIORITY:COMMAND (e.g., 1:HELP).\n");
            log_message("Invalid input format detected");
            continue;
        }

        // Send validated data to server
        log_message("Sending data to server");
        send(sock, send_data, strlen(send_data), 0);

        // Receive response from server
        bytes_received = recv(sock, recv_data, sizeof(recv_data) - 1, 0);
        if (bytes_received <= 0) {
            log_message("Server disconnected or error occurred");
            printf("Server disconnected or an error occurred. Exiting...\n");
            closesocket(sock);
            break;
        }
        recv_data[bytes_received] = '\0';
        printf("Server response: %s\n", recv_data);

        // Log server response
        char log_response[4200];
        snprintf(log_response, sizeof(log_response), "Server response: %s", recv_data);
        log_message(log_response);
    }

    // Cleanup Winsock
    log_message("Cleaning up Winsock");
    WSACleanup();

    // Close log file
    fclose(log_file);

    return 0;
}
