#include <arpa/inet.h>
#include <assert.h>
#include <ctype.h>
#include <netdb.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#include "http.h"

#define BUF_SIZE 1024
#define BAD_SOCKET -1
#define PORT_STR_LEN 20

#define ACCEPT_RANGES "accept-ranges:"
#define BYTES "bytes"

#define CONTENT_LENGTH "content-length:"

/**
 * @brief Creates and connects a socket.
 *
 * @param host The host name e.g. www.canterbury.ac.nz
 * @param port e.g. 80
 * @return int The connected socket.
 */
int create_socket(char* host, int* port) {
    struct addrinfo hints;               // server address info
    struct addrinfo* server_addr = NULL; // connector's address information
    int sockfd;
    char port_str[PORT_STR_LEN];

    if (snprintf(port_str, PORT_STR_LEN, "%d", *port) <= 0) {
        printf("ERROR: Malformed port\n");
        return BAD_SOCKET;
    }

    if ((sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1) {
        printf("ERROR: socket\n");
        return BAD_SOCKET;
    }

    memset(&hints, 0, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    if (getaddrinfo(host, port_str, &hints, &server_addr) != 0) {
        printf("ERROR: getaddrinfo\n");
        freeaddrinfo(server_addr);
        return BAD_SOCKET;
    }

    if (connect(sockfd, server_addr->ai_addr, server_addr->ai_addrlen) == -1) {
        printf("ERROR: connect\n");
        freeaddrinfo(server_addr);
        return BAD_SOCKET;
    }

    freeaddrinfo(server_addr);
    return sockfd;
}

/**
 * @brief Creates a buffer object
 *
 * @param size The size of the data.
 * @return Buffer*
 */
Buffer* create_buffer(int size) {
    Buffer* buffer = malloc(sizeof(Buffer));
    buffer->data = malloc(size * sizeof(char));
    buffer->length = 0;
    return buffer;
}

/**
 * @brief Reads the socket until it is empty, and returns a buffer of the
 * socket's contents.
 * NOTE: It is required that the returned buffer is freed.
 *
 * @param sockfd - The socket to read from.
 * @return Buffer* - The socket's contents.
 */
Buffer* read_socket(int sockfd) {
    int allocated = BUF_SIZE;
    int bytes_read = 0;

    Buffer* buffer = create_buffer(allocated);

    while ((bytes_read =
                read(sockfd, &buffer->data[buffer->length], BUF_SIZE)) > 0) {
        buffer->length += bytes_read;
        if (buffer->length + BUF_SIZE > allocated) {
            allocated += BUF_SIZE;
            buffer->data = realloc(buffer->data, allocated);
        }
    }

    return buffer;
}

/**
 * Perform an HTTP 1.0 query to a given host and page and port number.
 * host is a hostname and page is a path on the remote server. The query
 * will attempt to retrieve content in the given byte range.
 * User is responsible for freeing the memory.
 *
 * @param host The host name e.g. www.canterbury.ac.nz
 * @param page e.g. /index.html
 * @param range Byte range e.g. 0-500. NOTE: A server may not respect this
 * @param port e.g. 80
 * @return Buffer Pointer to a buffer holding response data from query
 *                  NULL is returned on failure.
 */
Buffer* http_query(char* host, char* page, const char* range, int port) {
    int sockfd = create_socket(host, &port);

    if (sockfd == BAD_SOCKET) {
        return NULL;
    }

    char* format = "GET /%s HTTP/1.0\r\n"
                   "Host: %s\r\n"
                   "Range: bytes=%s\r\n"
                   "User-Agent: getter\r\n\r\n";
    size_t length =
        strlen(format) + strlen(host) + strlen(page) + strlen(range);
    char header[length];
    snprintf(header, length, format, page, host, range);

    if (write(sockfd, header, strlen(header)) == -1) {
        printf("ERROR: send header");
        return NULL;
    }

    Buffer* buffer = read_socket(sockfd);

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

    char* format = "GET /%s HTTP/1.0\r\n"
                   "Host: %s\r\n"
                   "User-Agent: getter\r\n\r\n";
    size_t length = strlen(format) + strlen(host) + strlen(page);
    char header[length];
    snprintf(header, length, format, page, host);

    if (write(sockfd, header, strlen(header)) == -1) {
        printf("ERROR: send header");
        close(sockfd);
        return NULL;
    }

    Buffer* buffer = read_socket(sockfd);

    close(sockfd);
    return buffer;
}

/**
 * @brief Returns a pointer to the character after whitespace.
 *
 * @param location The character to start the search from.
 * @param buffer The `buffer`, of which `location` resides inside.
 * @return char* The pointer to the character after whitespace.
 */
char* consume_whitespace(char* location, Buffer* buffer) {
    char* end = buffer->data + buffer->length;
    while (*location == ' ' && location <= end) {
        location++;
    }
    return location;
}

/**
 * @brief Returns a Boolean indicating if the HTTP header has an Accept-Ranges
 * value of "bytes".
 *
 * @param buffer The buffer containing the HTTP header.
 * @return true The HTTP header has an "Accept-Ranges" header with a value of
 * "bytes".
 * @return false The HTTP header does not have an "Accept-Ranges" header with a
 * value of "bytes".
 */
bool get_accept_ranges(Buffer* buffer) {
    char* accept_header = strstr(buffer->data, ACCEPT_RANGES);
    if (accept_header == NULL) {
        return false;
    }

    char* value_start =
        consume_whitespace(accept_header + strlen(ACCEPT_RANGES), buffer);

    char* accept_value = strstr(value_start, BYTES);
    // If there is a substring matching BYTES, than it should be at the location
    // `accept_header`.
    return value_start == accept_value;
}

/**
 * @brief Gets the Content-Length value from the HTTP header.
 *
 * @param bufferThe buffer containing the HTTP header.
 * @return int The value for the Content-Length header. 0 if the header does not
 * exist.
 */
int get_content_length(Buffer* buffer) {
    char* length_header = strstr(buffer->data, CONTENT_LENGTH);
    if (length_header == NULL) {
        return 0;
    }

    return atoi(length_header + strlen(CONTENT_LENGTH));
}

/**
 * @brief Parses the header for Accept-Ranges and Content-Length headers.
 *
 * @param buffer
 * @param accept_ranges
 * @param content_length
 */
void parse_head(Buffer* buffer, bool* accept_ranges, int* content_length) {
    // Converts the buffer to a lower case.
    for (size_t i = 0; i < buffer->length; i++) {
        buffer->data[i] = tolower(buffer->data[i]);
    }

    *accept_ranges = get_accept_ranges(buffer);
    *content_length = get_content_length(buffer);
}

/**
 * @brief Divides the numerator by the denominator, and returns the ceiling of
 * the floating point value as an integer.
 *
 * @param num
 * @param denom
 * @return int
 */
int divide_ceil(int num, int denom) {
    int result = (float) num / denom;
    if (result * denom < num) {
        result++;
    }
    return result;
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
    if (buffer == NULL) {
        return 0;
    }

    bool accept_ranges;
    int content_length;
    parse_head(buffer, &accept_ranges, &content_length);
    buffer_free(buffer);

    if (accept_ranges == false || content_length < BUF_SIZE) {
        max_chunk_size = content_length;
        return 1;
    } else {
        max_chunk_size = divide_ceil(content_length, threads);
        return threads;
    }
}

int get_max_chunk_size() {
    return max_chunk_size;
}
