#include <arpa/inet.h>
#include <assert.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"

#define BUF_SIZE 1024
#define BAD_SOCKET -1
#define PORT_STR_LEN 20

/**
 * @brief Get the port as a string.
 *
 * @param port
 * @return char*
 */
char* get_port_str(int* port) {
    char* port_str = malloc(PORT_STR_LEN * sizeof(char));

    int len = snprintf(port_str, PORT_STR_LEN, "%d", *port);
    if (len < 0 || len >= PORT_STR_LEN) {
        printf("ERROR: Malformed port");
        return NULL;
    }

    return port_str;
}

/**
 * @brief Creates and connects a socket.
 *
 * @param host - The host name e.g. www.canterbury.ac.nz
 * @param port - e.g. 80
 * @return int - The connected socket.
 */
int create_socket(char* host, int* port) {
    struct addrinfo hints;       // server address info
    struct addrinfo* res = NULL; // connector's address information
    int sockfd;
    char* port_str = get_port_str(port);

    if (port_str == NULL) {
        return BAD_SOCKET;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("ERROR: socket");
        return BAD_SOCKET;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    int result = getaddrinfo(host, port_str, &hints, &res);

    freeaddrinfo(res);
    free(port_str);

    if (result != 0) {
        printf("ERROR: getaddrinfo");
        return BAD_SOCKET;
    }

    if (connect(sockfd, res->ai_addr, res->ai_addrlen) == -1) {
        printf("ERROR: connect");
        return BAD_SOCKET;
    }

    return sockfd;
}

/**
 * @brief Reads the socket until it is empty, and returns a buffer of the
 * socket's contents.
 *
 * @param sockfd - The socket to read from.
 * @return Buffer* - The socket's contents.
 */
Buffer* read_socket(int sockfd) {
    int currentSize = BUF_SIZE;
    int bytesRead = 0;

    Buffer* buffer = malloc(sizeof(Buffer));
    buffer->data = calloc(currentSize, sizeof(char));
    buffer->length = 0;

    if ((bytesRead = read(sockfd, buffer->data, BUF_SIZE)) <= 0) {
        printf("ERROR: reading from socket");
        free(buffer->data);
        free(buffer);
        return NULL;
    }
    buffer->length += bytesRead;

    char newData[BUF_SIZE] = {0};

    while ((bytesRead = read(sockfd, newData, BUF_SIZE)) > 0) {
        if (buffer->length + bytesRead >= currentSize) {
            currentSize += BUF_SIZE;
            buffer->data = realloc(buffer->data, currentSize);
        }

        memcpy(&buffer->data[buffer->length], &newData, bytesRead);
        buffer->length += bytesRead;
        memset(&newData, 0, bytesRead);
    }

    return buffer;
}

/**
 * @brief Creates an HTTP GET header.
 *
 * @param host
 * @param page
 * @param range
 * @return char*
 */
char* create_http_get(char* host, char* page, const char* range) {
    char* format = "GET /%s HTTP/1.0\r\n"
                   "Host: %s\r\n"
                   "Range: bytes=%s\r\n"
                   "User-Agent: getter\r\n\r\n";
    size_t length =
        strlen(format) + strlen(host) + strlen(page) + strlen(range);

    char* header = malloc(sizeof(char) * length);
    sprintf(header, format, page, host, range);
    return header;
}

/**
 * Perform an HTTP 1.0 query to a given host and page and port number.
 * host is a hostname and page is a path on the remote server. The query
 * will attempt to retrieve content in the given byte range.
 * User is responsible for freeing the memory.
 *
 * @param host - The host name e.g. www.canterbury.ac.nz
 * @param page - e.g. /index.html
 * @param range - Byte range e.g. 0-500. NOTE: A server may not respect this
 * @param port - e.g. 80
 * @return Buffer - Pointer to a buffer holding response data from query
 *                  NULL is returned on failure.
 */
Buffer* http_query(char* host, char* page, const char* range, int port) {
    int sockfd = create_socket(host, &port);

    if (sockfd == BAD_SOCKET) {
        return NULL;
    }

    char* header = create_http_get(host, page, range);

    if (write(sockfd, header, strlen(header)) == -1) {
        printf("ERROR: send header");
        return NULL;
    }

    Buffer* buffer = read_socket(sockfd);

    free(header);
    close(sockfd);
    return buffer;
}

/**
 * Separate the content from the header of an http request.
 * NOTE: returned string is an offset into the response, so
 * should not be freed by the user. Do not copy the data.
 * @param response - Buffer containing the HTTP response to separate
 *                   content from
 * @return string response or NULL on failure (buffer is not HTTP response)
 */
char* http_get_content(Buffer* response) {

    char* header_end = strstr(response->data, "\r\n\r\n");

    if (header_end) {
        return header_end + 4;
    } else {
        return response->data;
    }
}

/**
 * Splits an HTTP url into host, page. On success, calls http_query
 * to execute the query against the url.
 * @param url - Webpage url e.g. learn.canterbury.ac.nz/profile
 * @param range - The desired byte range of data to retrieve from the page
 * @return Buffer pointer holding raw string data or NULL on failure
 */
Buffer* http_url(const char* url, const char* range) {
    char host[BUF_SIZE];
    strncpy(host, url, BUF_SIZE);

    char* page = strstr(host, "/");

    if (page) {
        page[0] = '\0';

        ++page;
        return http_query(host, page, range, 80);
    } else {

        fprintf(stderr, "could not split url into host/page %s\n", url);
        return NULL;
    }
}

/**
 * @brief Creates an HTTP head header.
 *
 * @param host
 * @param page
 * @return char*
 */
char* create_http_head(char* host, char* page) {
    char* format = "GET /%s HTTP/1.0\r\n"
                   "Host: %s\r\n"
                   "User-Agent: getter\r\n\r\n";
    size_t length = strlen(format) + strlen(host) + strlen(page);

    char* header = malloc(sizeof(char) * length);
    sprintf(header, format, page, host);
    return header;
}

/**
 * @brief Performs an HTTP head request.
 *
 * @param host
 * @param page
 * @param port
 * @return Buffer*
 */
Buffer* http_head(char* host, char* page, int port) {
    int sockfd = create_socket(host, &port);

    if (sockfd == BAD_SOCKET) {
        return NULL;
    }

    char* header = create_http_head(host, page);

    if (write(sockfd, header, strlen(header)) == -1) {
        printf("ERROR: send header");
        return NULL;
    }

    Buffer* buffer = read_socket(sockfd);

    free(header);
    close(sockfd);

    return buffer;
}

/**
 * Makes a HEAD request to a given URL and gets the content length
 * Then determines max_chunk_size and number of split downloads needed
 * @param url   The URL of the resource to download
 * @param threads   The number of threads to be used for the download
 * @return int  The number of downloads needed satisfying max_chunk_size
 *              to download the resource
 */
int get_num_tasks(char* url, int threads) {
    char host[BUF_SIZE];
    strncpy(host, url, BUF_SIZE);
    char* page = strstr(host, "/");

    if (page) {
        page[0] = '\0';
        ++page;
    } else {
        return 0;
    }

    Buffer* buffer = http_head(host, page, 80);

    if (buffer != NULL) {
        printf("%s\n", buffer->data);
    }

    return 0;
}

int get_max_chunk_size() {
    return max_chunk_size;
}
