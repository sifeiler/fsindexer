#include <stddef.h>

#include "../include/thr_thread.h"
#include "../include/fsi_worker.h"
#include "../include/fsi_queue.h"
#include "../include/fsi_utils.h"

void fsi_wrk_work_process(void* args);

fsi_t_wrk_worker* fsi_wrk_create_worker(fsi_t_wrk_config* cfg) {
    if(cfg == NULL) {
        fsi_utils_report_error("worker config is null. Cannot create worker.");
        return;
    }

    if(cfg->parallelismn < 1) {
        fsi_utils_report_error("worker config invalid. parallelism has to be at least 1.");
        return;
    }

    fsi_t_wrk_worker* worker = malloc(sizeof(fsi_t_wrk_worker));
    if(worker == NULL) {
        fsi_utils_report_error("Cannot create worker.");
        return NULL;
    }

    //create threads
    thr_t_thread* threads = malloc(cfg->parallelismn * sizeof(thr_t_thread));
    if(threads == NULL) {
        fsi_utils_report_error("Cannot create worker. Threads could not be allocated.");
        free(worker);
        return NULL;
    }
    worker->context.threads = threads;
    worker->context.thread_count = cfg->parallelismn;
    return worker;
}

void fsi_wrk_start_worker(fsi_t_wrk_worker* worker) {
    if(worker == NULL) {
        fsi_utils_report_error("worker is null. Cannot start working.");
        return;
    }

    fsi_t_wrk_context context = worker->context;

    for (size_t i = 0; i < context.thread_count; i++) {
        if(thr_thread_create(context.threads[i], fsi_wrk_work_process, &context) != 0) {
            fsi_utils_report_error("Cannot start worker. Threads could not be initialized and started.");
            return NULL;
        }
    }
    
}

void fsi_wrk_work_process(void* args) {
    fsi_t_wrk_context* context = (fsi_t_wrk_context*)args;
    fsi_t_queue* file_queue = context->file_queue;
    fsi_file_info file;
    while(fsi_queue_pop(file_queue, &file)) {
        fsi_wrk_process_file(context, &file);
    }
}

void fsi_wrk_process_file(fsi_t_wrk_context* context, fsi_file_info* file) {
    //extract file information, create file item for permanent storage and push to storage queue
}

void fsi_wrk_stop_worker(fsi_t_wrk_worker* worker) {

}