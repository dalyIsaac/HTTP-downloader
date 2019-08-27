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
    char port_str[20] = {0};

    int n = snprintf(port_str, 20, "%d", *port);
    if (n < 0 || n >= 20) {
        printf("ERROR: Malformed port");
        return BAD_SOCKET;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("ERROR: socket");
        return BAD_SOCKET;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &res) != 0) {
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
 * @param range - The size of the content to retrieve.
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
 * @brief Creates the HTTP header.
 *
 * @param host
 * @param page
 * @param range
 * @return char*
 */
char* create_header(char* host, char* page, const char* range) {
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

    char* header = create_header(host, page, range);

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
 * Makes a HEAD request to a given URL and gets the content length
 * Then determines max_chunk_size and number of split downloads needed
 * @param url   The URL of the resource to download
 * @param threads   The number of threads to be used for the download
 * @return int  The number of downloads needed satisfying max_chunk_size
 *              to download the resource
 */
int get_num_tasks(char* url, int threads) {
    assert(0 && "not implemented yet!");
}

int get_max_chunk_size() {
    return max_chunk_size;
}
