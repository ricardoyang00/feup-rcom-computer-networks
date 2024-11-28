#include "app_download.h"

int parseURL(const char *url, ConnectionSettings *settings) {
    regex_t regex;
    int res;

    // Compile regex to find '@' in the URL
    res = regcomp(&regex, "@", 0);
    if (res) {
        printf("ERROR: Could not compile regex\n");
        return -1;
    }

    // Execute regex to check if '@' is present in the URL
    res = regexec(&regex, url, 0, NULL, 0);
    regfree(&regex);

    if (!res) {
        // '@' found in the URL, extract user, password, host, and path
        if (sscanf(url, USER_PASS_HOST_REGEX, settings->user, settings->password, settings->host, settings->url_path) != 4) {
            printf("ERROR: Could not parse URL with user and password\n");
            return -1;
        }
    } else {
        // '@' not found, extract host and path
        if (sscanf(url, HOST_REGEX, settings->host, settings->url_path) != 2) {
            printf("ERROR: Could not parse URL without user and password\n");
            return -1;
        }
        strcpy(settings->user, DEFAULT_USER);
        strcpy(settings->password, DEFAULT_PASSWORD);
    }

    // Extract file name from URL path
    char *file_name = strrchr(settings->url_path, '/');
    if (file_name) {
        strcpy(settings->file_name, file_name + 1);
    } else {
        strcpy(settings->file_name, settings->url_path);
    }

    // Get IP address from host
    struct hostent *h;
    if (strlen(settings->host) > 0) {
        if ((h = gethostbyname(settings->host)) == NULL) {
            herror("gethostbyname()");
            return -1;
        }
        strcpy(settings->host_name, h->h_name);
        strcpy(settings->ip_address, inet_ntoa(*((struct in_addr *) h->h_addr)));
    } else {
        return -1;
    }

    return 0; 
}

int openConnection(const char* ip_address, const int port) {
    int sockfd;
    struct sockaddr_in serv_addr;

    // Create socket
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (sockfd < 0) {
        perror("ERROR opening socket");
        return -1;
    }

    // Set up server address
    memset(&serv_addr, 0, sizeof(serv_addr));
    serv_addr.sin_family = AF_INET;
    serv_addr.sin_addr.s_addr = inet_addr(ip_address);
    serv_addr.sin_port = htons(port);

    // Connect to server
    if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        perror("ERROR connecting");
        close(sockfd);
        return -1;
    }

    return sockfd; // Return the socket file descriptor
}

int readResponse(const int socket, char *buffer, int *code) {
    if (buffer == NULL || code == NULL) {
        printf("ERROR: Invalid buffer or code\n");
        return -1;
    }

    State state = READING_CODE;
    *code = 0;
    int index = 0;
    int pre_code = 0;
    ssize_t read_bytes;

    while (state != DONE) {
        char byte = 0;
        read_bytes = read(socket, &byte, 1);
        if (read_bytes < 0) {
            perror("ERROR: Could not read byte from socket");
            return -1;
        } else if (read_bytes == 0) {
            state = DONE;
            break;
        }

        switch (state) {
            case READING_CODE:
                if (byte >= '0' && byte <= '9') {
                    *code = *code * 10 + (byte - '0');
                } else if (byte == ' ') {
                    state = READING_MESSAGE;
                } else if (byte == '-') {
                    state = MULTILINE_RESPONSE;
                }
                break;
            case READING_MESSAGE:
                if (byte == '\n') {
                    state = DONE;
                    buffer[index] = '\0';
                } else {
                    buffer[index++] = byte;
                }
                break;
            case MULTILINE_RESPONSE:
                if (byte == '\n') {
                    state = READING_CODE;
                    if (pre_code != 0 && pre_code != *code) {
                        printf("ERROR: Inconsistent response code\n");
                        return -1;
                    }
                    pre_code = *code;
                    *code = 0;
                }
                buffer[index++] = byte;
                break;
            default:
                state = READING_CODE;
                break;
        }
    }

    return 0;
}

int connectFTP(const int socket, const char *user, const char *password) {
    char *buffer = (char *)malloc(MAX_RESPONSE);
    int response = 0;

    if (readResponse(socket, buffer, &response) != 0) {
        printf("ERROR: Could not read response from server\n");
        free(buffer);
        buffer = NULL;
        return -1;
    }

    if (response != FTP_RESPONSE_220) {
        printf("ERROR: Expected response code %d, received %d\n", FTP_RESPONSE_220, response);
        free(buffer);
        buffer = NULL;
        return -1;
    }

    // Send USER command
    sprintf(buffer, "USER %s\r\n", user);
    if (write(socket, buffer, strlen(buffer)) < 0) {
        printf("ERROR: Could not write to socket\n");
        free(buffer);
        return -1;
    }

    // Read response from server
    if (readResponse(socket, buffer, &response) != 0) {
        printf("ERROR: Could not read response from server\n");
        free(buffer);
        return -1;
    }

    // Check for successful USER command response (typically 331)
    if (response != FTP_RESPONSE_331) {
        printf("ERROR: Expected response code %d, received %d\n", FTP_RESPONSE_331, response);
        free(buffer);
        return -1;
    }

    // Send PASS command
    sprintf(buffer, "PASS %s\r\n", password);
    if (write(socket, buffer, strlen(buffer)) < 0) {
        printf("ERROR: Could not write to socket\n");
        free(buffer);
        return -1;
    }

    // Read response from server
    if (readResponse(socket, buffer, &response) != 0) {
        printf("ERROR: Could not read response from server\n");
        free(buffer);
        return -1;
    }

    // Check for successful PASS command response (typically 230)
    if (response != FTP_RESPONSE_230) {
        printf("ERROR: Expected response code %d, received %d\n", FTP_RESPONSE_230, response);
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}

int enterPassiveMode(const int socket, char *ip_address, int *port) {
    char *buffer = (char *)malloc(MAX_RESPONSE);
    int response = 0;

    if (send(socket, "PASV\r\n", 6, 0) < 0) {
        perror("ERROR: Could not send PASV command to server");
        free(buffer);
        buffer = NULL;
        return -1;
    }

    if (readResponse(socket, buffer, &response) != 0) {
        printf("ERROR: Could not read response from server\n");
        free(buffer);
        buffer = NULL;
        return -1;
    }

    if (response != FTP_RESPONSE_227) {
        printf("ERROR: Expected response code 227, received %d\n", response);
        free(buffer);
        buffer = NULL;
        return -1;
    }

    // Parse IP address and port from PASV response
    int ip1, ip2, ip3, ip4, p1, p2;
    if (sscanf(buffer, PASSIVE_REGEX, &ip1, &ip2, &ip3, &ip4, &p1, &p2) != 6) {
        printf("ERROR: Could not parse PASV response\n");
        free(buffer);
        buffer = NULL;
        return -1;
    }

    sprintf(ip_address, "%d.%d.%d.%d", ip1, ip2, ip3, ip4);
    *port = p1 * 256 + p2;

    free(buffer);
    buffer = NULL;
    return 0;
}

int requestResource(const int socket, const char *resource) {
    char *buffer = (char *)malloc(MAX_RESPONSE);
    int response = 0;

    sprintf(buffer, "RETR %s\r\n", resource);
    if (write(socket, buffer, strlen(buffer)) < 0) {
        perror("ERROR writing to socket");
        free(buffer);
        return -1;
    }

    if (readResponse(socket, buffer, &response) != 0) {
        printf("ERROR: Could not read response from server\n");
        free(buffer);
        buffer = NULL;
        return -1;
    }

    if (response != 150) {
        printf("ERROR: Expected response code 150, received %d\n", response);
        free(buffer);
        buffer = NULL;
        return -1;
    }

    free(buffer);
    return 0;
}

int receiveData(const int socket, const char *file_name) {
    char file_path[MAX_SIZE];
    snprintf(file_path, sizeof(file_path), "downloads/%s", file_name);

    FILE *file = fopen(file_path, "wb");
    if (file == NULL) {
        perror("ERROR: Could not open file for writing");
        return -1;
    }

    char *buffer = (char *)malloc(MAX_RESPONSE);
    ssize_t read_bytes;

    while ((read_bytes = read(socket, buffer, MAX_RESPONSE)) > 0) {
        if (fwrite(buffer, 1, read_bytes, file) != read_bytes) {
            perror("ERROR: Could not write to file");
            free(buffer);
            fclose(file);
            return -1;
        }
    }

    if (read_bytes < 0) {
        perror("ERROR: Could not read from socket");
        free(buffer);
        fclose(file);
        return -1;
    }

    free(buffer);
    fclose(file);
    return 0;
}

int closeConnection(const int socket) {
    if (close(socket) < 0) {
        perror("ERROR: Could not close socket");
        return -1;
    }
    return 0;
}
