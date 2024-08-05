#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <netinet/in.h>

//defining the global variables
#define PORT 8080
#define BUFFER_SIZE 1024

void prcclient(int client_sock);

int main() {
    int server_sock, client_sock;
    struct sockaddr_in server_addr, client_addr;
    socklen_t addr_size;
    pid_t child_pid;

    server_sock = socket(AF_INET, SOCK_STREAM, 0);
    if (server_sock < 0) {
        perror("Socket creation failed");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = INADDR_ANY;

    if (bind(server_sock, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        perror("Bind failed");
        exit(EXIT_FAILURE);
    }

    if (listen(server_sock, 10) < 0) {
        perror("Listen failed");
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_sock = accept(server_sock, (struct sockaddr*)&client_addr, &addr_size);
        if (client_sock < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        child_pid = fork();
        if (child_pid == 0) {
            close(server_sock);
            prcclient(client_sock);
            close(client_sock);
            exit(0);
        } else if (child_pid > 0) {
            close(client_sock);
        } else {
            perror("Fork failed");
        }
    }

    close(server_sock);
    return 0;
}

void prcclient(int client_sock) {
    char buffer[BUFFER_SIZE];
    while (1) {
        bzero(buffer, BUFFER_SIZE);
        read(client_sock, buffer, BUFFER_SIZE);
        if (strcmp(buffer, "exit") == 0) {
            break;
        }
        // Handle commands here
        // Example: ufile, dfile, rmfile, dtar, display
        write(client_sock, buffer, strlen(buffer));
    }
}