#include "../include/thr_thread.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_queue.h"
#include "../include/fsi_utils.h"

#include <stddef.h>

#define CWL_QUE_SIZE 50000

void fsi_cwl_crawl_process(void* args);
void fsi_cwl_process_dir(fsi_t_cwl_context* context, fsi_file_info* item, thr_t_thread* thread);

fsi_t_cwl_crawler* fsi_cwl_create_crawler(fsi_t_cwl_config* cfg) {
    if(cfg == NULL) {
        fsi_utils_report_error("crawler config is null. Cannot create crawler.");
        return NULL;
    }

    if(cfg->parallelismn < 1) {
        fsi_utils_report_error("crawler config invalid. parallelism has to be at least 1.");
        return NULL;
    }

    fsi_t_cwl_crawler* crawler = malloc(sizeof(fsi_t_cwl_crawler));
    if(crawler == NULL) {
        fsi_utils_report_error("Cannot allocate crawler.");
        return NULL;
    }

    thr_mtx_init(&crawler->context.lock);

    fsi_t_cwl_context* context = &crawler->context;
    //create queues
    fsi_t_queue* dir_queue = fsi_queue_create(CWL_QUE_SIZE, sizeof(fsi_file_info));
    if(dir_queue == NULL) {
        return NULL;
    }

    fsi_t_queue* file_queue = fsi_queue_create(CWL_QUE_SIZE, sizeof(fsi_file_info));
    if(file_queue == NULL) {
        return NULL;
    }

    context->dir_queue = dir_queue;
    context->file_queue = file_queue;
    context->thread_count = cfg->parallelismn;
    context->threads_all_done = 0;
    context->threads_done_count = 0;
    
    //create threads
    thr_t_thread* threads = malloc(cfg->parallelismn * sizeof(thr_t_thread));
    if(threads == NULL) {
        fsi_utils_report_error("Cannot create crawler. Threads could not be allocated.");
        free(dir_queue);
        free(file_queue);
        free(crawler);
        return NULL;
    }
    context->threads = threads;

    return crawler;
}

int fsi_cwl_start_crawler(fsi_t_cwl_crawler* crawler, const char** root_paths, size_t path_count) {
    if(crawler == NULL) {
        fsi_utils_report_error("crawler is null. Cannot start crawling.");
        return -1;
    }

    fsi_t_cwl_context* context = &crawler->context;

    for (size_t i = 0; i < path_count; i++) {
        fsi_file_info file_info = {0};
        strncpy(file_info.path, root_paths[i], FSI_PATH_MAX - 1);
        //item on stack is copied to queue
        fsi_queue_push(context->dir_queue, &file_info);
    }

    for (size_t i = 0; i < context->thread_count; i++) {
        thr_t_thread_args* args = malloc(sizeof(thr_t_thread_args));
        args->thread = &context->threads[i];
        args->context = context;
        context->threads[i].interrupted = 0;
        if(thr_thread_create(&context->threads[i], fsi_cwl_crawl_process, args) != 0) {
            fsi_utils_report_error("Cannot start crawler. Threads could not be initialized and started.");
            return -1;
        }
    }
    return 0;
}

//TODO: fuse this with fsi_wrk_set_thread_done
void fsi_cwl_set_thread_done(fsi_t_cwl_context* context) {
    thr_mtx_lock(&context->lock);
    context->threads_done_count++;
    context->threads_all_done = (context->thread_count == context->threads_done_count);
    thr_mtx_unlock(&context->lock);

    if(context->threads_all_done) {
        //crawler owns both, directory and file queue, so when crawler is done we signal a queue shutdown
        fsi_queue_shutdown(context->dir_queue);
        fsi_queue_shutdown(context->file_queue);
    }
}

void fsi_cwl_crawl_process(void* args) {
    printf("Crawler Thread %" PRId64 ": start\n", thr_get_id());
    thr_t_thread_args* thread_args = (thr_t_thread_args*)args;
    fsi_t_cwl_context* context = (fsi_t_cwl_context*)thread_args->context;
    thr_t_thread* thread = (thr_t_thread*)thread_args->thread;
    fsi_file_info item = {0};
    while(fsi_queue_pop(context->dir_queue, &item) == FSI_QUEUE_OK) {
        fsi_cwl_process_dir(context, &item, thread);
        fsi_queue_item_done_try_shutdown(context->dir_queue);
    }
    free(thread_args);
    fsi_cwl_set_thread_done(context);
    printf("Crawler Thread %" PRId64 ": finished\n", thr_get_id());
}

void fsi_cwl_process_dir(fsi_t_cwl_context* context, fsi_file_info* item, thr_t_thread* thread) {
    fsi_os_directory dir = {0};

    if(fsi_os_directory_open(&dir, item->path) != 0) {
        fsi_utils_report_error("Cannot open directory %s", item->path);
        return;
    }

    fsi_file_info file_info = {0};
    while((fsi_os_directory_next(&dir, &file_info) == 0) && !thread->interrupted) {
        // Skip the "." and ".." items to avoid infinite loops
        if (strcmp(file_info.file_name, ".") == 0 || strcmp(file_info.file_name, "..") == 0) {
            continue;
        }
        if(file_info.type & FSI_OS_DIR) {
            fsi_queue_push(context->dir_queue, &file_info);
        } else if(file_info.type & FSI_OS_FILE) {
            fsi_queue_push(context->file_queue, &file_info);
        }
    }
}

/**
 * Join all crawler threads.
*/
void fsi_cwl_join_crawler(fsi_t_cwl_crawler* crawler) {
    printf("Joining crawler threads...\n");
    for (size_t i = 0; i < crawler->context.thread_count; i++) {
        thr_thread_join(&crawler->context.threads[i]);
    }
}

void fsi_cwl_stop_crawler(fsi_t_cwl_crawler* crawler) {
    printf("Interrupting crawler threads...\n");
    for (size_t i = 0; i < crawler->context.thread_count; i++) {
        crawler->context.threads[i].interrupted = 1;
    }
}

/**
 * Make sure that threads are no longer running.
 * Call fsi_cwl_join_crawler first, then fsi_cwl_stop_crawler, and then this function.
 */
void fsi_cwl_destroy_crawler(fsi_t_cwl_crawler* crawler) {
    printf("Destroying crawler threads...\n");
    
    fsi_queue_destroy(crawler->context.dir_queue);
    fsi_queue_destroy(crawler->context.file_queue);

    for (size_t i = 0; i < crawler->context.thread_count; i++) {
        thr_thread_destroy(&crawler->context.threads[i]);
    }

    free(crawler);
}