#ifndef FSI_OS_H
#define FSI_OS_H

#include <inttypes.h>
#include <assert.h>

#include "fsi_utils.h"

#ifdef _WIN32
    #include <windows.h>

    #define ftruncate _chsize_s
    //#define FS_PATH_MAX 32767   //with long path support enabled
    #define FSI_PATH_MAX 260
    #define FSI_NAME_MAX 255
    #define FSI_PATH_DELIMITER_STR "\\"

    typedef struct {
        HANDLE dir;
        WIN32_FIND_DATAW data;  //current directory data
        char name[260 * 4];     //file name of WIN32_FIND_DATAW in UTF-8
        unsigned next;
    } fsi_os_directory;
#else
    #include <limits.h>
    #include <dirent.h>
    #include <sys/types.h>

    #define FSI_PATH_MAX PATH_MAX
    #define FSI_PATH_DELIMITER_STR "/"
    #define FSI_NAME_MAX PATH_MAX

    typedef struct {
	    DIR *dir;
        char name[FSI_PATH_MAX];
    } fsi_os_directory;
#endif

typedef enum FSI_OS_FILE_TYPE {
        FSI_OS_FILE = 1 << 0,
        FSI_OS_DIR  = 1 << 1,
        FSI_OS_LNK  = 1 << 2,
        FSI_OS_UKN  = 1 << 3
} FSI_OS_FILE_TYPE;

typedef struct fsi_file_info {
    char file_name[FSI_NAME_MAX];
    char path[FSI_PATH_MAX];
    FSI_OS_FILE_TYPE type;
    uint64_t size;
} fsi_file_info;

int fsi_os_directory_open(fsi_os_directory *d, const char *dirpath);
const int fsi_os_directory_next(fsi_os_directory *d, fsi_file_info* file_info);
void fsi_os_directory_close(fsi_os_directory *d);

#endif