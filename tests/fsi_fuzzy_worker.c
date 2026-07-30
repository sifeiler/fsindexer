
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <stdlib.h>

#include "../include/fsi_worker.h"
#include "../src/fsi_worker.c"
#include "fsi_utils_fuzzy.h"
#include "fsi_utils_test.h"
#include "mos_string.h"

#define MAX_FILE_NAME_LEN FSI_NAME_MAX
#define MAX_FILES 50000

void fsi_fuzzy_wrk_fill_file_queue(fsi_t_queue* queue, expected_entry_list* expected_list) {
    for(uint64_t i = 0; i < expected_list->count; i++) {
        expected_entry entry = expected_list->entries[i];
        if(entry.type != FSI_OS_FILE) {
            continue;
        }
        fsi_file_info file_info = {0};
        snprintf(file_info.file_name, sizeof(file_info.file_name), "%s", entry.name);
        snprintf(file_info.file_path, sizeof(file_info.file_path), "%s", entry.path);
        file_info.file_name_len = entry.name_len;
        file_info.file_path_len = entry.path_len;
        fsi_queue_push(queue, &file_info);
    }
}

// returns 1 if found and matched, 0 if not found (unexpected entry!), -1 if found but already seen (duplicate!)
int fsi_fuzzy_wrk_expected_entry_check_off(expected_entry_list *entries, fsi_t_file_data* file_data) {
    for (int i = 0; i < entries->count; i++) {
        if ((strlen(entries->entries[i].path) == file_data->file_info.file_path_len)
            && (strncmp(entries->entries[i].path, file_data->file_info.file_path, file_data->file_info.file_path_len) == 0)
            && (strlen(entries->entries[i].name) == file_data->file_info.file_name_len)
            && (strncmp(entries->entries[i].name, file_data->file_info.file_name, file_data->file_info.file_name_len) == 0)) {
            if (entries->entries[i].seen) return -1;            // duplicate pop -- bug!
            if (entries->entries[i].type != FSI_OS_FILE) return 0;     // type mismatch -- bug!
            entries->entries[i].seen = 1;
            return 1;
        }
    }
    return 0;   // path never created -- bug!
}

int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size) {
    if(size == 0) {
        return 0;
    }

    size_t pos = 0;

    char test_path[FSI_PATH_MAX];
    uint64_t test_path_len = fsi_get_test_root(test_path, FSI_PATH_MAX);
    //printf("test_path %s\n", test_path);
    fsi_os_directory_create(test_path);

    expected_entry_list* expected_list = calloc(1, MAX_FILES * sizeof(expected_entry));
    expected_list->count = 0;
    expected_list->size = MAX_FILES;
    create_test_tree_for_fuzzer_data(data, size, &pos, test_path, test_path_len, expected_list, MAX_FILES);

    fsi_t_wrk_config wrk_cfg = {
        .parallelismn = 1
    };

    fsi_t_wrk_worker* worker = fsi_wrk_create_worker(&wrk_cfg);

    if(worker == NULL) {
        remove_test_dir_recursive(test_path);
        free(expected_list);
        return 0;
    }

    const char* path_list[] = { test_path };

    fsi_t_queue* file_queue = fsi_queue_create(MAX_FILES, sizeof(fsi_file_info));
    if(file_queue == NULL) {
        free(expected_list);
        free(worker);
        thr_mtx_destroy(&worker->context.lock);
        return 0;
    }

    fsi_t_queue* storage_queue = fsi_queue_create(MAX_FILES, sizeof(fsi_t_file_data));
    if(storage_queue == NULL) {
        free(expected_list);
        free(worker);
        thr_mtx_destroy(&worker->context.lock);
        return 0;
    }

    worker->context.file_queue = file_queue;
    worker->context.storage_queue = storage_queue;

    fsi_fuzzy_wrk_fill_file_queue(file_queue, expected_list);
    fsi_wrk_work_process_loop(&worker->context, FSI_QUEUE_NON_BLOCKING);
    
    fsi_t_wrk_context* context = &worker->context;
    fsi_t_file_data file_data = {0};
    //needed because otherwise queue is blocking when empty
    while (context->storage_queue->count > 0) {
        if (fsi_queue_pop(context->storage_queue, &file_data, FSI_QUEUE_NON_BLOCKING) == FSI_QUEUE_OK) {
            int result = fsi_fuzzy_wrk_expected_entry_check_off(expected_list, &file_data);
            if (result != 1) {
                abort();   // unexpected, duplicate, or type-mismatched entry -- real bug found
            }
        }
    }

    remove_test_dir_recursive(test_path);
    fsi_wrk_destroy_worker(worker);
    fsi_queue_destroy(file_queue);
    fsi_queue_destroy(storage_queue);
    free(expected_list);
    return 0;
}
