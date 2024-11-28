#include <stdio.h>
#include <stdlib.h>
#include <netdb.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <regex.h>

#define TRUE 1
#define FALSE 0

#define DEFAULT_USER "anonymous"
#define DEFAULT_PASSWORD "anonymous"
#define DEFAULT_PORT 21
#define MAX_SIZE 256

#define MAX_RESPONSE 1024

#define FTP_RESPONSE_220 220
#define FTP_RESPONSE_227 227

/*
#define HOST_REGEX      "%*[^/]//%[^/]"
#define HOST_AT_REGEX   "%*[^/]//%*[^@]@%[^/]"
#define RESOURCE_REGEX  "%*[^/]//%*[^/]/%s"
#define USER_REGEX      "%*[^/]//%[^:/]"
#define PASS_REGEX      "%*[^/]//%*[^:]:%[^@\n$]"
#define RESPCODE_REGEX  "%d"
#define PASSIVE_REGEX   "%*[^(](%d,%d,%d,%d,%d,%d)%*[^\n$)]"*/

#define USER_PASS_HOST_REGEX "ftp://%[^:]:%[^@]@%[^/]/%s"
#define HOST_REGEX "ftp://%[^/]/%s"
#define PASSIVE_REGEX "Entering Passive Mode (%d,%d,%d,%d,%d,%d)"

#define ASCII_ZERO 48
#define ASCII_NINE 57

/*

1. Parse FTP URL -> ftp://user:password@host/path/to/file

2. Establish a connection to FTP server

3. Login to the server with credentials from URL

4. Set passive mode for data transfer

5. Establish a data connection to the server

6. Send a RETR command to the server to download the file

7. Save the file to the local disk

8. Close the data connection

*/

typedef struct {
    char user[MAX_SIZE];
    char password[MAX_SIZE];
    char host[MAX_SIZE];
    char host_name[MAX_SIZE];
    char url_path[MAX_SIZE];
    char file_name[MAX_SIZE];
    char ip_address[16];
} ConnectionSettings;

typedef enum {
    READING_CODE,       // state of receive code
    READING_MESSAGE,    // state of receive message
    MULTILINE_RESPONSE, // state of receive multiline response
    DONE
} State;

/***
    url format: ftp://[<user>:<password>@]<host>/<url-path>
    The purpose of the function is to extract the user, password, host, and path from the URL.
 */
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

/***
    Creates and connects a socket to the specified IP address and port.
    Returns the socket file descriptor if successful, -1 otherwise.
 */
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


/***
    Reads the response from the server and extracts the response code and message.
    Returns 0 if successful, -1 otherwise.
 */
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

// Function to connect to FTP server and read welcome message
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
        perror("ERROR writing to socket");
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
    if (response != 331) {
        printf("ERROR: Expected response code 331, received %d\n", response);
        free(buffer);
        return -1;
    }

    // Send PASS command
    sprintf(buffer, "PASS %s\r\n", password);
    if (write(socket, buffer, strlen(buffer)) < 0) {
        perror("ERROR writing to socket");
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
    if (response != 230) {
        printf("ERROR: Expected response code 230, received %d\n", response);
        free(buffer);
        return -1;
    }

    free(buffer);
    return 0;
}

/***
    Enter in passive mode to receive data from the server.
 */
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

// Function to request a resource from the server
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

// Function to receive data from the server and save it to a file
int receiveData(const int socket, const char *file_name) {
    FILE *file = fopen(file_name, "wb");
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

// Function to close the connection
int closeConnection(const int socket) {
    if (close(socket) < 0) {
        perror("ERROR: Could not close socket");
        return -1;
    }
    return 0;
}

int main() {
    ConnectionSettings settings;
    const char *urls[] = {
        "ftp://ftp.up.pt/pub/gnu/emacs/elisp-manual-21-2.8.tar.gz",
        "ftp://demo:password@test.rebex.net/readme.txt",
        "ftp://anonymous:anonymous@ftp.bit.nl/speedtest/100mb.bin",
        NULL
    };

    for (int i = 0; urls[i] != NULL; i++) {
        printf("Testing URL: %s\n", urls[i]);
        if (parseURL(urls[i], &settings) == 0) {
            printf("          User: %s\n", settings.user);
            printf("      Password: %s\n", settings.password);
            printf("          Host: %s\n", settings.host);
            printf("     Host Name: %s\n", settings.host_name);
            printf("    IP Address: %s\n", settings.ip_address);
            printf("      URL Path: %s\n", settings.url_path);
            printf("     File Name: %s\n", settings.file_name);

            // Create socket and connect to the server
            int control_sockfd = openConnection(settings.ip_address, DEFAULT_PORT); // FTP typically uses port 21
            if (control_sockfd < 0) {
                printf("Failed to connect to %s on port %d\n", settings.ip_address, DEFAULT_PORT);
            } else {
                printf("Successfully connected to %s on port %d\n", settings.ip_address, DEFAULT_PORT);
                
                // Perform FTP login
                if (connectFTP(control_sockfd, settings.user, settings.password) != 0) {
                    printf("Failed to log in to FTP server\n");
                } else {
                    printf("Successfully logged in to FTP server\n");

                    // Enter passive mode
                    char data_ip[16];
                    int data_port;
                    if (enterPassiveMode(control_sockfd, data_ip, &data_port) == 0) {
                        printf("Entered passive mode. Data connection IP: %s, Port: %d\n", data_ip, data_port);

                        int data_sockfd = openConnection(data_ip, data_port);
                        if (data_sockfd >= 0) {
                            printf("Successfully connected to data connection\n");

                            // Request resource from server
                            if (requestResource(control_sockfd, settings.url_path) == 0) {
                                printf("Successfully requested resource from server\n");

                                // Receive data from server and save to file
                                if (receiveData(data_sockfd, settings.file_name) == 0) {
                                    printf("Successfully received data from server and saved to file\n");
                                } else {
                                    printf("Failed to receive data from server\n");
                                }
                            } else {
                                printf("Failed to request resource from server\n");
                            }
                            if (closeConnection(data_sockfd) != 0) {
                                printf("Failed to close data connection\n");
                            }
                        } else {
                            printf("Failed to connect to data connection\n");
                        }
                    } else {
                        printf("Failed to enter passive mode\n");
                    }
                }
            }
            if (closeConnection(control_sockfd) != 0) {
                printf("Failed to close control connection\n");
            }
        } else {
            printf("Failed to parse URL\n");
        }
        printf("\n");
    }

    return 0;
}

/*
int get_ip_address();

int ftp_connect();
int ftp_login();
int ftp_set_passive_mode();
int ftp_retrieve_file();
int ftp_disconnect();

int send_ftp_command();
int parse_pasv_response();
*/
