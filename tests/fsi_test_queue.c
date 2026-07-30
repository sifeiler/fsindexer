#include "unity.h"

#include "../include/fsi_queue.h"
#include "../include/fsi_indexer.h"

void setUp(void) {}

void tearDown(void) {}

void fsi_test_queue_push_pop(void) {
    fsi_t_queue* q = fsi_queue_create(10, sizeof(fsi_t_mos_file));

    fsi_t_mos_file file1 = {
        .file_id = 1,
        .file_name = {
            .str = "file1",
            .str_len = 5
        },
        .file_path = {
            .str = "/this/is/file/path/1",
            .str_len = 20
        }
    };
    
    fsi_queue_push(q, &file1);
    fsi_t_mos_file file1_pop;
    fsi_queue_pop(q, &file1_pop, FSI_QUEUE_BLOCKING);
    TEST_ASSERT_EQUAL_MEMORY(&file1, &file1_pop, sizeof(fsi_t_mos_file));
}

void fsi_test_queue_push_pop_multiple(void) {
    fsi_t_queue* q = fsi_queue_create(10, sizeof(fsi_t_mos_file));

    fsi_t_mos_file file1 = {
        .file_id = 1,
        .file_name = {
            .str = "file1",
            .str_len = 5
        },
        .file_path = {
            .str = "/this/is/file/path/1",
            .str_len = 20
        }
    };

    fsi_t_mos_file file2 = {
        .file_id = 2,
        .file_name = {
            .str = "file2",
            .str_len = 5
        },
        .file_path = {
            .str = "/this/is/file/path/2",
            .str_len = 20
        }
    };

    fsi_t_mos_file file3 = {
        .file_id = 3,
        .file_name = {
            .str = "file3",
            .str_len = 5
        },
        .file_path = {
            .str = "/this/is/file/path/3",
            .str_len = 20
        }
    };

    fsi_t_mos_file file4 = {
        .file_id = 4,
        .file_name = {
            .str = "file4",
            .str_len = 5
        },
        .file_path = {
            .str = "/this/is/file/path/4",
            .str_len = 20
        }
    };

    fsi_t_mos_file file5 = {
        .file_id = 5,
        .file_name = {
            .str = "file5",
            .str_len = 5
        },
        .file_path = {
            .str = "/this/is/file/path/5",
            .str_len = 20
        }
    };

    
    fsi_queue_push(q, &file1);
    fsi_queue_push(q, &file2);
    fsi_queue_push(q, &file3);

    fsi_t_mos_file file1_pop;
    fsi_t_mos_file file2_pop;
    fsi_t_mos_file file3_pop;
    fsi_t_mos_file file4_pop;
    fsi_t_mos_file file5_pop;

    fsi_queue_pop(q, &file1_pop, FSI_QUEUE_BLOCKING);
    fsi_queue_push(q, &file4);
    fsi_queue_pop(q, &file2_pop, FSI_QUEUE_BLOCKING);
    fsi_queue_push(q, &file5);
    fsi_queue_pop(q, &file3_pop, FSI_QUEUE_BLOCKING);
    fsi_queue_pop(q, &file4_pop, FSI_QUEUE_BLOCKING);
    fsi_queue_pop(q, &file5_pop, FSI_QUEUE_BLOCKING);

    TEST_ASSERT_EQUAL_MEMORY(&file1, &file1_pop, sizeof(fsi_t_mos_file));
    TEST_ASSERT_EQUAL_MEMORY(&file2, &file2_pop, sizeof(fsi_t_mos_file));
    TEST_ASSERT_EQUAL_MEMORY(&file3, &file3_pop, sizeof(fsi_t_mos_file));
    TEST_ASSERT_EQUAL_MEMORY(&file4, &file4_pop, sizeof(fsi_t_mos_file));
    TEST_ASSERT_EQUAL_MEMORY(&file5, &file5_pop, sizeof(fsi_t_mos_file));
}

void fsi_test_queue_create(void) {
    fsi_t_queue* queue = fsi_queue_create(10, 8);
    
    TEST_ASSERT_EQUAL_INT(queue->shutdown, 0);
    TEST_ASSERT_EQUAL_UINT64(queue->in_process, 0);
    TEST_ASSERT_EQUAL_size_t(queue->tail, 0);
    TEST_ASSERT_EQUAL_size_t(queue->head, 0);
    TEST_ASSERT_EQUAL_size_t(queue->item_size, 8);
    TEST_ASSERT_EQUAL_size_t(queue->count, 0);
    TEST_ASSERT_EQUAL_size_t(queue->capacity, 10);
    TEST_ASSERT_NOT_NULL(queue->items);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(fsi_test_queue_push_pop);
    RUN_TEST(fsi_test_queue_push_pop_multiple);
    RUN_TEST(fsi_test_queue_create);
    return UNITY_END();
}