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

#define PORT 9080
#define BUFFER_SIZE 1024
#define PDF_SERVER_PORT 8091
#define TEXT_SERVER_PORT 8801

// Function declarations
void create_directory(const char *path);
void forward_file_to_server(int client_socket, const char *filename, const char *dest_path, const char *server_ip, int server_port);
void retrieve_file_from_server(int client_socket, const char *filename, const char *server_ip, int server_port);
void handle_rmfile_request(int client_socket, const char *filename, const char *server_ip, int server_port);
void retrieve_file_list_from_server(const char *pathname, const char *server_ip, int server_port, char *combined_buffer, char **file_list, int *file_count);

// Function to replace ~smain with ~spdf or ~stext in the destination path
char* replace_substring(const char* str, const char* old_sub, const char* new_sub) {
    char* result;
    int i, count = 0;
    int newlen = strlen(new_sub);
    int oldlen = strlen(old_sub);

    // Counting the number of times the old substring occurs in the string
    for (i = 0; str[i] != '\0'; i++) {
        if (strstr(&str[i], old_sub) == &str[i]) {
            count++;
            i += oldlen - 1;
        }
    }

    // Allocating memory for the new string
    result = (char*)malloc(i + count * (newlen - oldlen) + 1);

    if (result == NULL) {
        perror("Error allocating memory");
        exit(EXIT_FAILURE);
    }

    i = 0;
    while (*str) {
        if (strstr(str, old_sub) == str) {
            strcpy(&result[i], new_sub);
            i += newlen;
            str += oldlen;
        } else {
            result[i++] = *str++;
        }
    }

    result[i] = '\0';
    return result;
}

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

// Function to forward the file to the respective server
void forward_file_to_server(int client_socket, const char *filename, const char *dest_path, const char *server_ip, int server_port) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int n;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        close(sock);
        return;
    }

    // Send the file path to the respective server
    snprintf(buffer, sizeof(buffer), "ufile %s %s", filename, dest_path);
    send(sock, buffer, strlen(buffer), 0);

    // Forward the file content received from the client to the respective server
    while ((n = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
        printf("Smain read %d bytes\n", n);
        send(sock, buffer, n, 0);
        printf("Smain sent %d bytes to server\n", n);
        if (n < BUFFER_SIZE) break;  // End of file
    }

    shutdown(sock, SHUT_WR);  // Signal end of transmission
    close(sock);
}

// Function to forward the download request to the respective server and receive the file
void retrieve_file_from_server(int client_socket, const char *filename, const char *server_ip, int server_port) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int n;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        close(sock);
        return;
    }

    // Send the dfile command to the respective server
    snprintf(buffer, sizeof(buffer), "dfile %s", filename);
    send(sock, buffer, strlen(buffer), 0);

    // Receive the file content from the server and send it to the client
    while ((n = read(sock, buffer, BUFFER_SIZE)) > 0) {
        write(client_socket, buffer, n);
    }

    close(sock);
}

// Function to handle the rmfile request
void handle_rmfile_request(int client_socket, const char *filename, const char *server_ip, int server_port) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int n;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        close(sock);
        return;
    }

    // Send the rmfile command to the respective server
    snprintf(buffer, sizeof(buffer), "rmfile %s", filename);
    send(sock, buffer, strlen(buffer), 0);

    // Receive the response from the server and send it to the client
    while ((n = read(sock, buffer, BUFFER_SIZE)) > 0) {
        write(client_socket, buffer, n);
    }

    close(sock);
}

// Function to handle the client request
void prcclient(int client_socket) {
    char buffer[BUFFER_SIZE];
    char combined_buffer[BUFFER_SIZE * 10];  // Buffer to store the combined response
    char *file_list[BUFFER_SIZE * 10];  // Array to track unique file names
    int file_count = 0;  // Counter for files added to the list
    combined_buffer[0] = '\0';  // Initialize the combined buffer
    int n;

    while (1) {
        // Reset the file list and count for each new command
        for (int i = 0; i < file_count; i++) {
            free(file_list[i]);
        }
        file_count = 0;
        bzero(combined_buffer, sizeof(combined_buffer));
        
        bzero(buffer, BUFFER_SIZE);
        n = read(client_socket, buffer, BUFFER_SIZE);
        if (n <= 0) {
            printf("Client disconnected\n");
            break;
        }
        printf("Client: %s\n", buffer);

        char *command = strtok(buffer, " ");
        char *filename = strtok(NULL, " ");
        char *dest_path = strtok(NULL, " ");

        // Log the received command, filename, and destination path
        printf("Command: %s, Filename: %s, Dest Path: %s\n", 
               command ? command : "NULL", 
               filename ? filename : "NULL", 
               dest_path ? dest_path : "NULL");

        if (command && filename) {
            printf("Received command: %s, filename: %s\n", command, filename);

            if (strcmp(command, "display") == 0) {
                // Handle the display command
                char full_path[BUFFER_SIZE];
                snprintf(full_path, sizeof(full_path), "%s", filename);

                // List files in the specified directory on Smain
                printf("Listing files in: %s\n", full_path);
                struct dirent *entry;
                DIR *dp = opendir(full_path);

                if (dp == NULL) {
                    perror("Error opening directory");
                    strcpy(buffer, "Error opening directory\n");
                    write(client_socket, buffer, strlen(buffer));
                    continue;
                }

                // Collect .c files from Smain
                while ((entry = readdir(dp))) {
                                        if (entry->d_type == DT_REG && strstr(entry->d_name, ".c")) {
                        int is_duplicate = 0;

                        // Check if the file is already in the list
                        for (int i = 0; i < file_count; i++) {
                            if (strcmp(file_list[i], entry->d_name) == 0) {
                                is_duplicate = 1;
                                break;
                            }
                        }

                        // If the file is not a duplicate, add it to the list
                        if (!is_duplicate) {
                            file_list[file_count] = strdup(entry->d_name);
                            snprintf(buffer, sizeof(buffer), "%s\n", entry->d_name);
                            strcat(combined_buffer, buffer);  // Append to combined buffer
                            file_count++;
                        }
                    }
                }
                closedir(dp);

                // Request .pdf files from Spdf
                retrieve_file_list_from_server(full_path, "127.0.0.1", PDF_SERVER_PORT, combined_buffer, file_list, &file_count);

                // Clear buffer before requesting from Stext
                bzero(buffer, BUFFER_SIZE);

                // Request .txt files from Stext
                retrieve_file_list_from_server(full_path, "127.0.0.1", TEXT_SERVER_PORT, combined_buffer, file_list, &file_count);

                // Send the combined response to the client
                write(client_socket, combined_buffer, strlen(combined_buffer));

                // Indicate end of file list to the client
                strcpy(buffer, "End of file list\n");
                write(client_socket, buffer, strlen(buffer));
            } 
            // Handle other commands like ufile, dfile, rmfile...
            else if (strcmp(command, "ufile") == 0) {
                if (dest_path == NULL) {
                    strcpy(buffer, "Destination path missing\n");
                    write(client_socket, buffer, strlen(buffer));
                    continue;
                }

                if (strstr(filename, ".c") != NULL) {
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

                    while ((n = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
                        fwrite(buffer, sizeof(char), n, fp);
                        if (n < BUFFER_SIZE) break;
                    }
                    fclose(fp);
                    printf("C file saved: %s\n", full_path);
                    strcpy(buffer, "File saved successfully\n");
                } else if (strstr(filename, ".pdf") != NULL) {
                    dest_path = replace_substring(dest_path, "~smain", "~spdf");
                    forward_file_to_server(client_socket, filename, dest_path, "127.0.0.1", PDF_SERVER_PORT);
                    strcpy(buffer, "PDF file forwarded to Spdf server\n");
                } else if (strstr(filename, ".txt") != NULL) {
                    dest_path = replace_substring(dest_path, "~smain", "~stext");
                    forward_file_to_server(client_socket, filename, dest_path, "127.0.0.1", TEXT_SERVER_PORT);
                    strcpy(buffer, "Text file forwarded to Stext server\n");
                } else {
                    strcpy(buffer, "Unsupported file type\n");
                }

                // Send response to client after successful handling
                write(client_socket, buffer, strlen(buffer));

            } else if (strcmp(command, "dfile") == 0) {
                if (strstr(filename, ".c") != NULL) {
                    printf("Sending .c file: %s\n", filename);
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
                } else if (strstr(filename, ".pdf") != NULL) {
                    printf("Retrieving .pdf file from Spdf server\n");
                    retrieve_file_from_server(client_socket, filename, "127.0.0.1", PDF_SERVER_PORT);
                } else if (strstr(filename, ".txt") != NULL) {
                    printf("Retrieving .txt file from Stext server\n");
                    retrieve_file_from_server(client_socket, filename, "127.0.0.1", TEXT_SERVER_PORT);
                } else {
                    strcpy(buffer, "Unsupported file type\n");
                    write(client_socket, buffer, strlen(buffer));
                }
            } else if (strncmp(command, "rmfile", 6) == 0) {
                if (strstr(filename, ".c") != NULL) {
                    printf("Deleting .c file: %s\n", filename);
                    char full_path[BUFFER_SIZE];
                    snprintf(full_path, sizeof(full_path), "/home/chauha5a/ASP_Project_Main/Server/SmainServer/%s", filename);

                    printf("Attempting to delete file: %s\n", full_path);

                    if (remove(full_path) == 0) {
                        printf("File %s deleted successfully\n", full_path);
                        strcpy(buffer, "File deleted successfully\n");
                    } else {
                        perror("Error deleting file");
                        strcpy(buffer, "Error deleting file\n");
                    }
                    write(client_socket, buffer, strlen(buffer));
                } else if (strstr(filename, ".pdf") != NULL) {
                    dest_path = replace_substring(filename, "~smain", "~spdf");
                    printf("Forwarding delete request for .pdf file: %s to Spdf server\n", dest_path);
                    handle_rmfile_request(client_socket, dest_path, "127.0.0.1", PDF_SERVER_PORT);
                } else if (strstr(filename, ".txt") != NULL) {
                    dest_path = replace_substring(filename, "~smain", "~stext");
                    printf("Forwarding delete request for .txt file: %s to Stext server\n", dest_path);
                    handle_rmfile_request(client_socket, dest_path, "127.0.0.1", TEXT_SERVER_PORT);
                } else {
                    strcpy(buffer, "Unsupported file type for deletion\n");
                    write(client_socket, buffer, strlen(buffer));
                }
            } else {
                strcpy(buffer, "Unknown command\n");
                write(client_socket, buffer, strlen(buffer));
            }
        } else {
            printf("Invalid command received or filename missing.\n");
            strcpy(buffer, "Invalid command\n");
            write(client_socket, buffer, strlen(buffer));
        }
    }

    // Free memory allocated for the file list before closing the client connection
    for (int i = 0; i < file_count; i++) {
        free(file_list[i]);
    }

    close(client_socket);
}

// Function to retrieve file list from another server (for .pdf and .txt files)
// Function to retrieve file list from another server (for .pdf and .txt files)
void retrieve_file_list_from_server(const char *pathname, const char *server_ip, int server_port, char *combined_buffer, char **file_list, int *file_count) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    int n;

    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        perror("Socket creation failed");
        return;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(server_port);
    server_addr.sin_addr.s_addr = inet_addr(server_ip);

    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
        perror("Connect failed");
        close(sock);  // Ensure socket is closed on connect failure
        return;
    }

    printf("Sending display command to server: %s with path: %s\n", server_ip, pathname ? pathname : "NULL");

    if (!pathname || strlen(pathname) == 0) {
        snprintf(buffer, sizeof(buffer), "display");
    } else {
        snprintf(buffer, sizeof(buffer), "display %s", pathname);
    }

    send(sock, buffer, strlen(buffer), 0);

    // Receive the file list from the server and append it to the combined buffer
    while ((n = read(sock, buffer, BUFFER_SIZE)) > 0) {
        buffer[n] = '\0';  // Null-terminate the received buffer
        printf("Received from server %s: %s\n", server_ip, buffer);

        // Check for duplicates before appending to combined buffer
        char *token = strtok(buffer, "\n");
        while (token != NULL) {
            int is_duplicate = 0;
            for (int i = 0; i < *file_count; i++) {
                if (strcmp(file_list[i], token) == 0) {
                    is_duplicate = 1;
                    break;
                }
            }

            if (!is_duplicate) {
                file_list[*file_count] = strdup(token);
                strcat(combined_buffer, token);
                strcat(combined_buffer, "\n");
                (*file_count)++;
            }

            token = strtok(NULL, "\n");
        }
    }

    close(sock);  // Close socket after processing the request
}

// Main function to set up the server
int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t len;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        printf("Error in socket creation\n");
        exit(1);
    }
    printf("Socket created successfully\n");

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
        printf("Server accepted the client\n");

        if (fork() == 0) {
            close(server_socket);
            prcclient(client_socket);
            exit(0);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}