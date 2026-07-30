#include "../include/fsi_os.h"

static void remove_test_dir_recursive(const char* path) {
    fsi_os_directory dir;
    if (fsi_os_directory_open(&dir, path) != 0) return;

    fsi_file_info info;
    while (fsi_os_directory_next(&dir, &info) == 0) {
        if (strcmp(info.file_name, ".") == 0 || strcmp(info.file_name, "..") == 0) continue;

        if (info.type == FSI_OS_DIR) {
            remove_test_dir_recursive(info.file_path);   // delete subdirectory contents first
        } else {
            fsi_os_file_delete(info.file_path);
        }
    }
    fsi_os_directory_close(&dir);
    fsi_os_directory_delete(path);   // now empty, safe to remove
}