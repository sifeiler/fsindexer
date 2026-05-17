#include "../include/thr_thread.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_utils.h"

#define CWL_QUE_SIZE 50000

int fsi_cwl_queue_pop(fsi_t_cwl_queue* q, fsi_t_cwl_item* item) {
    thr_mtx_lock(&q->lock);
    if(q->count == 0) {
        //thread frees lock while waiting to avoid deadlocks
        thr_cond_wait(&q->is_not_empty, &q->lock);
    }
    fsi_t_cwl_item* popped = q->items[q->head];
    if(popped == NULL) {
        item = NULL;
        return 0;
    }
    memcpy(item, popped, sizeof(fsi_t_cwl_item));
    //start at beginning if CWL_QUE_SIZE is reached. Basically a ring.
    q->head = (q->head + 1) % CWL_QUE_SIZE;
    q->count--;
    thr_cond_notify_all(&q->is_not_full);
    thr_mtx_unlock(&q->lock);
    return 1;
}

int fsi_cwl_queue_push(fsi_t_cwl_queue* q, fsi_t_cwl_item* item) {
    thr_mtx_lock(&q->lock);
    if(q->count == CWL_QUE_SIZE) {
        //thread frees lock while waiting to avoid deadlocks
        thr_cond_wait(&q->is_not_full, &q->lock);
    }
    memcpy(&(q->items[q->tail]), item, sizeof(fsi_t_cwl_item));
    q->tail = (q->tail + 1) % CWL_QUE_SIZE;
    q->count++;
    thr_cond_notify_all(&q->is_not_empty);
    thr_mtx_unlock(&q->lock);
    return 1;
}

fsi_t_cwl_queue* fsi_create_crawler_queue(size_t size) {
    fsi_t_cwl_queue* queue = malloc(sizeof(fsi_t_cwl_queue));
    if(queue == NULL) {
        return NULL;
    }

    fsi_t_cwl_item** items = malloc(size * sizeof(fsi_t_cwl_item*));
    if(items == NULL) {
        free(queue);
        return NULL;
    }

    queue->items = items;
    return queue;
}

fsi_t_cwl_crawler* fsi_create_crawler(fsi_t_cwl_config* cfg) {
    if(cfg == NULL) {
        fsi_utils_report_error("crawler config is null. Cannot create crawler.");
        return;
    }

    if(cfg->parallelismn < 1) {
        fsi_utils_report_error("crawler config invalid. parallelism has to be at least 1.");
        return;
    }

    fsi_t_cwl_crawler* crawler = malloc(sizeof(fsi_t_cwl_crawler));
    if(crawler == NULL) {
        return NULL;
    }

    fsi_t_cwl_context context = crawler->context;
    //create queues
    fsi_t_cwl_queue* dir_queue = fsi_create_crawler_queue(CWL_QUE_SIZE);
    if(dir_queue == NULL) {
        return NULL;
    }

    fsi_t_cwl_queue* file_queue = fsi_create_crawler_queue(CWL_QUE_SIZE);
    if(file_queue == NULL) {
        return NULL;
    }

    context.dir_queue = dir_queue;
    context.file_queue = file_queue;
    context.thread_count = cfg->parallelismn;
    
    //create threads
    thr_t_thread* threads = malloc(cfg->parallelismn * sizeof(thr_t_thread));
    if(threads == NULL) {
        fsi_utils_report_error("Cannot create crawler. Threads could not be allocated.");
        free(dir_queue);
        free(file_queue);
        free(crawler);
        return NULL;
    }

    return crawler;
}

void fsi_cwl_start_crawler(fsi_t_cwl_crawler* crawler, char** root_paths, size_t path_count) {
    if(crawler == NULL) {
        fsi_utils_report_error("crawler is null. Cannot start crawling.");
        return;
    }

    fsi_t_cwl_context context = crawler->context;

    for (size_t i = 0; i < path_count; i++) {
        fsi_t_cwl_item item = {
            .path = root_paths[i]
        };
        //item on stack is copied to queue
        fsi_cwl_queue_push(context.dir_queue, &item);
    }

    for (size_t i = 0; i < context.thread_count; i++) {
        if(thr_thread_create(context.threads[i], fsi_cwl_crawl_process, &context) != 0) {
            fsi_utils_report_error("Cannot start crawler. Threads could not be initialized and started.");
            return NULL;
        }
    }
}

void fsi_cwl_crawl_process(void* args) {
    fsi_t_cwl_context* context = (fsi_t_cwl_context*)args;
    fsi_t_cwl_item item;
    fsi_cwl_queue_pop(context->dir_queue, &item);
    while(fsi_cwl_queue_pop(context->dir_queue, &item)) {
        fsi_cwl_process_item(&item);
    }
}

void fsi_cwl_process_item(fsi_t_cwl_item* item) {

}

void fsi_cwl_stop_crawler(fsi_t_cwl_crawler* crawler) {

}