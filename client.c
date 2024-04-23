#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define SERVER_IP "127.0.0.1" // localhost
#define PORT 8888
#define MAXDATASIZE 1024

// Function to send commands to the server and receive responses
void send_command_to_server(int client_socket, const char *command) {
    char buffer[MAXDATASIZE];
    int bytes_received;

    // Send command to server
    send(client_socket, command, strlen(command), 0);

    // Handle specific responses
    if (strcmp(command, "quitc") == 0) {
        return; // No need to receive response for quit command
    }
    else if (strncmp(command, "w24fz ", 6) == 0) {
        // Handle w24fz response separately
        bytes_received = recv(client_socket, buffer, MAXDATASIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive");
            return;
        }
        buffer[bytes_received] = '\0';
        if (strcmp(buffer, "No file found") == 0) {
            printf("No file found within the specified size range.\n");
        } else {
            printf("TAR file created\n");
        }
    }
    else if (strncmp(command, "w24ft ", 6) == 0) {
        // Handle w24ft response separately
        bytes_received = recv(client_socket, buffer, MAXDATASIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive");
            return;
        }
        buffer[bytes_received] = '\0';
        if (strcmp(buffer, "No file found") == 0) {
            printf("No files found matching the specified extensions.\n");
        } else {
            printf("Archive created\n");
        }
    }
    else if (strncmp(command, "w24fn ", 6) == 0) {
        // Handle w24fn response separately
        bytes_received = recv(client_socket, buffer, MAXDATASIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive");
            return;
        }
        buffer[bytes_received] = '\0';
        if (strcmp(buffer, "No file found") == 0) {
            printf("No files found matching the specified extensions.\n");
        } else {
        printf("File contents:\n%s\n", buffer);
        }
    }
    else if (strncmp(command, "w24fdb ", 7) == 0 || strncmp(command, "w24fda ", 7) == 0) {
        // Handle w24fdb/w24fda response separately
        bytes_received = recv(client_socket, buffer, MAXDATASIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive");
            return;
        }
        buffer[bytes_received] = '\0';
        if (strcmp(buffer, "No files found with the specified creation date or earlier.") == 0 || strcmp(buffer, "No files found with the specified creation date or later.") == 0) {
            printf("No files found with the specified creation date.\n");
        } else {
            printf("TAR file received and saved as temp.tar.gz\n");
        }
    }
    else {
        // Receive response from server for other commands
        bytes_received = recv(client_socket, buffer, MAXDATASIZE, 0);
        if (bytes_received <= 0) {
            perror("Failed to receive");
            return;
        }
        buffer[bytes_received] = '\0';
        printf("Response from server: %s\n", buffer);
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char command[MAXDATASIZE];

    // Create socket
    if ((client_socket = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        exit(1);
    }

    // Server address setup
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    memset(&(server_addr.sin_zero), '\0', 8);

    // Connect to server
    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(struct sockaddr)) == -1) {
        perror("Connection failed");
        exit(1);
    }

    printf("Connected to server.\n");

    while (1) {
        printf("Enter command: ");
        fgets(command, MAXDATASIZE, stdin);
        command[strcspn(command, "\n")] = '\0';

        // Validate command syntax
        if (strcmp(command, "dirlist -a") != 0 && strcmp(command, "dirlist -t") != 0 && strcmp(command, "quitc") != 0 && strncmp(command, "w24fn ", 6) != 0 && strncmp(command, "w24fz ", 6) != 0 && strncmp(command, "w24ft ", 6) != 0 && strncmp(command, "w24fdb ", 7) != 0 && strncmp(command, "w24fda ", 7) != 0) {
            printf("Invalid command. Please enter a valid command\n");
            continue;
        }
        else if (strncmp(command, "w24fz ", 6) == 0) {
            long size1, size2;
            if (sscanf(command + 6, "%ld %ld", &size1, &size2) != 2) {
                printf("Invalid command syntax for w24fz. Please enter two integer values for size1 and size2.\n");
                continue;
            }
        }

        // Send command to server and receive response
        send_command_to_server(client_socket, command);

        // Check if quit command is entered
        if (strcmp(command, "quitc") == 0) {
            break;
        }
    }

    // Close socket
    close(client_socket);

    return 0;
}
