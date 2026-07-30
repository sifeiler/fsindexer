#include "unity.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#include "../include/fsi_os.h"
#include "../include/fsi_crawler.h"
#include "../src/fsi_crawler.c"
#include "fsi_utils_fuzzy.h"
#include "fsi_utils_test.h"

#define TEST_DIR "test_crawl_dir"

long get_number_after_last_underscore(const char* filename) {
    const char* last_underscore = strrchr(filename, '_');
    if (!last_underscore) {
        return -1;
    }

    char* endptr;
    long value = strtol(last_underscore + 1, &endptr, 10);

    if (endptr == last_underscore + 1) {
        return -1;   // nothing after the underscore was a valid number
    }

    return value;
}

static void create_recursive_test_tree(const char* path, uint64_t* files_remaining, uint64_t* files_counter, uint64_t* dir_counter, uint64_t curr_depth, const uint64_t max_files_per_dir, const uint64_t max_depth) {
    fsi_os_directory_create(path);
    
    char new_path[512];
    
    for(int i = 0; i < max_files_per_dir && *files_remaining > 0; i++) {
        snprintf(new_path, sizeof(new_path), "%s/file_%" PRId64, path, *files_counter);
        fsi_os_file_create(new_path);
        (*files_counter)++;
        (*files_remaining)--;
    }

    if(curr_depth < max_depth && *files_remaining > 0) {
        char new_dir_path[512];
        snprintf(new_dir_path, sizeof(new_dir_path), "%s/dir_%" PRId64, path, *dir_counter);
        (*dir_counter)++;
        create_recursive_test_tree(new_dir_path, files_remaining, files_counter, dir_counter, curr_depth + 1, max_files_per_dir, max_depth);
    }
}

void setUp(void) {}

void tearDown(void) {
    remove_test_dir_recursive(TEST_DIR);
}

static void fsi_test_crawler(const uint64_t count) {
    uint64_t remaining = count;
    uint64_t file_counter = 0;
    uint64_t dir_counter = 0;
    char test_dir[FSI_PATH_MAX];
    fsi_get_test_root(test_dir, FSI_PATH_MAX);
    create_recursive_test_tree(test_dir, &remaining, &file_counter, &dir_counter, 0, count, 0);

    fsi_file_info* expected_files = calloc(count, sizeof(fsi_file_info));

    for(int i = 0; i < count; i++) {
        fsi_file_info* expected = &expected_files[i];
        uint64_t name_len = snprintf(expected->file_name, sizeof(expected->file_name), "file_%d", i);
        uint64_t path_len = snprintf(expected->file_path, sizeof(expected->file_path), "%s%s%s", test_dir, FSI_PATH_DELIMITER_STR, expected->file_name);
        expected->type = FSI_OS_FILE;
        expected->file_name_len = name_len;
        expected->file_path_len = path_len;
    }

    fsi_t_cwl_config cwl_cfg = {
        .parallelismn = 1
    };

    fsi_t_cwl_crawler* crawler = fsi_cwl_create_crawler(&cwl_cfg);
    char root_path[FSI_PATH_MAX];
    if(fsi_os_full_path(test_dir, root_path) == -1) {
        TEST_FAIL_MESSAGE("Cannot run test fsi_test_crawler because full root_path of test directory cannot be evaluated.");
    }
    const char* path_list[] = { root_path };

    fsi_cwl_crawler_init(crawler, path_list, 1);

    int interrupted = 0;
    fsi_cwl_crawl_process_loop(&crawler->context, &interrupted, FSI_QUEUE_NON_BLOCKING);

    fsi_file_info* actual_files = calloc(count, sizeof(fsi_file_info));
    fsi_file_info entry = {0};
    
    fsi_t_cwl_context* context = &crawler->context;
    fsi_file_info dir_entry = {0};
    //needed because otherwise queue is blocking when empty
    while (context->file_queue->count > 0) {
        if (fsi_queue_pop(context->file_queue, &entry, FSI_QUEUE_NON_BLOCKING) == FSI_QUEUE_OK) {
            long id = get_number_after_last_underscore(entry.file_name);
            if (id >= 0 && id < (long)count) {
                actual_files[id] = entry;
            }
        }
    }

    for(int i = 0; i < count; i++) {
        fsi_file_info expected = expected_files[i];
        fsi_file_info actual = actual_files[i];
        TEST_ASSERT_EQUAL_MEMORY(expected.type, actual.type, sizeof(FSI_OS_FILE_TYPE));
        TEST_ASSERT_EQUAL_MEMORY(expected.file_name, actual.file_name, expected.file_name_len);
        TEST_ASSERT_EQUAL_MEMORY(expected.file_path, actual.file_path, expected.file_path_len);
    }


    free(expected_files);
    free(actual_files);
    fsi_cwl_destroy_crawler(crawler);
}

void fsi_test_crawler_100(void) {
    fsi_test_crawler(100);
}

void fsi_test_crawler_1000(void) {
    fsi_test_crawler(1000);
}

void fsi_test_crawler_5000(void) {
    fsi_test_crawler(5000);
}


int main(void) {
    UNITY_BEGIN();
    RUN_TEST(fsi_test_crawler_100);
    RUN_TEST(fsi_test_crawler_1000);
    RUN_TEST(fsi_test_crawler_5000);
    return UNITY_END();
}