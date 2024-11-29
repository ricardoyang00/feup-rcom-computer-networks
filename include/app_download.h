#ifndef APP_DOWNLOAD_H
#define APP_DOWNLOAD_H

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

#define USER_PASS_HOST_REGEX "ftp://%[^:]:%[^@]@%[^/]/%s"
#define HOST_REGEX "ftp://%[^/]/%s"
#define PASSIVE_REGEX "Entering Passive Mode (%d,%d,%d,%d,%d,%d)"

#define DOWNLOAD_PATH "downloads/"

#define DEFAULT_USER "anonymous"
#define DEFAULT_PASSWORD "anonymous"
#define DEFAULT_PORT 21

#define FTP_RESPONSE_150 150    // File status OK, about to open data connection
#define FTP_RESPONSE_220 220    // Service ready for new user
#define FTP_RESPONSE_227 227    // Entering Passive Mode
#define FTP_RESPONSE_230 230    // User logged in, proceed
#define FTP_RESPONSE_331 331    // User name OK, need password

#define MAX_RESPONSE 1024
#define MAX_SIZE 256


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

// Function to parse URL and extract connection settings
// URL format: ftp://[<user>:<password>@]<host>/<url-path>
int parseURL(const char *url, ConnectionSettings *settings);

// Function to open a connection to the specified IP address and port
int openConnection(const char* ip_address, const int port);

// Function to read response from server
int readResponse(const int socket, char *buffer, int *code);

// Function to connect to FTP server
int connectFTP(const int socket, const char *user, const char *password);

// Function to enter passive mode
int enterPassiveMode(const int socket, char *ip_address, int *port);

// Function to request a resource from the server
int requestResource(const int socket, const char *resource);

// Function to receive data from the server and save it to a file
int receiveData(const int socket, const char *file_name);

// Function to close the connection
int closeConnection(const int socket);

#endif