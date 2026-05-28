#include "../include/fsi_indexer.h"
#include "../include/fsi_crawler.h"
#include "../include/fsi_worker.h"
#include <stdio.h>

int main (int argc, char** argv) {
    if(argc < 4) {
        printf("Usage: indexer.exe <storage_path> <rootdir_path> <records_count>\n");
        return 1;
    }

    char* mos_path = argv[1];
    const char* rootdir_path = argv[2];
    uint64_t records_count = strtoull(argv[3], NULL, 10);
    printf("Configuring indexer for parameters: \n");
    printf("storage file: %s \n", mos_path);
    printf("root dir: %s \n", rootdir_path);
    printf("records_count: %llu \n", records_count);

    fsi_t_cwl_config cwl_cfg = {
        .parallelismn = 4
    };

    fsi_t_wrk_config wrk_cfg = {
        .parallelismn = 4
    };

    fsi_t_idr_config idr_cfg = {
        .crawler_cfg = &cwl_cfg,
        .worker_cfg = &wrk_cfg,
        .records_count = records_count,
        .storage_path = mos_path
    };

    fsi_t_idr_indexer* indexer = fsi_create_indexer(&idr_cfg);
    if(indexer == NULL) {
        fsi_utils_report_error("Indexer null. Cannot start indexer.");
        return -1;
    }
    fsi_idr_start(indexer, &rootdir_path, 1);
}