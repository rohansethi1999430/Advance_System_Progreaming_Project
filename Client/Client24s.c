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
        printf("Error in socket creation\n");
        exit(1);
    }
    printf("Socket created successfully\n");

    bzero(&server_addr, sizeof(server_addr));

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(PORT);
    server_addr.sin_addr.s_addr = inet_addr("127.0.0.1"); // Ensure this matches the server's IP

    if (connect(client_socket, (struct sockaddr *)&server_addr, sizeof(server_addr)) != 0)
    {
        printf("Connection to Smain failed\n");
        exit(1);
    }
    printf("Connected to Smain\n");

    while (1)
    {
        bzero(buffer, BUFFER_SIZE);
        bzero(command, BUFFER_SIZE);

        printf("Enter command: ");
        fgets(command, BUFFER_SIZE, stdin);
        command[strcspn(command, "\n")] = 0; // Remove newline character
        printf("Sending command: %s\n", command);

        write(client_socket, command, strlen(command));

        // Handle 'ufile' command
        if (strncmp("ufile", command, 5) == 0)
        {
            char *filename = strtok(command + 6, " "); // Get filename after 'ufile '
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
                write(client_socket, buffer, n);
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
                    fwrite(buffer, sizeof(char), bytes_received, fp);
                    if (bytes_received < BUFFER_SIZE)
                    {
                        printf("End of file received.\n");
                        break; // End of file or transmission
                    }
                    else{break;}
                }
                fclose(fp);
                printf("File %s received successfully\n", filename);
                free(filename);
                continue;
            }
        }

        // Handle 'rmfile' command
        if (strncmp("rmfile", command, 6) == 0)
        {
            char *filepath = strtok(command + 7, " "); // Get file path after 'rmfile '
            if (filepath != NULL)
            {
                printf("Sending file removal request for: %s\n", filepath);
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
                FILE *fp = fopen(tar_filename, "wb");
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

                    // Break out of the loop if we received less than the buffer size, indicating the end of the file
                    if (bytes_received < BUFFER_SIZE)
                    {
                        printf("End of tar file received from Smain\n");
                        break; // End of file or transmission
                    }
                    else{
                        break;
                    }

                }
                fclose(fp);
                printf("Tarball %s received successfully\n", tar_filename);
                continue;
            }
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
