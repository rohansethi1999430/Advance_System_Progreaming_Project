#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <fcntl.h>

//some global variables


#define PORT 8081
#define BUFFER_SIZE 1024

void handle_smain(int sock);

int main() {
    //server socket
    int server_sock, smain_sock;
    struct sockaddr_in server_addr, smain_addr;
    socklen_t addr_size;

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
        smain_sock = accept(server_sock, (struct sockaddr*)&smain_addr, &addr_size);
        if (smain_sock < 0) {
            perror("Accept failed");
            exit(EXIT_FAILURE);
        }

        handle_smain(smain_sock);
        close(smain_sock);
    }

    close(server_sock);
    return 0;
}

void handle_smain(int sock) {
    char buffer[BUFFER_SIZE];
    // Handle communication with Smain here
    // Example: receiving and storing PDF files
    read(sock, buffer, BUFFER_SIZE);
    // Process the command
}