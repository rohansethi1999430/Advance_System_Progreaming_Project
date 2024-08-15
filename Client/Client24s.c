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
char *extract_filename(const char *path)
{
    char *filename = strrchr(path, '/');
    if (filename == NULL)
    {
        return strdup(path); // No '/' found, return the original path as filename
    }
    else
    {
        return strdup(filename + 1); // Return the string after the last '/'
    }
}

int main()
{
    int client_socket;
    struct sockaddr_in server_addr;
    char buffer[BUFFER_SIZE];
    char command[BUFFER_SIZE];
    FILE *fp;
    int n;

    client_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (client_socket < 0)
    {
        perror("Error in socket creation");
        exit(EXIT_FAILURE);
    }
    printf("Socket created successfully\n");

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Ensure this matches the server's IP

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        perror("Connection to Smain failed");
        close(client_socket);
        exit(EXIT_FAILURE);
    }
    printf("Connected to Smain\n");

    while (1)
    {
        bzero(buffer, BUFFER_SIZE);
        bzero(command, BUFFER_SIZE);

        printf("Enter command: ");
        if (fgets(command, BUFFER_SIZE, stdin) == NULL)
        {
            perror("Error reading command");
            continue;
        }
        command[strcspn(command, "\n")] = 0; // Remove newline character
        printf("Sending command: %s\n", command);

        if (write(client_socket, command, strlen(command)) < 0)
        {
            perror("Error sending command to server");
            continue;
        }

        // Handle 'ufile' command
        if (strncmp("ufile", command, 5) == 0)
        {
            char *filename = strtok(command + 6, " "); // Get filename after 'ufile '
            if (filename == NULL)
            {
                fprintf(stderr, "Error: No filename provided\n");
                continue;
            }

            fp = fopen(filename, "r");
            if (fp == NULL)
            {
                perror("Error opening file");
                continue;
            }
            printf("Sending file content of %s\n", filename);

            // Send file content to the server
            while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0)
            {
                if (write(client_socket, buffer, n) < 0)
                {
                    perror("Error sending file content to server");
                    break;
                }
            }

            if (ferror(fp))
            {
                perror("Error reading from file");
            }

            fclose(fp);
        }

        // Handle 'dfile' command
        if (strncmp("dfile", command, 5) == 0)
        {
            char *filepath = strtok(command + 6, " "); // Get file path after 'dfile '
            if (filepath != NULL)
            {
                char *filename = extract_filename(filepath);
                printf("Receiving file: %s\n", filename);
                fp = fopen(filename, "w");
                if (fp == NULL)
                {
                    perror("Error creating file");
                    free(filename);
                    continue;
                }

                int bytes_received;
                while ((bytes_received = read(client_socket, buffer, BUFFER_SIZE)) > 0)
                {
                    if (fwrite(buffer, sizeof(char), bytes_received, fp) != bytes_received)
                    {
                        perror("Error writing to file");
                        break;
                    }

                    if (bytes_received < BUFFER_SIZE)
                    {
                        printf("End of file received.\n");
                        break; // End of file or transmission
                    }
                }

                if (bytes_received < 0)
                {
                    perror("Error receiving file from server");
                }

                fclose(fp);
                printf("File %s received successfully\n", filename);
                free(filename);
            }
            else
            {
                fprintf(stderr, "Error: No file path provided\n");
            }
            continue;
        }

        // Handle 'rmfile' command
        if (strncmp("rmfile", command, 6) == 0)
        {
            char *filepath = strtok(command + 7, " "); // Get file path after 'rmfile '
            if (filepath != NULL)
            {
                printf("Sending file removal request for: %s\n", filepath);
            }
            else
            {
                fprintf(stderr, "Error: No file path provided\n");
            }
        }

        // Handle 'dtar' command
        if (strncmp("dtar", command, 4) == 0)
        {
            char *filetype = strtok(command + 5, " "); // Get file type after 'dtar '
            if (filetype != NULL)
            {
                char tar_filename[BUFFER_SIZE];
                snprintf(tar_filename, sizeof(tar_filename), "%sfiles.tar", filetype + 1); // Creates "cfiles.tar", "pdffiles.tar", or "txtfiles.tar"
                printf("Receiving tarball: %s\n", tar_filename);

                // Open the file for writing in binary mode
                fp = fopen(tar_filename, "wb");
                if (fp == NULL)
                {
                    perror("Error creating tar file");
                    continue;
                }

                int bytes_received;
                while ((bytes_received = read(client_socket, buffer, BUFFER_SIZE)) > 0)
                {
                    printf("Received %d bytes from Smain\n", bytes_received); // Debug print
                    if (fwrite(buffer, sizeof(char), bytes_received, fp) != bytes_received)
                    {
                        perror("Error writing to tar file");
                        break;
                    }

                    if (bytes_received < BUFFER_SIZE)
                    {
                        printf("End of tar file received from Smain\n");
                        break; // End of file or transmission
                    }
                }

                if (bytes_received < 0)
                {
                    perror("Error receiving tar file from server");
                }

                fclose(fp);
                printf("Tarball %s received successfully\n", tar_filename);
            }
            else
            {
                fprintf(stderr, "Error: No file type provided\n");
            }
            continue;
        }

        // Handle 'display' command
        if (strncmp("display", command, 7) == 0)
        {
            printf("Receiving file list:\n");
            int bytes_received;
            while ((bytes_received = read(client_socket, buffer, BUFFER_SIZE)) > 0)
            {
                buffer[bytes_received] = '\0'; // Null-terminate the buffer
                printf("%s", buffer);
                if (bytes_received < BUFFER_SIZE)
                {
                    break; // End of transmission
                }
            }

            if (bytes_received < 0)
            {
                perror("Error receiving file list from server");
            }
            continue;
        }

        // Expect a response from the server
        int bytes_received = read(client_socket, buffer, BUFFER_SIZE);
        if (bytes_received > 0)
        {
            buffer[bytes_received] = '\0'; // Null-terminate the buffer
            printf("Server: %s\n", buffer);
        }
        else if (bytes_received == 0)
        {
            printf("Server closed the connection.\n");
            break;
        }
        else
        {
            perror("Error reading from server");
            break;
        }

        if (strncmp("exit", command, 4) == 0)
        {
            printf("Exiting...\n");
            break;
        }
    }

    close(client_socket);
    return 0;
}