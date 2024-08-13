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
#include <dirent.h>

#define PORT 8091  // Port number for the Spdf server
#define BUFFER_SIZE 1024
#define BASE_DIR "/Users/rohansethi/Downloads/ASP_Programs/ASP_Final_Project/Server/SpdfServer"  // Replace with your actual base directory

// Function to map `~smain` to `~spdf` in the destination path
char* map_path(const char* path) {
    if (strncmp(path, "~smain", 6) == 0) {
        // Replace `~smain` with `~spdf` in the path
        char mapped_path[BUFFER_SIZE];
        snprintf(mapped_path, sizeof(mapped_path), "%s/~spdf/%s", BASE_DIR, path + 7);  // Skip "~smain/"
        return strdup(mapped_path);
    } else {
        return strdup(path);  // Return the original path if no mapping is needed
    }
}

// Function to list files in the directory specified by `path`
void list_files(const char *path, const char *extension, int client_socket) {
    struct dirent *entry;

    printf("Attempting to open directory: %s\n", path);  // Log the directory path being opened

    DIR *dp = opendir(path);
    if (dp == NULL) {
        perror("Error opening directory");
        char buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "Error opening directory: %s\n", path);
        write(client_socket, buffer, strlen(buffer));
        return;
    }

    // Directory opened successfully, log it
    printf("Directory opened successfully: %s\n", path);

    // Read the directory contents and send matching files to the client
    char buffer[BUFFER_SIZE];
    int files_found = 0;  // Track whether any files were found
    while ((entry = readdir(dp))) {
        if (entry->d_type == DT_REG && strstr(entry->d_name, extension)) {
            snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
            write(client_socket, buffer, strlen(buffer));
            printf("File sent: %s\n", entry->d_name);  // Log the file sent
            files_found = 1;  // File found, set the flag
        }
    }

    // Check if no files were found
    if (!files_found) {
        snprintf(buffer, sizeof(buffer), "No %s files found in directory: %s\n", extension, path);
        write(client_socket, buffer, strlen(buffer));
    }

    closedir(dp);

    // Indicate end of file list to the client
    // snprintf(buffer, sizeof(buffer), "End of %s file list\n", extension);
    write(client_socket, buffer, strlen(buffer));
    // printf("End of file list sent for: %s\n", path);  // Log end of file list
    close(client_socket);
}

// Function to handle the client request
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

        // Log the received filename to ensure it's correct
        printf("Received filename: %s\n", filename);

        if (command && strcmp(command, "display") == 0) {
            if (filename == NULL || strlen(filename) == 0) {
                strcpy(buffer, "Error: Filename or path is missing for display command\n");
                write(client_socket, buffer, strlen(buffer));
                continue;
            }

            printf("Received command: %s, filename: %s\n", command, filename);

            // Map the `~smain` path to the actual `~spdf` directory path
                        // Map the `~smain` path to the actual `~spdf` directory path
            char *full_path = map_path(filename);
            printf("Mapped path: %s\n", full_path);

            // List .pdf files in the mapped directory
            list_files(full_path, ".pdf", client_socket);

            free(full_path);
        } else {
            strcpy(buffer, "Unknown or unsupported command\n");
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
        close(server_socket);
        exit(1);
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if ((bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr))) != 0) {
        perror("Socket bind failed");
        close(server_socket);
        exit(1);
    }
    printf("Socket binded successfully\n");

    if ((listen(server_socket, 5)) != 0) {
        printf("Listen failed\n");
        close(server_socket);
        exit(1);
    }
    printf("Server listening...\n");

    len = sizeof(client_addr);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &len);
        if (client_socket < 0) {
            printf("Server accept failed\n");
            continue;
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