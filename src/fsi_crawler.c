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

    fsi_t_cwl_context* context = &crawler->context;
    //create queues
    fsi_t_queue* dir_queue = fsi_create_queue(CWL_QUE_SIZE, sizeof(fsi_file_info));
    if(dir_queue == NULL) {
        return NULL;
    }

    fsi_t_queue* file_queue = fsi_create_queue(CWL_QUE_SIZE, sizeof(fsi_file_info));
    if(file_queue == NULL) {
        return NULL;
    }

    context->dir_queue = dir_queue;
    context->file_queue = file_queue;
    context->thread_count = cfg->parallelismn;
    
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
        fsi_file_info file_info = {};
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

void fsi_cwl_crawl_process(void* args) {
    printf("Crawler Thread %" PRId64 " running.\n", thr_get_id());
    thr_t_thread_args* thread_args = (thr_t_thread_args*)args;
    fsi_t_cwl_context* context = (fsi_t_cwl_context*)thread_args->context;
    thr_t_thread* thread = (thr_t_thread*)thread_args->thread;
    fsi_file_info item;
    while(fsi_queue_pop(context->dir_queue, &item)) {
        //printf("Pop dir [q=dir]:   %s\n", item.path);
        fsi_cwl_process_dir(context, &item, thread);
    }
    free(thread_args);
}

void fsi_cwl_process_dir(fsi_t_cwl_context* context, fsi_file_info* item, thr_t_thread* thread) {
    fsi_os_directory dir = {};
    //printf("Open dir %s\n", item->path);

    if(fsi_os_directory_open(&dir, item->path) != 0) {
        fsi_utils_report_error("Cannot open directory %s", item->path);
        return;
    }

    fsi_file_info file_info = {};
    while((fsi_os_directory_next(&dir, &file_info) == 0) && !thread->interrupted) {
        //printf("Found:  %s\n", file_info.path);
        // Skip the "." and ".." items to avoid infinite loops
        if (strcmp(file_info.file_name, ".") == 0 || strcmp(file_info.file_name, "..") == 0) {
            continue;
        }
        if(file_info.type & FSI_OS_DIR) {
            //printf("Push dir [q=dir]:  %s\n", file_info.path);
            fsi_queue_push(context->dir_queue, &file_info);
        } else if(file_info.type & FSI_OS_FILE) {
            //printf("Push file [q=file]: %s\n", file_info.path);
            fsi_queue_push(context->file_queue, &file_info);
        }
    }
    //printf("Finished dir %s", item->path);
}

void fsi_cwl_stop_crawler(fsi_t_cwl_crawler* crawler) {

}