#include "unity.h"

#include "../include/fsi_queue.h"
#include "../include/fsi_indexer.h"

void setUp(void) {}

void tearDown(void) {}

void fsi_test_queue_put(void) {
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
    fsi_queue_pop(q, &file1_pop);
    TEST_ASSERT_EQUAL_MEMORY(&file1, &file1_pop, sizeof(fsi_t_mos_file));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(fsi_test_queue_put);
    return UNITY_END();
}