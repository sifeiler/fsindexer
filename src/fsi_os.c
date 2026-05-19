#include "../include/fsi_os.h"

#ifdef _WIN32
    #include <windows.h>

    #define ftruncate _chsize_s

    int fsi_os_directory_open(fsi_os_directory *d, const char *dirpath)
    {
        assert(d != NULL);
        wchar_t path_buffer[1000];
        int r = MultiByteToWideChar(CP_UTF8, 0, dirpath, -1, path_buffer, 1000 - 2);
        if (r == 0)
            return -1;
        r--;    //because of last NULL counted
        path_buffer[r++] = '\\';
        path_buffer[r++] = '*';
        path_buffer[r] = '\0';

        HANDLE dir = FindFirstFileW(path_buffer, &d->data);
        if (dir == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND)
            return -1;

        d->dir = dir;
        return 0;
    }

    const int fsi_os_directory_next(fsi_os_directory *d, fsi_file_info* file_info)
    {
        assert(d != NULL);
        //on first iteration
        if(!d->next) {
            if(d->dir == INVALID_HANDLE_VALUE) {
                SetLastError(ERROR_NO_MORE_FILES);
			    return -1;
            }
            d->next = 1;
        } else {
            if (!FindNextFileW(d->dir, &d->data)) {
                return -1;
            }
        }
        if (0 == WideCharToMultiByte(CP_UTF8, 0, d->data.cFileName, -1, d->name, sizeof(d->name), NULL, NULL)) {
            return -1;
        }
        
        char full_path[FSI_PATH_MAX];
        int len = snprintf(file_info->path, sizeof(full_path), "%s%s%s", d->name, FSI_PATH_DELIMITER_STR, d->data.cFileName);

        // Check if the path was truncated because it was too long
        if (len >= sizeof(full_path)) {
            fsi_utils_report_error("Warning: Path truncated for %s %s\n", d->name, d->data.cFileName);
            snprintf(file_info->path, sizeof(file_info->path), "TRUNCATE");
            return -1;
        }

        strcpy(file_info->file_name, d->data.cFileName);

        if(d->data.dwFileAttributes & FILE_ATTRIBUTE_REPARSE_POINT) {
            file_info->type = FSI_OS_LNK;
        } else if(d->data.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) {
            file_info->type = FSI_OS_DIR;
        } else {
            file_info->type = FSI_OS_FILE;
        }
        return 0;
    }

    void fsi_os_directory_close(fsi_os_directory *d)
    {
        assert(d != NULL);
        FindClose(d->dir);
    }

#else
    #include <sys/mman.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/types.h>

    int fsi_os_directory_open(fsi_os_directory *d, const char *dirpath)
    {
        assert(d != NULL);
        DIR *dir = opendir(dirpath);
        if (dir == NULL)
            return -1;
        d->dir = dir;
        return 0;
    }

    const int fsi_os_directory_next(fsi_os_directory *d, fsi_file_info* file_info)
    {
        assert(d != NULL);
        const struct dirent *di;
        if (NULL == (di = readdir(d->dir)))
            return -1;
        
        char full_path[FSI_PATH_MAX];
        int len = snprintf(file_info->path, sizeof(full_path), "%s%s%s", d->name, FSI_PATH_DELIMITER_STR, di->d_name);

        // Check if the path was truncated because it was too long
        if (len >= sizeof(full_path)) {
            fsi_utils_report_error("Warning: Path truncated for %s %s\n", d->name, di->d_name);
            snprintf(file_info->path, sizeof(file_info->path), "TRUNCATE");
            return -1;
        }
        strcpy(file_info->file_name, di->d_name);

        if(di->d_type == DT_LNK) {
            file_info->type = FSI_OS_LNK;
        } else if(di->d_type == DT_DIR) {
            file_info->type = FSI_OS_DIR;
        } else if(di->d_type == DT_REG) {
            file_info->type = FSI_OS_FILE;
        } else {
            file_info->type = FSI_OS_UKN;
        }
        
        return 0;
    }

    void fsi_os_directory_close(fsi_os_directory *d)
    {
        assert(d != NULL);
        closedir(d->dir);
        d->dir = NULL;
    }

#endif