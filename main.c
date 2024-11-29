#include <stdio.h>
#include "app_download.h"

int main(int argc, char *argv[]) {
    if (argc != 2) {
        printf("Usage: %s <argument>\n", argv[0]);
        return 1;
    }

    const char *argument = argv[1];
    printf("Received argument: %s\n", argument);

    ConnectionSettings settings;
    if (parseURL(argument, &settings) != 0) {
        printf("[ERROR] Could not parse URL\n");
        printf("URL format should be: ftp://[<user>:<password>@]<host>/<url-path>");
        return 1;
    }

    printf("Starting Download Application\n");
    printf("  - User: %s\n", settings.user);
    printf("  - Password: %s\n", settings.password);
    printf("  - Host: %s\n", settings.host);
    printf("  - Host Name: %s\n", settings.host_name);
    printf("  - IP Address: %s\n", settings.ip_address);
    printf("  - URL Path: %s\n", settings.url_path);
    printf("  - File Name: %s\n", settings.file_name);
    printf("\n");
    
    // Connect to FTP server
    int control_sockfd = openConnection(settings.ip_address, DEFAULT_PORT);
    if (control_sockfd < 0) {
        printf("[ERROR] Failed to connect to %s on port %d\n", settings.ip_address, DEFAULT_PORT);
        return 1;
    }
    printf("Successfully connected to %s on port %d\n", settings.ip_address, DEFAULT_PORT);

    // Perform FTP login
    if (connectFTP(control_sockfd, settings.user, settings.password) != 0) {
        printf("[ERROR] Failed to log in to FTP server\n");
        return 1;
    }
    printf("Successfully logged in to FTP server\n");

    // Enter passive mode
    char data_ip[16];
    int data_port;
    if (enterPassiveMode(control_sockfd, data_ip, &data_port) != 0) {
        printf("[ERROR] Failed to enter passive mode\n");
        return 1;
    }
    printf("Entered passive mode. Data connection IP: %s, Port: %d\n", data_ip, data_port);

    // Connect to data connection
    int data_sockfd = openConnection(data_ip, data_port);
    if (data_sockfd < 0) {
        printf("[ERROR] Failed to connect to data connection\n");
        return 1;
    }
    printf("Successfully connected to data connection\n");

    // Request resource from server
    if (requestResource(control_sockfd, settings.url_path) != 0) {
        printf("[ERROR] Failed to request resource from server\n");
        return 1;
    }
    printf("Successfully requested resource from server\n");

    // Receive data from server and save to file
    printf("Starting download...\n");
    if (receiveData(data_sockfd, settings.file_name) != 0) {
        printf("[ERROR] Failed to receive data from server\n");
        return 1;
    }
    printf("Successfully received data from server and saved to \"%s%s\"\n", DOWNLOAD_PATH, settings.file_name);
    printf("\nExiting...\n");

    // Close data connection
    if (closeConnection(data_sockfd) != 0) {
        printf("[ERROR] Failed to close data connection\n");
        return 1;
    }

    // Close control connection
    if (closeConnection(control_sockfd) != 0) {
        printf("[ERROR] Failed to close control connection\n");
        return 1;
    }

    return 0;
}