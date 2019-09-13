#include "queue.h"

#include <assert.h>
#include <errno.h>
#include <pthread.h>
#include <semaphore.h>
#include <stdio.h>
#include <stdlib.h>

#define handle_error_en(en, msg)                                               \
    do {                                                                       \
        errno = en;                                                            \
        perror(msg);                                                           \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

#define handle_error(msg)                                                      \
    do {                                                                       \
        perror(msg);                                                           \
        exit(EXIT_FAILURE);                                                    \
    } while (0)

/*
 * Queue - the abstract type of a concurrent queue.
 * You must provide an implementation of this type
 * but it is hidden from the outside.
 */
typedef struct QueueStruct {
    void** data;
    int size;
    int head;
    int tail;

    pthread_mutex_t mutex;
    sem_t empty;
    sem_t full;
} Queue;

/**
 * Allocate a concurrent queue of a specific size
 * @param size - The size of memory to allocate to the queue
 * @return queue - Pointer to the allocated queue
 */
Queue* queue_alloc(int size) {
    Queue* q = malloc(sizeof(Queue));
    q->data = malloc(sizeof(void*) * size);

    q->size = size;
    q->head = 0;
    q->tail = 0;

    pthread_mutex_init(&q->mutex, NULL);

    sem_init(&q->empty, 0, size);
    sem_init(&q->full, 0, 0);

    return q;
}

/**
 * Free a concurrent queue and associated memory
 *
 * Don't call this function while the queue is still in use.
 * (Note, this is a pre-condition to the function and does not need
 * to be checked)
 *
 * @param queue - Pointer to the queue to free
 */
void queue_free(Queue* queue) {
    pthread_mutex_destroy(&queue->mutex);
    sem_destroy(&queue->empty);
    sem_destroy(&queue->full);

    free(queue->data);
    free(queue);
}

/**
 * Place an item into the concurrent queue.
 * If no space available then queue will block
 * until a space is available when it will
 * put the item into the queue and immediately return
 *
 * @param queue - Pointer to the queue to add an item to
 * @param item - An item to add to queue. Uses void* to hold an arbitrary
 *               type. User's responsibility to manage memory and ensure
 *               it is correctly typed.
 */
void queue_put(Queue* queue, void* item) {
    sem_wait(&queue->empty); // decrement empty count
    pthread_mutex_lock(&queue->mutex);

    queue->data[queue->tail] = item;
    queue->tail = (queue->tail + 1) % queue->size;

    pthread_mutex_unlock(&queue->mutex);
    sem_post(&queue->full); // increment count of full slots
}

/**
 * Get an item from the concurrent queue
 *
 * If there is no item available then queue_get
 * will block until an item becomes available when
 * it will immediately return that item.
 *
 * @param queue - Pointer to queue to get item from
 * @return item - item retrieved from queue. void* type since it can be
 *                arbitrary
 */
void* queue_get(Queue* queue) {
    sem_wait(&queue->full); // decrement full count
    pthread_mutex_lock(&queue->mutex);

    void* item = queue->data[queue->head];
    queue->head = (queue->head + 1) % queue->size;

    pthread_mutex_unlock(&queue->mutex);
    sem_post(&queue->empty); // increment count of empty slots

    return item;
}
