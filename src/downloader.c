#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>

#include "http.h"
#include "queue.h"
#include <libgen.h>

#define FILE_SIZE 256

typedef struct {
    char* url;
    int min_range;
    int max_range;
    Buffer* result;
} Task;

typedef struct {
    Queue* todo;
    Queue* done;

    pthread_t* threads;
    int num_workers;

} Context;

void create_directory(const char* dir) {
    struct stat st = {0};

    if (stat(dir, &st) == -1) {
        int rc = mkdir(dir, 0700);
        if (rc == -1) {
            perror("mkdir");
            exit(EXIT_FAILURE);
        }
    }
}

void* worker_thread(void* arg) {
    Context* context = (Context*) arg;

    Task* task = (Task*) queue_get(context->todo);
    char* range = (char*) malloc(1024 * sizeof(char));

    while (task) {
        snprintf(range, 1024 * sizeof(char), "%d-%d", task->min_range,
                 task->max_range);

        task->result = http_url(task->url, range);

        queue_put(context->done, task);
        task = (Task*) queue_get(context->todo);
    }

    free(range);
    return NULL;
}

Context* spawn_workers(int num_workers) {
    Context* context = (Context*) malloc(sizeof(Context));

    context->todo = queue_alloc(num_workers * 2);
    context->done = queue_alloc(num_workers * 2);

    context->num_workers = num_workers;

    context->threads = (pthread_t*) malloc(sizeof(pthread_t) * num_workers);
    int i = 0;

    for (i = 0; i < num_workers; ++i) {
        if (pthread_create(&context->threads[i], NULL, worker_thread,
                           context) != 0) {
            perror("pthread_create");
            exit(1);
        }
    }

    return context;
}

void free_workers(Context* context) {
    int num_workers = context->num_workers;
    int i = 0;

    for (i = 0; i < num_workers; ++i) {
        queue_put(context->todo, NULL);
    }

    for (i = 0; i < num_workers; ++i) {
        if (pthread_join(context->threads[i], NULL) != 0) {
            perror("pthread_join");
            exit(1);
        }
    }

    queue_free(context->todo);
    queue_free(context->done);

    free(context->threads);
    free(context);
}

Task* new_task(char* url, int min_range, int max_range) {
    Task* task = malloc(sizeof(Task));
    task->result = NULL;
    task->url = malloc(strlen(url) + 1);
    task->min_range = min_range;
    task->max_range = max_range;

    strcpy(task->url, url);

    return task;
}

void free_task(Task* task) {

    if (task->result) {
        free(task->result->data);
        free(task->result);
    }

    free(task->url);
    free(task);
}

void wait_task(const char* download_dir, Context* context) {
    char filename[FILE_SIZE], url_file[FILE_SIZE];
    Task* task = (Task*) queue_get(context->done);

    if (task->result) {

        snprintf(url_file, FILE_SIZE * sizeof(char), "%d", task->min_range);
        size_t len = strlen(url_file);
        for (int i = 0; i < len; ++i) {
            if (url_file[i] == '/') {
                url_file[i] = '|';
            }
        }

        snprintf(filename, FILE_SIZE, "%s/%s", download_dir, url_file);
        FILE* fp = fopen(filename, "w");

        if (fp == NULL) {
            fprintf(stderr, "error writing to: %s\n", filename);
            exit(EXIT_FAILURE);
        }

        char* data = http_get_content(task->result);
        if (data) {
            size_t length = task->result->length - (data - task->result->data);

            fwrite(data, 1, length, fp);
            fclose(fp);

            printf("downloaded %d bytes from %s\n", (int) length, task->url);
        } else {
            printf("error in response from %s\n", task->url);
        }

    } else {

        fprintf(stderr, "error downloading: %s\n", task->url);
    }

    free_task(task);
}

/**
 * @brief Writes the source file to the destination file.
 *
 * @param dest_file The destination file.
 * @param src_name The name of the source file.
 * @param buffer The buffer to store the contents of the read file.
 * @param bytes The maximum byte size downloaded.
 * @param currentTask The current task number.
 * @param tasks The total number of tasks.
 * @return int 0 for no error, -1 for an error opening the file specified by
 * src_name.
 */
int write_to_dest(FILE* dest_file, char* src_name, char* buffer, int bytes,
                  int currentTask, int tasks) {
    FILE* src_file = fopen(src_name, "r");

    if (src_file == NULL) {
        return -1;
    }

    int bytes_read;
    while ((bytes_read = fread(buffer, 1, bytes, src_file)) > 0) {
        fwrite(buffer, bytes_read, 1, dest_file);
    }

    fclose(src_file);
    remove(src_name);
    return 0;
}

/**
 * @brief Replaces all instances of old_char in a string with new_char. This
 * relies on the string being null terminated.
 *
 * @param str
 * @param old_char
 * @param new_char
 */
void replace_char(char* str, char old_char, char new_char) {
    for (size_t i = 0; i < strlen(str); i++) {
        if (str[i] == old_char) {
            str[i] = new_char;
        }
    }
}

/**
 * Merge all files in from src to file with name dest synchronously
 * by reading each file, and writing its contents to the dest file.
 * @param src_dir - char pointer to src directory holding files to merge.
 * @param file_url - char pointer to the name of the file's url.
 * @param bytes - The maximum byte size downloaded.
 * @param tasks - The tasks needed for the multipart download.
 */
void merge_files(char* src_dir, char* file_url, int bytes, int tasks) {
    int dest_name_len = strlen(src_dir) + 1 + strlen(file_url) + 1;
    char dest_name[dest_name_len];

    replace_char(file_url, '/', '_');
    snprintf(dest_name, dest_name_len, "%s/%s", src_dir, file_url);

    FILE* dest_file = fopen(dest_name, "w");

    if (dest_file == NULL) {
        return;
    }

    // The maximum amount of bytes required to represent the largest task
    // number.
    int max_bytes_len = snprintf(NULL, 0, "%d", bytes * tasks);
    int src_name_len = strlen(src_dir) + 2 + max_bytes_len;
    char src_filename[src_name_len + max_bytes_len];

    char* buffer = malloc(bytes * sizeof(char));
    for (int i = 0; i < tasks; i++) {
        int src_file_num = bytes * i;
        snprintf(src_filename, src_name_len, "%s/%d", src_dir, src_file_num);

        if (write_to_dest(dest_file, src_filename, buffer, bytes, i, tasks) !=
            0) {
            break;
        };
    }

    free(buffer);
    fclose(dest_file);
}

int main(int argc, char** argv) {
    if (argc != 4) {
        fprintf(stderr,
                "usage: ./downloader url_file num_workers download_dir\n");
        exit(1);
    }

    char* url_file = argv[1];
    int num_workers = atoi(argv[2]);
    char* download_dir = argv[3];

    create_directory(download_dir);
    FILE* fp = fopen(url_file, "r");
    char* line = NULL;
    size_t len = 0;

    if (fp == NULL) {
        exit(EXIT_FAILURE);
    }

    // spawn threads and create work queue(s)
    Context* context = spawn_workers(num_workers);

    int work = 0, bytes = 0, num_tasks = 0;
    while ((len = getline(&line, &len, fp)) != -1) {

        if (line[len - 1] == '\n') {
            line[len - 1] = '\0';
        }

        num_tasks = get_num_tasks(line, num_workers);
        bytes = get_max_chunk_size();

        for (int i = 0; i < num_tasks; i++) {
            ++work;
            queue_put(context->todo,
                      new_task(line, i * bytes, ((i + 1) * bytes) - 1));
        }

        // Get results back
        while (work > 0) {
            --work;
            wait_task(download_dir, context);
        }

        /* Merge the files -- simple synchronous method
         * Then remove the chunked download files
         * Beware, this is not an efficient method
         */
        merge_files(download_dir, line, bytes, num_tasks);
    }

    // cleanup
    fclose(fp);
    free(line);

    free_workers(context);

    return 0;
}
