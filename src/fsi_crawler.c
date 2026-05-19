#include "../include/thr_thread.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_queue.h"
#include "../include/fsi_utils.h"

#include <stddef.h>

#define CWL_QUE_SIZE 50000

void fsi_cwl_crawl_process(void* args);

fsi_t_cwl_crawler* fsi_create_crawler(fsi_t_cwl_config* cfg) {
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

    fsi_t_cwl_context context = crawler->context;
    //create queues
    fsi_t_queue* dir_queue = fsi_create_queue(CWL_QUE_SIZE, sizeof(fsi_file_info));
    if(dir_queue == NULL) {
        return NULL;
    }

    fsi_t_queue* file_queue = fsi_create_queue(CWL_QUE_SIZE, sizeof(fsi_file_info));
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
    context.threads = threads;

    return crawler;
}

void fsi_cwl_start_crawler(fsi_t_cwl_crawler* crawler, char** root_paths, size_t path_count) {
    if(crawler == NULL) {
        fsi_utils_report_error("crawler is null. Cannot start crawling.");
        return;
    }

    fsi_t_cwl_context context = crawler->context;

    for (size_t i = 0; i < path_count; i++) {
        fsi_file_info file_info = {
            .path = root_paths[i]
        };
        //item on stack is copied to queue
        fsi_queue_push(context.dir_queue, &file_info);
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
    fsi_file_info item;
    while(fsi_queue_pop(context->dir_queue, &item)) {
        fsi_cwl_process_dir(context, &item);
    }
}

void fsi_cwl_process_dir(fsi_t_cwl_context* context, fsi_file_info* item) {
    fsi_os_directory dir = {};
    if(fsi_os_directory_open(&dir, item->path) != 0) {
        fsi_utils_report_error("Cannot open directory %s", item->path);
        return;
    }

    fsi_file_info file_info = {};
    while(fsi_os_directory_next(&dir, &file_info) == 0) {
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

void fsi_cwl_stop_crawler(fsi_t_cwl_crawler* crawler) {

}