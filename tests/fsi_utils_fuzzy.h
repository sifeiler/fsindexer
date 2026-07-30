#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "../include/fsi_os.h"

#ifdef _WIN32
    #include <windows.h>
#else
    #include <unistd.h>
    #include <sys/stat.h>
#endif

typedef struct expected_entry {
    char path[FSI_PATH_MAX];
    char name[FSI_NAME_MAX];
    uint64_t path_len;
    uint64_t name_len;
    uint8_t seen;
    FSI_OS_FILE_TYPE type;
} expected_entry;

typedef struct expected_entry_list {
    uint64_t count;
    uint64_t size;
    expected_entry entries[];
} expected_entry_list;

typedef struct available_directory {
    char path[FSI_PATH_MAX];
    uint64_t path_len;
    uint64_t depth;
} available_directory;

void expected_entry_add(expected_entry_list* entries, char* path, size_t path_len, char* name, size_t name_len, FSI_OS_FILE_TYPE type) {
    if (entries->count >= entries->size) return;
    entries->entries[entries->count].seen = 0;
    snprintf(entries->entries[entries->count].path, sizeof(entries->entries[entries->count].path), "%s", path);
    snprintf(entries->entries[entries->count].name, sizeof(entries->entries[entries->count].name), "%s", name);
    entries->entries[entries->count].path_len = path_len;
    entries->entries[entries->count].name_len = name_len;
    entries->entries[entries->count].type = type;
    entries->count++;
}

// generates a name from fuzz data, writing into `out` (max out_len chars incl. null)
// returns number of bytes consumed from data, or -1 if input exhausted
int consume_name(const uint8_t *data, size_t size, size_t *pos, char *out, size_t out_len) {
    //printf("Creating name:\n");
    //printf("Size: %d\n", size);
    //printf("Pos: %d\n", *pos);
    //printf("out_len: %d\n", out_len);

    if (*pos >= size) return -1;
    //printf("raw_len: %d\n", data[*pos]);

    uint8_t raw_len = data[(*pos)++];
    size_t name_len = raw_len % (out_len - 1);   // leave room for null terminator, cap at max

    if (*pos + name_len > size) {
        name_len = size - *pos;   // truncate to what's left, rather than bailing entirely
    }

    for (size_t i = 0; i < name_len; i++) {
        // sanitize into a printable-ish charset so you get valid-ish names most of the time,
        // but still occasionally hit edge cases via the raw byte's low bits
        uint8_t b = data[*pos + i];
        out[i] = (char)('a' + (b % 26));
    }
    out[name_len] = '\0';
    *pos += name_len;

    return (int)name_len;
}

uint64_t create_test_tree_for_fuzzer_data(const uint8_t* data, size_t data_size, size_t* pos, char* root_path, size_t root_path_len, expected_entry_list* expected_entries, uint64_t max_files) {
    //there are at max 256 directories the logic can choose from to add a file or a sub directory
    available_directory directories[256];
    int directory_count = 1;

    snprintf(directories[0].path, sizeof(directories[0].path), "%s", root_path);
    directories[0].path_len = root_path_len;
    directories[0].depth = 0;

    uint64_t file_count = 0;
    uint64_t directories_created = 0;

    while (*pos + 2 <= data_size && directory_count > 0 && file_count < max_files) {
        // pick which open directory to target this round
        int idx = data[(*pos)++] % directory_count;
        available_directory *target = &directories[idx];

        uint8_t choice = data[(*pos)++];
        int make_dir = (choice & 1) && directory_count < 256;

        char name[FSI_NAME_MAX];
        int name_len = consume_name(data, data_size, pos, name, sizeof(name));
        if (name_len < 0) break;   // ran out of fuzz data
        if (name_len == 0) continue;

        char full_path[FSI_PATH_MAX];
        int written = snprintf(full_path, sizeof(full_path), "%s%s%s", target->path, FSI_PATH_DELIMITER_STR, name);

        // enforce the length budget BEFORE creating anything
        if (written < 0 || (size_t)written >= FSI_PATH_MAX) {
            // this branch is exhausted -- remove it from frontier so we never
            // pick it again, and just retry the loop (don't burn the file_count)
            directories[idx] = directories[--directory_count];   // swap-remove
            continue;
        }

        if (make_dir) {
            fsi_os_directory_create(full_path);
            expected_entry_add(expected_entries, full_path, written, name, name_len, FSI_OS_DIR);
            // this new directory becomes a fresh target for FUTURE entries
            directories[directory_count].depth = target->depth + 1;
            directories[directory_count].path_len = written;
            snprintf(directories[directory_count].path, sizeof(directories[directory_count].path), "%s", full_path);
            directory_count++;
            directories_created++;
        } else {
            if (!fsi_os_file_exists(full_path) && (fsi_os_file_create(full_path) == 0)) {
                file_count++;
                expected_entry_add(expected_entries, full_path, written, name, name_len, FSI_OS_FILE);
            }
        }
    }

    //printf("created %d directories\n", directories_created);
    //printf("created %d files\n", file_count);

    return file_count;
}

// Picks the best available test root:
// 1. FSI_TEST_ROOT env var, if set (lets contributors point at a RAM disk manually)
// 2. /dev/shm on Linux, if it exists and is writable (tmpfs, zero SSD wear)
// 3. OS temp dir as fallback (works everywhere, no setup needed)
// Returns the actual string length written to 'out', or -1 on error/truncation
int64_t fsi_get_test_root(char *out, size_t out_size) {
    if(out == NULL || out_size == 0) return -1;

    int written = -1;
    const char *override = getenv("FSI_TEST_ROOT");

    if (override != NULL) {
        written = snprintf(out, out_size, "%s", override);
    }

#ifndef _WIN32
    struct stat st;
    if (stat("/dev/shm", &st) == 0 && S_ISDIR(st.st_mode)) {
        written = snprintf(out, out_size, "/dev/shm/fsi_test_%d", getpid());
    } else {
        const char *tmpdir = getenv("TMPDIR");
        if (tmpdir == NULL) tmpdir = "/tmp";
        written = snprintf(out, out_size, "%s/fsi_test_%d", tmpdir, getpid());
    }
#else
    char temp_path[MAX_PATH];
    GetTempPathA(sizeof(temp_path), temp_path);
    written = snprintf(out, out_size, "%sfsi_test_%lu", temp_path, GetCurrentProcessId());
#endif

    if(written < 0 || (size_t)written >= out_size) return -1;

    return (int64_t)written;
}