#include <inttypes.h>
#include <stddef.h>
#include <string.h>
#include <stdlib.h>

#include "../include/thr_thread.h"
#include "../include/fsi_queue.h"

/**
 * @returns
 * 1  - item found
 * -1 - queue shutdown and no more items to pop
 */
int fsi_queue_pop(fsi_t_queue* q, void* dst_ptr) {
    thr_mtx_lock(&q->lock);
    while(q->count == 0) {
        //when we are waiting for is_not_empty and the queue was shutdown, we can exit because no more queue items are expected
        if(q->shutdown) {
            thr_mtx_unlock(&q->lock);
            return FSI_QUEUE_SHUTDOWN;
        }
        //thread frees lock while waiting to avoid deadlocks
        thr_cond_wait(&q->is_not_empty, &q->lock);
    }
    uint8_t* src_ptr = q->items + (q->head * q->item_size);
    memcpy(dst_ptr, src_ptr, q->item_size);
    //start at beginning if q->capacity is reached. Basically a ring.
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    thr_cond_notify_all(&q->is_not_full);
    thr_mtx_unlock(&q->lock);
    return FSI_QUEUE_OK;
}

int fsi_queue_push(fsi_t_queue* q, void* item) {
    thr_mtx_lock(&q->lock);
    while(q->count == q->capacity) {
        //thread frees lock while waiting to avoid deadlocks
        thr_cond_wait(&q->is_not_full, &q->lock);
    }
    uint8_t* dst_ptr = q->items + (q->tail * q->item_size);
    memcpy(dst_ptr, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    q->in_process++;
    thr_cond_notify_all(&q->is_not_empty);
    thr_mtx_unlock(&q->lock);
    return FSI_QUEUE_OK;
}

fsi_t_queue* fsi_queue_create(size_t item_count, size_t item_size) {
    fsi_t_queue* queue = (fsi_t_queue*)malloc(sizeof(fsi_t_queue));
    if(queue == NULL) {
        return NULL;
    }

    thr_mtx_init(&queue->lock);
    thr_cond_init(&queue->is_not_empty);
    thr_cond_init(&queue->is_not_full);

    uint8_t* items = (uint8_t*)calloc(1, item_count * item_size);
    if(items == NULL) {
        free(queue);
        return NULL;
    }

    queue->items = items;
    queue->capacity = item_count;
    queue->count = 0;
    queue->item_size = item_size;
    queue->tail = 0;
    queue->head = 0;
    queue->shutdown = 0;
    queue->in_process = 0;
    
    return queue;
}

void fsi_queue_destroy(fsi_t_queue* queue) {
    if(queue == NULL) {
        return;
    }

    free(queue->items);
    free(queue);
}

void fsi_queue_shutdown(fsi_t_queue* queue) {
    if(queue->shutdown) {
        return;
    }

    thr_mtx_lock(&queue->lock);
    queue->shutdown = 1;
    //wake up waiting threads
    thr_cond_notify_all(&queue->is_not_empty);
    thr_mtx_unlock(&queue->lock);
}

void fsi_queue_item_done_try_shutdown(fsi_t_queue* q) {
    thr_mtx_lock(&q->lock);
    q->in_process--;
    if(q->in_process == 0) {
        q->shutdown = 1;
        thr_cond_notify_all(&q->is_not_empty);
    }
    thr_mtx_unlock(&q->lock);
}