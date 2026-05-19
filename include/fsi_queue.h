#ifndef FSI_QUEUE_H
#define FSI_QUEUE_H

#include <inttypes.h>
#include "thr_thread.h"
#include <stddef.h>

typedef struct fsi_t_queue {
    uint8_t* items;
    size_t item_size;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    thr_t_mutex lock;
    thr_t_cond is_not_full;
    thr_t_cond is_not_empty;
} fsi_t_queue;

int fsi_queue_pop(fsi_t_queue* q, void* dst_ptr);
int fsi_queue_push(fsi_t_queue* q, void* item);
fsi_t_queue* fsi_create_queue(size_t item_count, size_t item_size);

#endif