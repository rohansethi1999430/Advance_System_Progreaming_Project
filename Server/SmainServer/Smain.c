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

#define PORT 8080
#define BUFFER_SIZE 1024
#define PDF_SERVER_PORT 8091
#define TEXT_SERVER_PORT 8081

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
        // Compare the substring with the result
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

void forward_file_to_server(const char *filename, const char *dest_path, const char *server_ip, int server_port) {
    int sock;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    FILE *fp;

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

    // Send file content
    fp = fopen(filename, "r");
    if (fp == NULL) {
        perror("Error opening file");
        close(sock);
        return;
    }
    while (fgets(buffer, BUFFER_SIZE, fp) != NULL) {
        send(sock, buffer, strlen(buffer), 0);
    }
    fclose(fp);
    close(sock);
}

void prcclient(int client_socket) {
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
        char *dest_path = strtok(NULL, " ");

        if (command && filename && dest_path) {
            printf("Received command: %s, filename: %s, destination: %s\n", command, filename, dest_path);

            if (strcmp(command, "ufile") == 0) {
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
                    forward_file_to_server(filename, dest_path, "127.0.0.1", PDF_SERVER_PORT);
                    strcpy(buffer, "PDF file forwarded to Spdf server\n");
                } else if (strstr(filename, ".txt") != NULL) {
                    dest_path = replace_substring(dest_path, "~smain", "~stext");
                    forward_file_to_server(filename, dest_path, "127.0.0.1", TEXT_SERVER_PORT);
                    strcpy(buffer, "Text file forwarded to Stext server\n");
                } else {
                    strcpy(buffer, "Unsupported file type\n");
                }

                // Send response to client and break out of loop after successful handling
                write(client_socket, buffer, strlen(buffer));
                break; // Exit the loop after processing the command
            } else {
                strcpy(buffer, "Unknown command\n");
                write(client_socket, buffer, strlen(buffer));  // Send response to client
            }
        } else {
            printf("Invalid command received.\n");
            strcpy(buffer, "Invalid command\n");
            write(client_socket, buffer, strlen(buffer));
        }
    }

    close(client_socket);
}


int main() {
    int server_socket, client_socket, len;
    struct sockaddr_in server_addr, client_addr;

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
