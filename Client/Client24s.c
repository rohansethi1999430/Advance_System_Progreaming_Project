#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>

#define PORT 9080
#define BUFFER_SIZE 1024

// Function to extract the file name from the path
char* extract_filename(const char* path) {
    char *filename = strrchr(path, '/');
    if (filename == NULL) {
        return strdup(path);  // No '/' found, return the original path as filename
    } else {
        return strdup(filename + 1);  // Return the string after the last '/'
    }
}

int main() {
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    FILE *fp;
    int n; // Declare the variable 'n' here

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0) {
        printf("Error in socket creation\n");
        exit(1);
    }
    printf("Socket created successfully\n");

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Ensure this matches the server's IP

    if (connect(client_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        printf("Connection to Smain failed\n");
        exit(1);
    }
    printf("Connected to Smain\n");

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        bzero(command, BUFFER_SIZE);

        printf("Enter command: ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = 0; // Remove newline character
        printf("Sending command: %s\n", command);

        write(client_socket, command, strlen(command));

        // Handle 'ufile' command
        if (strncmp("ufile", command, 5) == 0) {
            char *filename = strtok(command + 6, " "); // Get filename after 'ufile '
            fp = fopen(filename, "r");
            if (fp == NULL) {
                perror("Error opening file");
                continue;
            }
            printf("Sending file content of %s\n", filename);

            // Send file content to the server
            while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
                write(client_socket, buffer, n);
            }
            fclose(fp);
        }

        // Handle 'dfile' command
        if (strncmp("dfile", command, 5) == 0) {
            char *filepath = strtok(command + 6, " "); // Get file path after 'dfile '
            if (filepath != NULL) {
                char *filename = extract_filename(filepath);
                printf("Receiving file: %s\n", filename);
                fp = fopen(filename, "w");
                if (fp == NULL) {
                    perror("Error creating file");
                    free(filename);
                    continue;
                }

                int bytes_received;
                while ((bytes_received = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
                    fwrite(buffer, sizeof(char), bytes_received, fp);
                    if (bytes_received < BUFFER_SIZE) {
                        break;  // End of file or transmission
                    }
                }
                fclose(fp);
                printf("File %s received successfully\n", filename);
            continue;

            }
        }

        if (strncmp("rmfile", command, 6) == 0) {
            char *filepath = strtok(command + 7, " "); // Get file path after 'rmfile '
            if (filepath != NULL) {
                printf("Sending file removal request for: %s\n", filepath);
            }
        }

        // Expect a response from the server
        int bytes_received = read(client_socket, buffer, BUFFER_SIZE);
        if (bytes_received > 0) {
            buffer[bytes_received] = '\0'; // Null-terminate the buffer
            printf("Server: %s\n", buffer);
        } else {
            printf("No response from server, connection might be closed.\n");
            break;
        }

        if (strncmp("exit", command, 4) == 0) {
            printf("Exiting...\n");
            break;
        }
    }

    close(client_socket);
    return 0;
}
