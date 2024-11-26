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

struct connection_settings {
    char user[MAX_SIZE];
    char password[MAX_SIZE];
    char host[MAX_SIZE];
    char host_name[MAX_SIZE];
    char url_path[MAX_SIZE];
    char file_name[MAX_SIZE];
    char ip_address[16];
};

/***
    url format: ftp://[<user>:<password>@]<host>/<url-path>
    The purpose of the function is to extract the user, password, host, and path from the URL.
 */
int parseURL(const char *url, struct connection_settings *settings) {
    regex_t regex;
    int res;

    // Initialize settings to avoid garbage values
    memset(settings, 0, sizeof(struct connection_settings));

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
int connectSocket(const char* ip_address, const int port) {
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

    return sockfd;
}

int main() {
    struct connection_settings settings;
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
            printf("      File Name: %s\n", settings.file_name);

            // Create socket and connect to the server
            int sockfd = connectSocket(settings.ip_address, DEFAULT_PORT); // FTP typically uses port 21
            if (sockfd >= 0) {
                printf("Successfully connected to %s on port %d\n", settings.ip_address, DEFAULT_PORT);
                close(sockfd); // Close the socket after use
            } else {
                printf("Failed to connect to %s on port 21\n", settings.ip_address);
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
