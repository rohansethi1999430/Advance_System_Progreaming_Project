#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>

#define PORT 8801  // Port number for the Stext server
#define BUFFER_SIZE 1024

// Function to create the directory structure
void create_directory(const char *path) {
    char tmp[BUFFER_SIZE];
    char *p = NULL;
    size_t len;

    snprintf(tmp, sizeof(tmp), "%s", path);
    len = strlen(tmp);
    if (tmp[len - 1] == '/')
        tmp[len - 1] = 0;
    for (p = tmp + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, S_IRWXU) != 0 && errno != EEXIST) {
                perror("Error creating directory");
                return;
            }
            *p = '/';
        }
    }
    if (mkdir(tmp, S_IRWXU) != 0 && errno != EEXIST) {
        perror("Error creating directory");
    }
}

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int n;

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        n = read(client_socket, buffer, BUFFER_SIZE);
        if (n <= 0) {
            printf("Client disconnected\n");
            break;
        }
        printf("Client: %s\n", buffer);

        char *command = strtok(buffer, " ");
        char *filename = strtok(NULL, " ");
        char *dest_path = strtok(NULL, " ");  // Optional, might be NULL for rmfile

        if (command && filename) {
            printf("Received command: %s, filename: %s\n", command, filename);

            if (strcmp(command, "ufile") == 0 && dest_path) {  // ufile requires dest_path
                printf("Creating directory: %s\n", dest_path);
                create_directory(dest_path);

                char full_path[BUFFER_SIZE];
                snprintf(full_path, sizeof(full_path), "%s/%s", dest_path, filename);

                FILE *fp = fopen(full_path, "w");
                if (fp == NULL) {
                    perror("Error opening file");
                    strcpy(buffer, "Error saving file\n");
                    write(client_socket, buffer, strlen(buffer));
                    continue;
                }

                // Read and write file content
                while ((n = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
                    fwrite(buffer, sizeof(char), n, fp);
                    if (n < BUFFER_SIZE) break; // End of file
                }
                fclose(fp);
                printf("Text file saved: %s\n", full_path);

                strcpy(buffer, "Text file saved successfully\n");
                write(client_socket, buffer, strlen(buffer));  // Send success message to Smain
                shutdown(client_socket, SHUT_RD);  // Shutdown the read side to close the communication cleanly
                break;

            } else if (strcmp(command, "dfile") == 0) {
                printf("Sending Text file: %s\n", filename);
                int fd = open(filename, O_RDONLY);
                if (fd == -1) {
                    perror("Error opening file");
                    strcpy(buffer, "Error reading file\n");
                    write(client_socket, buffer, strlen(buffer));
                } else {
                    while ((n = read(fd, buffer, BUFFER_SIZE)) > 0) {
                        write(client_socket, buffer, n);
                    }
                    close(fd);
                }
            } else if (strcmp(command, "rmfile") == 0) {  // rmfile does not need dest_path
                printf("Deleting file: %s\n", filename);
                char full_path[BUFFER_SIZE];
                snprintf(full_path, sizeof(full_path), "/home/chauha5a/ASP_Project_Main/Server/StextServer/%s", filename);

                printf("Full path for deletion: %s\n", full_path);  // Debugging print statement

                if (remove(full_path) == 0) {
                    printf("File %s deleted successfully\n", full_path);
                    strcpy(buffer, "File deleted successfully\n");
                } else {
                    perror("Error deleting file");
                    strcpy(buffer, "Error deleting file\n");
                }
                write(client_socket, buffer, strlen(buffer));
            } else {
                strcpy(buffer, "Unknown command\n");
                write(client_socket, buffer, strlen(buffer));
            }
        } else {
            printf("Invalid command received.\n");
            strcpy(buffer, "Invalid command\n");
            write(client_socket, buffer, strlen(buffer));
        }
    }

    close(client_socket);
}


// Main function to set up the server
int main() {
    int server_socket, client_socket, len;
    struct sockaddr_in server_addr, client_addr;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("Error in socket creation\n");
        exit(1);
    }
    printf("Socket created successfully\n");

    // Set the SO_REUSEADDR option to allow port reuse
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt))) {
        perror("setsockopt failed");
        exit(EXIT_FAILURE);
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if ((bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))) != 0) {
        printf("Socket bind failed\n");
        exit(1);
    }
    printf("Socket binded successfully\n");

    if ((listen(server_socket, 5)) != 0) {
        printf("Listen failed\n");
        exit(1);
    }
    printf("Server listening...\n");

    len = sizeof(client_addr);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &len);
        if (client_socket < 0) {
            printf("Server accept failed\n");
            exit(1);
        }
        printf("Server accepted the Smain request\n");

        if (fork() == 0) {
            close(server_socket);
            handle_client(client_socket);
            exit(0);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}