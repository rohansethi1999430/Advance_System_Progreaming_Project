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
    if (send(sock, buffer, strlen(buffer), 0) == -1) {
        perror("Error sending file path to server");
        close(sock);
        return;
    }

    // Forward the file content received from the client to the respective server
    while ((n = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
        printf("Smain read %d bytes\n", n);
        if (send(sock, buffer, n, 0) == -1) {
            perror("Error sending file content to server");
            break;
        }
        printf("Smain sent %d bytes to server\n", n);
        if (n < BUFFER_SIZE) break;  // End of file
    }

    if (n < 0) {
        perror("Error reading from client socket");
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
    if (send(sock, buffer, strlen(buffer), 0) == -1) {
        perror("Error sending dfile command to server");
        close(sock);
        return;
    }

    // Receive the file content from the server and send it to the client
    while ((n = read(sock, buffer, BUFFER_SIZE)) > 0) {
        if (write(client_socket, buffer, n) == -1) {
            perror("Error sending file content to client");
            break;
        }
    }

    if (n < 0) {
        perror("Error reading from server socket");
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
    if (send(sock, buffer, strlen(buffer), 0) == -1) {
        perror("Error sending rmfile command to server");
        close(sock);
        return;
    }

    // Receive the response from the server and send it to the client
    while ((n = read(sock, buffer, BUFFER_SIZE)) > 0) {
        if (write(client_socket, buffer, n) == -1) {
            perror("Error sending response to client");
            break;
        }
    }

    if (n < 0) {
        perror("Error reading from server socket");
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
                struct stat entry_stat;
                DIR *dp = opendir(full_path);

                if (dp == NULL) {
                    perror("Error opening directory");
                    strcpy(buffer, "Error opening directory\n");
                    write(client_socket, buffer, strlen(buffer));
                    continue;
                }

                // Collect .c files from Smain
                while ((entry = readdir(dp))) {
                    char entry_path[BUFFER_SIZE];
                    snprintf(entry_path, sizeof(entry_path), "%s/%s", full_path, entry->d_name);
                    
                    // Use stat to get file information
                    if (stat(entry_path, &entry_stat) == 0 && S_ISREG(entry_stat.st_mode) && strstr(entry->d_name, ".c")) {
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
                            if (file_list[file_count] == NULL) {
                                perror("Error duplicating file name");
                                break;
                            }
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
                if (write(client_socket, combined_buffer, strlen(combined_buffer)) == -1) {
                    perror("Error sending file list to client");
                }

                // Indicate end of file list to the client
                strcpy(buffer, "End of file list\n");
                if (write(client_socket, buffer, strlen(buffer)) == -1) {
                    perror("Error sending end of file list to client");
                }
            } 
            // Handle other commands like ufile, dfile, rmfile...
            else if (strcmp(command, "ufile") == 0) {
                if (dest_path == NULL) {
                    strcpy(buffer, "Destination path missing\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending missing destination path message to client");
                    }
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
                        if (write(client_socket, buffer, strlen(buffer)) == -1) {
                            perror("Error sending file save error message to client");
                        }
                        continue;
                    }

                    while ((n = read(client_socket, buffer, BUFFER_SIZE)) > 0) {
                        if (fwrite(buffer, sizeof(char), n, fp) != n) {
                            perror("Error writing to file");
                            break;
                        }
                        if (n < BUFFER_SIZE) break;
                    }
                    if (n < 0) {
                        perror("Error reading from client socket");
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
                if (write(client_socket, buffer, strlen(buffer)) == -1) {
                    perror("Error sending response to client");
                }

            } else if (strcmp(command, "dfile") == 0) {
                if (strstr(filename, ".c") != NULL) {
                    printf("Sending .c file: %s\n", filename);
                    int fd = open(filename, O_RDONLY);
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
                } else if (strstr(filename, ".pdf") != NULL) {
                    printf("Retrieving .pdf file from Spdf server\n");
                    retrieve_file_from_server(client_socket, filename, "127.0.0.1", PDF_SERVER_PORT);
                } else if (strstr(filename, ".txt") != NULL) {
                    printf("Retrieving .txt file from Stext server\n");
                    retrieve_file_from_server(client_socket, filename, "127.0.0.1", TEXT_SERVER_PORT);
                } else {
                    strcpy(buffer, "Unsupported file type\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending unsupported file type message to client");
                    }
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
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending file delete message to client");
                    }
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
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending unsupported file type for deletion message to client");
                    }
                }
            } else if (strcmp(command, "dtar") == 0) {
                char tar_file[BUFFER_SIZE];
                char filetype[10];
                snprintf(filetype, sizeof(filetype), "%s", filename); // filename holds the filetype in this case

                if (strcmp(filetype, ".c") == 0) {
                    printf("Creating tarball for .c files\n");

                    snprintf(tar_file, sizeof(tar_file), "/home/chauha5a/ASP_Project_Main/Server/SmainServer/cfiles.tar");

                                        // Create the tarball of all .c files in the ~smain directory
                    int ret = system("find /home/chauha5a/ASP_Project_Main/Server/SmainServer/~smain -name '*.c' | tar -cvf /home/chauha5a/ASP_Project_Main/Server/SmainServer/cfiles.tar -T -");
                    if (ret == -1 || WEXITSTATUS(ret) != 0) {
                        perror("Error creating tarball for .c files");
                        strcpy(buffer, "Error creating tarball for .c files\n");
                        if (write(client_socket, buffer, strlen(buffer)) == -1) {
                            perror("Error sending tarball creation error to client");
                        }
                        continue;
                    }
                    printf("Created tar file for .c files: %s\n", tar_file);
                } else if (strcmp(filetype, ".pdf") == 0) {
                    snprintf(tar_file, sizeof(tar_file), "/home/chauha5a/pdffiles.tar");
                    int sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (sock == -1) {
                        perror("Socket creation failed");
                        strcpy(buffer, "Error creating socket\n");
                        if (write(client_socket, buffer, strlen(buffer)) == -1) {
                            perror("Error sending socket creation error to client");
                        }
                        continue;
                    }
                    struct sockaddr_in server_addr;
                    server_addr.sin_family = AF_INET;
                    server_addr.sin_port = htons(PDF_SERVER_PORT);
                    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

                    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
                        perror("Connect failed");
                        close(sock);
                        strcpy(buffer, "Error connecting to PDF server\n");
                        if (write(client_socket, buffer, strlen(buffer)) == -1) {
                            perror("Error sending connection error to client");
                        }
                        continue;
                    }

                    // Requesting tar file from Spdf
                    snprintf(buffer, sizeof(buffer), "dtar .pdf");
                    if (send(sock, buffer, strlen(buffer), 0) == -1) {
                        perror("Error sending dtar command to PDF server");
                        close(sock);
                        continue;
                    }

                    // Opening a file to store the tar received from Spdf
                    int fd = open(tar_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) {
                        perror("Error opening file");
                        close(sock);
                        strcpy(buffer, "Error opening tar file\n");
                        if (write(client_socket, buffer, strlen(buffer)) == -1) {
                            perror("Error sending file opening error to client");
                        }
                        continue;
                    }

                    // Receiving the tar file from Spdf
                    while ((n = read(sock, buffer, BUFFER_SIZE)) > 0) {
                        if (write(fd, buffer, n) != n) {
                            perror("Error writing to tar file");
                            break;
                        }
                        printf("Received %d bytes of tar file from Spdf\n", n);
                    }
                    if (n < 0) {
                        perror("Error reading tar file from Spdf");
                    }

                    close(fd);
                    close(sock);
                    printf("Received tar file for .pdf files: %s\n", tar_file);

                } else if (strcmp(filetype, ".txt") == 0) {
                    snprintf(tar_file, sizeof(tar_file), "/home/chauha5a/txtfiles.tar");
                    int sock = socket(AF_INET, SOCK_STREAM, 0);
                    if (sock == -1) {
                        perror("Socket creation failed");
                        strcpy(buffer, "Error creating socket\n");
                        if (write(client_socket, buffer, strlen(buffer)) == -1) {
                            perror("Error sending socket creation error to client");
                        }
                        continue;
                    }
                    struct sockaddr_in server_addr;
                    server_addr.sin_family = AF_INET;
                    server_addr.sin_port = htons(TEXT_SERVER_PORT);
                    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1");

                    if (connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr)) == -1) {
                        perror("Connect failed");
                        close(sock);
                        strcpy(buffer, "Error connecting to text server\n");
                        if (write(client_socket, buffer, strlen(buffer)) == -1) {
                            perror("Error sending connection error to client");
                        }
                        continue;
                    }

                    // Requesting tar file from Stext
                    snprintf(buffer, sizeof(buffer), "dtar .txt");
                    if (send(sock, buffer, strlen(buffer), 0) == -1) {
                        perror("Error sending dtar command to text server");
                        close(sock);
                        continue;
                    }

                    // Opening a file to store the tar received from Stext
                    int fd = open(tar_file, O_WRONLY | O_CREAT | O_TRUNC, 0644);
                    if (fd == -1) {
                        perror("Error opening file");
                        close(sock);
                        strcpy(buffer, "Error opening tar file\n");
                        if (write(client_socket, buffer, strlen(buffer)) == -1) {
                            perror("Error sending file opening error to client");
                        }
                        continue;
                    }

                    // Receiving the tar file from Stext
                    while ((n = read(sock, buffer, BUFFER_SIZE)) > 0) {
                        if (write(fd, buffer, n) != n) {
                            perror("Error writing to tar file");
                            break;
                        }
                        printf("Received %d bytes of tar file from Stext\n", n);
                    }
                    if (n < 0) {
                        perror("Error reading tar file from Stext");
                    }

                    close(fd);
                    close(sock);
                    printf("Received tar file for .txt files: %s\n", tar_file);
                } else {
                    strcpy(buffer, "Unsupported file type\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending unsupported file type message to client");
                    }
                    return;
                }

                // Send the tar file to the client
                FILE *fp = fopen(tar_file, "rb");
                if (fp == NULL) {
                    perror("Error opening tar file");
                    strcpy(buffer, "Error reading tar file\n");
                    if (write(client_socket, buffer, strlen(buffer)) == -1) {
                        perror("Error sending tar file read error to client");
                    }
                } else {
                    while ((n = fread(buffer, sizeof(char), BUFFER_SIZE, fp)) > 0) {
                        if (write(client_socket, buffer, n) != n) {
                            perror("Error sending tar file to client");
                            break;
                        }
                        printf("Sent %d bytes of tar file to client\n", n); // Debug print
                    }
                    if (ferror(fp)) {
                        perror("Error reading tar file");
                    }
                    fclose(fp);
                    printf("Tar file %s sent to client\n", tar_file);
                }
            } else {
                strcpy(buffer, "Unknown command\n");
                if (write(client_socket, buffer, strlen(buffer)) == -1) {
                    perror("Error sending unknown command message to client");
                }
            }
        } else {
            printf("Invalid command received or filename missing.\n");
            strcpy(buffer, "Invalid command\n");
            if (write(client_socket, buffer, strlen(buffer)) == -1) {
                perror("Error sending invalid command message to client");
            }
        }
    }

    // Free memory allocated for the file list before closing the client connection
    for (int i = 0; i < file_count; i++) {
        free(file_list[i]);
    }

    close(client_socket);
}

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

    if (send(sock, buffer, strlen(buffer), 0) == -1) {
        perror("Error sending display command to server");
        close(sock);
        return;
    }

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
                if (file_list[*file_count] == NULL) {
                    perror("Error duplicating file name");
                    break;
                }
                strcat(combined_buffer, token);
                strcat(combined_buffer, "\n");
                (*file_count)++;
            }

            token = strtok(NULL, "\n");
        }
    }

    if (n < 0) {
        perror("Error reading file list from server");
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
        perror("Error in socket creation");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");

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
            close(server_socket);
            exit(EXIT_FAILURE);
        }
        printf("Server accepted the client\n");

        if (fork() == 0) {
            close(server_socket);
            prcclient(client_socket);
            close(client_socket);
            exit(EXIT_SUCCESS);
        } else {
            close(client_socket);
        }
    }

    close(server_socket);
    return 0;
}