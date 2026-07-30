#include <stddef.h>
#include <string.h>

#include "../include/thr_thread.h"
#include "../include/fsi_worker.h"
#include "../include/fsi_queue.h"
#include "../include/fsi_utils.h"
#include "../include/fsi_indexer.h"

void fsi_wrk_work_process(void* args);
void fsi_wrk_process_file(fsi_t_wrk_context* context, fsi_file_info* file);

fsi_t_wrk_worker* fsi_wrk_create_worker(fsi_t_wrk_config* cfg) {
    if(cfg == NULL) {
        fsi_utils_report_error("worker config is null. Cannot create worker.");
        return NULL;
    }

    if(cfg->parallelismn < 1) {
        fsi_utils_report_error("worker config invalid. parallelism has to be at least 1.");
        return NULL;
    }

    fsi_t_wrk_worker* worker = malloc(sizeof(fsi_t_wrk_worker));
    if(worker == NULL) {
        fsi_utils_report_error("Cannot create worker.");
        return NULL;
    }

    thr_mtx_init(&worker->context.lock);

    //create threads
    thr_t_thread* threads = malloc(cfg->parallelismn * sizeof(thr_t_thread));
    if(threads == NULL) {
        fsi_utils_report_error("Cannot create worker. Threads could not be allocated.");
        free(worker);
        return NULL;
    }
    worker->context.threads = threads;
    worker->context.thread_count = cfg->parallelismn;
    worker->context.threads_all_done = 0;
    worker->context.threads_done_count = 0;
    return worker;
}

int fsi_wrk_start_worker(fsi_t_wrk_worker* worker) {
    if(worker == NULL) {
        fsi_utils_report_error("worker is null. Cannot start working.");
        return -1;
    }

    fsi_t_wrk_context* context = &worker->context;

    for (size_t i = 0; i < context->thread_count; i++) {
        thr_t_thread_args* args = malloc(sizeof(thr_t_thread_args));
        args->thread = &context->threads[i];
        args->context = context;
        context->threads[i].interrupted = 0;
        if(thr_thread_create(&context->threads[i], fsi_wrk_work_process, args) != 0) {
            fsi_utils_report_error("Cannot start worker. Threads could not be initialized and started.");
            return -1;
        }
    }
    return 0;
}

void fsi_wrk_set_thread_done(fsi_t_wrk_context* context) {
    thr_mtx_lock(&context->lock);
    context->threads_done_count++;
    context->threads_all_done = (context->thread_count == context->threads_done_count);
    thr_mtx_unlock(&context->lock);

    if(context->threads_all_done) {
        //signal indexer that worker will no longer add items to the queue
        fsi_queue_shutdown(context->storage_queue);
    }
}

static void fsi_wrk_work_process_loop(fsi_t_wrk_context* context, fsi_t_queue_mode mode) {
    fsi_file_info file = {0};
    while(fsi_queue_pop(context->file_queue, &file, mode) == FSI_QUEUE_OK) {
        fsi_wrk_process_file(context, &file);
    }
}

void fsi_wrk_work_process(void* args) {
    printf("Worker Thread %" PRId64 ": start\n", thr_get_id());
    thr_t_thread_args* thread_args = (thr_t_thread_args*)args;
    fsi_t_wrk_context* context = (fsi_t_wrk_context*)thread_args->context;
    thr_t_thread* thread = (thr_t_thread*)thread_args->thread;
    fsi_wrk_work_process_loop(context, FSI_QUEUE_BLOCKING);
    free(args);
    fsi_wrk_set_thread_done(context);
    printf("Worker Thread %" PRId64 ": finished\n", thr_get_id());
}

void fsi_wrk_process_file(fsi_t_wrk_context* context, fsi_file_info* file) {
    if(file->file_id_set == 0) {
        if(fsi_os_get_file_id(file) == -1) {
            fsi_utils_report_error("Cannot read file_id for file %s. Cannot push file to storage queue. Skipping it.", file->file_path);
            return;
        }
    }

    fsi_t_file_data file_data = {
        .file_info = *file
        //later, additional filedata like vectors etc. will be evaluated and put here
    };

    fsi_queue_push(context->storage_queue, &file_data);
}

/**
 * Join all worker threads.
*/
void fsi_wrk_join_worker(fsi_t_wrk_worker* worker) {
    printf("Joining worker threads...");
    for (size_t i = 0; i < worker->context.thread_count; i++) {
        thr_thread_join(&worker->context.threads[i]);
    }
}

void fsi_wrk_stop_worker(fsi_t_wrk_worker* worker) {
    printf("Interrupting worker threads...");
    for (size_t i = 0; i < worker->context.thread_count; i++) {
        worker->context.threads[i].interrupted = 1;
    }
}

/**
 * Make sure that threads are no longer running.
 * Call fsi_wrk_join_worker first, then fsi_wrk_stop_worker, and then this function.
 */
void fsi_wrk_destroy_worker(fsi_t_wrk_worker* worker) {
    //printf("Destroying worker threads...");

    for (size_t i = 0; i < worker->context.thread_count; i++) {
        thr_thread_destroy(&worker->context.threads[i]);
    }

    free(worker->context.threads);
    thr_mtx_destroy(&worker->context.lock);
    free(worker);
}