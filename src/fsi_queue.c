#include <inttypes.h>
#include <stddef.h>

#include "../include/thr_thread.h"
#include "../include/fsi_queue.h"

int fsi_queue_pop(fsi_t_queue* q, void* dst_ptr) {
    thr_mtx_lock(&q->lock);
    if(q->count == 0) {
        //thread frees lock while waiting to avoid deadlocks
        thr_cond_wait(&q->is_not_empty, &q->lock);
    }

    uint8_t* src_ptr = q->items + (q->head * q->item_size);
    memcpy(dst_ptr, src_ptr, q->item_size);
    //start at beginning if CWL_QUE_SIZE is reached. Basically a ring.
    q->head = (q->head + 1) % q->capacity;
    q->count--;
    thr_cond_notify_all(&q->is_not_full);
    thr_mtx_unlock(&q->lock);
    return 1;
}

int fsi_queue_push(fsi_t_queue* q, void* item) {
    thr_mtx_lock(&q->lock);
    if(q->count == q->capacity) {
        //thread frees lock while waiting to avoid deadlocks
        thr_cond_wait(&q->is_not_full, &q->lock);
    }
    uint8_t* dst_ptr = q->items + (q->tail * q->item_size);
    memcpy(dst_ptr, item, q->item_size);
    q->tail = (q->tail + 1) % q->capacity;
    q->count++;
    thr_cond_notify_all(&q->is_not_empty);
    thr_mtx_unlock(&q->lock);
    return 1;
}

fsi_t_queue* fsi_create_queue(size_t item_count, size_t item_size) {
    fsi_t_queue* queue = (fsi_t_queue*)malloc(sizeof(fsi_t_queue));
    if(queue == NULL) {
        return NULL;
    }

    uint8_t* items = (uint8_t*)calloc(1, item_count * item_size);
    if(items == NULL) {
        free(queue);
        return NULL;
    }

    queue->items = items;
    return queue;
}