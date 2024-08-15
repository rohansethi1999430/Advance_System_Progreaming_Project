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

#define PORT 8801 // Port number for the Stext server
#define BUFFER_SIZE 1024
#define BASE_DIR "/home/sethi83/ASP_Project_Main/Server/StextServer"  // Replace with your actual base directory

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

// Function to map `~smain` to `~stext` in the destination path
char* map_path(const char* path) {
    if (strncmp(path, "~smain", 6) == 0) {
        // Replace `~smain` with `~stext` in the path
        char mapped_path[BUFFER_SIZE];
        snprintf(mapped_path, sizeof(mapped_path), "%s/~stext/%s", BASE_DIR, path + 7);  // Skip "~smain/"
        char *result = strdup(mapped_path);
        if (result == NULL) {
            perror("Error duplicating mapped path");
            return NULL;
        }
        return result;
    } else {
        char *result = strdup(path);  // Return the original path if no mapping is needed
        if (result == NULL) {
            perror("Error duplicating path");
            return NULL;
        }
        return result;
    }
}

// Function to list files in the directory specified by `path`
void list_files(const char *path, const char *extension, int client_socket) {
    struct dirent *entry;
    struct stat entry_stat;
    char full_entry_path[BUFFER_SIZE];

    printf("Attempting to open directory: %s\n", path);  // Log the directory path being opened

    DIR *dp = opendir(path);
    if (dp == NULL) {
        perror("Error opening directory");
        char buffer[BUFFER_SIZE];
        snprintf(buffer, sizeof(buffer), "Error opening directory: %s\n", path);
        if (write(client_socket, buffer, strlen(buffer)) == -1) {
            perror("Error sending directory open error to client");
        }
        return;
    }

    // Directory opened successfully, log it
    printf("Directory opened successfully: %s\n", path);

    // Read the directory contents and send matching files to the client
    char buffer[BUFFER_SIZE];
    int files_found = 0;  // Track whether any files were found
    while ((entry = readdir(dp))) {
        snprintf(full_entry_path, sizeof(full_entry_path), "%s/%s", path, entry->d_name);

        // Use stat to get the entry's information
        if (stat(full_entry_path, &entry_stat) == 0 && S_ISREG(entry_stat.st_mode)) {
            if (strstr(entry->d_name, extension)) {
                snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
                if (write(client_socket, buffer, strlen(buffer)) == -1) {
                    perror("Error sending file name to client");
                    break;
                }
                printf("File sent: %s\n", entry->d_name);  // Log the file sent
                files_found = 1;  // File found, set the flag
            }
        } else {
            perror("Error reading file status");
        }
    }

    // Check if no files were found
    if (!files_found) {
        snprintf(buffer, sizeof(buffer), "No %s files found in directory: %s\n", extension, path);
        if (write(client_socket, buffer, strlen(buffer)) == -1) {
            perror("Error sending no files found message to client");
        }
    }

    closedir(dp);

    // Indicate end of file list to the client
    snprintf(buffer, sizeof(buffer), "End of file list\n");
    if (write(client_socket, buffer, strlen(buffer)) == -1) {
        perror("Error sending end of file list to client");
    }
    printf("End of file list sent for: %s\n", path);  // Log end of file list
}

// Function to handle the client request
void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    int n;

    while (1) {
        bzero(buffer, BUFFER_SIZE);
        n = read(client_socket, buffer, BUFFER_SIZE);
        if (n <= 0) {
            if (n < 0) {
                perror("Error reading from client socket");
            } else {
                printf("Client disconnected\n");
            }
            break;
        }
        printf("Client: %s\n", buffer);

        char *command = strtok(buffer, " ");
        char *filename = strtok(NULL, " ");
        char *dest_path = strtok(NULL, "\n");

        // Trim any extra spaces or newlines from dest_path
        if (dest_path != NULL) {
            dest_path = strtok(dest_path, "\n");
        }

        if (command && strcmp(command, "display") == 0) {
            if (filename == NULL || strlen(filename) == 0) {
                strcpy(buffer, "Error: Filename or path is missing for display command\n");
                if (write(client_socket, buffer, strlen(buffer)) == -1) {
                    perror("Error sending missing filename error to client");
                }
                continue;
            }

            printf("Received command: %s, filename: %s\n", command, filename);

            // Map the `~smain` path to the actual `~stext` directory path
            char *full_path = map_path(filename);
            if (full_path == NULL) {
                strcpy(buffer, "Error mapping path\n");
                if (write(client_socket, buffer, strlen(buffer)) == -1) {
                    perror("Error sending path mapping error to client");
                }
                continue;
            }
            printf("Mapped path: %s\n", full_path);

            // List .txt files in the mapped directory
            list_files(full_path, ".txt", client_socket);

            free(full_path);
        } else if (command && filename) {
            printf("Received command: %s, filename: %s\n", command, filename);

            if (strcmp(command, "ufile") == 0 && dest_path != NULL) {
                printf("Creating directory: %s\n", dest_path);
                create_directory(dest_path);

                char full_path[BUFFER_SIZE];
                snprintf(full_path, sizeof(full_path), "%s/%s", dest_path, filename);

                FILE *fp = fopen(full_path, "wb");
                if (fp == NULL) {
                    perror("Error opening file");
                    strcpy(buffer, "Error saving file\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending file save error message to client");
                    }
                    continue;
                }

                // Read and write file content
                while ((n = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
                    printf("txt received %d bytes\n", n);
                    if (fwrite(buffer, sizeof(char), n, fp) != n) {
                        perror("Error writing to file");
                        break;
                    }
                    if (n < BUFFER_SIZE) break; // End of file
                }
                if (ferror(fp)) {
                    perror("Error writing to file");
                }
                fclose(fp);
                printf("Text file saved: %s\n", full_path);
                strcpy(buffer, "Text file saved successfully\n");

            } else if (strcmp(command, "dfile") == 0) {
                printf("Sending Text file: %s\n", filename);
                char *full_path = map_path(filename);
                if (full_path == NULL) {
                    strcpy(buffer, "Error mapping path\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending path mapping error to client");
                    }
                    continue;
                }
                printf("Mapped path: %s\n", full_path);
                int fd = open(full_path, O_RDONLY);
                if (fd == -1) {
                    perror("Error opening file");
                    strcpy(buffer, "Error reading file\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending file read error message to client");
                    }
                } else {
                    while ((n = read(fd, buffer, BUFFER_SIZE)) > 0) {
                        if (write(client_socket, buffer, n) == -1) {
                            perror("Error sending file to client");
                            break;
                        }
                    }
                    if (n < 0) {
                        perror("Error reading file for client");
                    }
                    close(fd);
                }
                free(full_path);
                close(client_socket);
                        } else if (strcmp(command, "rmfile") == 0) {
                printf("Deleting file: %s\n", filename);
                char full_path[BUFFER_SIZE];
                snprintf(full_path, sizeof(full_path), "%s/%s", BASE_DIR, filename);

                printf("Full path for deletion: %s\n", full_path); // Debugging print statement

                if (remove(full_path) == 0) {
                    printf("File %s deleted successfully\n", full_path);
                    strcpy(buffer, "File deleted successfully\n");
                } else {
                    perror("Error deleting file");
                    strcpy(buffer, "Error deleting file\n");
                }
                if (write(client_socket, buffer, strlen(buffer)) == -1) {
                    perror("Error sending file delete message to client");
                }
                close(client_socket);

            } else if (strcmp(command, "dtar") == 0) {
                printf("Creating tarball for .txt files\n");

                char tar_file[BUFFER_SIZE];
                snprintf(tar_file, sizeof(tar_file), "%s/txtfiles.tar", BASE_DIR);

                // Ensure the path in the `find` command points to the directory where the .txt files are stored
                int ret = system("find /home/sethi83/ASP_Project_Main/Server/StextServer/~stext -name '*.txt' | tar -cvf /home/sethi83/ASP_Project_Main/Server/StextServer/txtfiles.tar -T -");
                if (ret != 0) {
                    perror("Error creating tar file");
                    strcpy(buffer, "Error creating tar file\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending tar file creation error to client");
                    }
                    close(client_socket);
                    return;
                }

                printf("Tarball created at: %s\n", tar_file);

                // Sending the tarball back to Smain
                int fd = open(tar_file, O_RDONLY);
                if (fd == -1) {
                    perror("Error opening tar file");
                    strcpy(buffer, "Error reading tar file\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending tar file read error to client");
                    }
                } else {
                    while ((n = read(fd, buffer, BUFFER_SIZE)) > 0) {
                        if (write(client_socket, buffer, n) != n) {
                            perror("Error sending tar file");
                            break;
                        }
                        printf("Sent %d bytes of tar file to Smain\n", n);  // Debug print
                    }
                    if (n < 0) {
                        perror("Error reading tar file for client");
                    }
                    close(fd);
                    printf("Tarball sent to Smain\n");
                }

            } else {
                strcpy(buffer, "Unknown command\n");
                if (write(client_socket, buffer, strlen(buffer)) == -1) {
                    perror("Error sending unknown command message to client");
                }
            }
        } else {
            printf("Invalid command received.\n");
            strcpy(buffer, "Invalid command\n");
            if (write(client_socket, buffer, strlen(buffer)) == -1) {
                perror("Error sending invalid command message to client");
            }
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
        perror("Error in socket creation");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");

    // Set the SO_REUSEADDR option to allow port reuse
    int opt = 1;
    if (setsockopt(server_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) == -1) {
        perror("setsockopt failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    bzero(&server_addr, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = htonl(INADDR_ANY);
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr*)&server_addr, sizeof(server_addr)) != 0) {
        perror("Socket bind failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Socket binded successfully\n");

    if (listen(server_socket, 5) != 0) {
        perror("Listen failed");
        close(server_socket);
        exit(EXIT_FAILURE);
    }
    printf("Server listening...\n");

    len = sizeof(client_addr);

    while (1) {
        client_socket = accept(server_socket, (struct sockaddr*)&client_addr, &len);
        if (client_socket < 0) {
            perror("Server accept failed");
            continue;
        }
        printf("Server accepted the Smain request\n");

        if (fork() == 0) {
            close(server_socket);
            handle_client(client_socket);
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}