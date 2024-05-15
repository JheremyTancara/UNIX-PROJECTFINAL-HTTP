#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <arpa/inet.h> 

#define BUFFER_SIZE 1024
#define MAX_CONNECTIONS 30

void handle_get_request(int client_socket, const char* url) {

    if (strcmp(url, "/") == 0) {
        const char* response = "HTTP/1.0 200 OK\r\nContent-Type: text/plain\r\n\r\nServer Jheremy HTTP-ClientServer versión 1.0\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }

    int file_fd = open(url + 1, O_RDONLY);
    if (file_fd < 0) {
    const char* response = "HTTP/1.0 404 Not Found\r\nContent-Type: text/plain\r\n\r\nResource not found\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }



    char file_buffer[BUFFER_SIZE];
    ssize_t bytes_read;
    while ((bytes_read = read(file_fd, file_buffer, BUFFER_SIZE)) > 0) {
        send(client_socket, file_buffer, bytes_read, 0);
    }
    close(file_fd);
    close(client_socket);
}

void handle_post_request(int client_socket, const char* url, const char* body) {
 
    if (strstr(url, ".py") == NULL) {
    const char* response = "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nValid POST request only for .py files\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }


    int pipefd[2];
    if (pipe(pipefd) == -1) {
        perror("Error creating pipe");
        const char* response = "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nInternal server error\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }


    pid_t pid = fork();
    if (pid == -1) {
        perror("Error creating child process");
        const char* response = "HTTP/1.0 500 Internal Server Error\r\nContent-Type: text/plain\r\n\r\nInternal server errorv\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }

    if (pid == 0) {

        dup2(pipefd[1], STDOUT_FILENO);
        close(pipefd[0]);
        close(pipefd[1]);

        execlp("python3", "python3", url + 1, NULL);
        perror("Error executing Python script");
        exit(EXIT_FAILURE);
    } else { 

        close(pipefd[1]);

 
        char buffer[BUFFER_SIZE];
        ssize_t bytes_read;
        while ((bytes_read = read(pipefd[0], buffer, BUFFER_SIZE)) > 0) {
            send(client_socket, buffer, bytes_read, 0);
        }

        close(pipefd[0]);
        close(client_socket);
    }
}

void handle_request(int client_socket) {
    char buffer[BUFFER_SIZE];
    int bytes_received = recv(client_socket, buffer, BUFFER_SIZE, 0);
    if (bytes_received < 0) {
        perror("Error receiving customer data");
        close(client_socket);
        return;
    }
    printf("HTTP request received:\n%s\n", buffer);

    char method[10];
    char url[256];
    if (sscanf(buffer, "%9s %255s", method, url) != 2) {
        const char* response = "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nInvalid HTTP request\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }

    if (strcmp(method, "GET") == 0) {
        handle_get_request(client_socket, url);
    } else if (strcmp(method, "POST") == 0) {
        
        char* body_start = strstr(buffer, "\r\n\r\n");
        if (body_start != NULL) {
            body_start += 4;
            handle_post_request(client_socket, url, body_start);
        } else {
            const char* response = "HTTP/1.0 400 Bad Request\r\nContent-Type: text/plain\r\n\r\nMissing POST request body\r\n";
            send(client_socket, response, strlen(response), 0);
            close(client_socket);
        }
    } else {
        const char* response = "HTTP/1.0 405 Method Not Allowed\r\nContent-Type: text/plain\r\n\r\nMethod not allowed\r\n";
        send(client_socket, response, strlen(response), 0);
        close(client_socket);
        return;
    }
}

int main(int argc, char* argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <port>\n", argv[0]);
        return 1;
    }

    int port = atoi(argv[1]);

    int server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("Error creating server socket");
        return 1;
    }

    struct sockaddr_in server_address;
    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(server_socket, (struct sockaddr *)&server_address, sizeof(server_address)) < 0) {
        perror("Error binding server socket");
        close(server_socket);
        return 1;
    }

    if (listen(server_socket, MAX_CONNECTIONS) < 0) {
        perror("Error when putting the socket in listening mode");
        close(server_socket);
        return 1;
    }

    printf("HTTP server listening on port %d...\n", port);

    while (1) {
        struct sockaddr_in client_address;
        socklen_t client_address_size = sizeof(client_address);
        int client_socket = accept(server_socket, (struct sockaddr *)&client_address, &client_address_size);
        if (client_socket < 0) {
            perror("Error accepting client connection");
            continue;
        }
        printf("Connection accepted from %s:%d\n", inet_ntoa(client_address.sin_addr), ntohs(client_address.sin_port));

        pid_t pid = fork();
        if (pid == 0) {
            handle_request(client_socket);
            exit(0);
        } else if (pid < 0) {
            perror("Error creating child process");
            close(client_socket);
        }

        close(client_socket);
    }

    close(server_socket);
    return 0;
}