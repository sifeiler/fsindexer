#ifndef FSI_QUEUE_H
#define FSI_QUEUE_H

#include <inttypes.h>
#include "thr_thread.h"
#include <stddef.h>

#define FSI_QUEUE_SHUTDOWN -1
#define FSI_QUEUE_OK        1

typedef struct fsi_t_queue {
    uint8_t* items;
    size_t item_size;
    size_t capacity;
    size_t head;
    size_t tail;
    size_t count;
    uint64_t in_process;
    int shutdown;
    thr_t_mutex lock;
    thr_t_cond is_not_full;
    thr_t_cond is_not_empty;
} fsi_t_queue;

int fsi_queue_pop(fsi_t_queue* q, void* dst_ptr);
int fsi_queue_push(fsi_t_queue* q, void* item);
fsi_t_queue* fsi_queue_create(size_t item_count, size_t item_size);
void fsi_queue_destroy(fsi_t_queue* queue);
void fsi_queue_shutdown(fsi_t_queue* queue);
void fsi_queue_item_done_try_shutdown(fsi_t_queue* q);

#endif