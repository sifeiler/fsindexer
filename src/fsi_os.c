#include "../include/fsi_os.h"

#ifdef _WIN32
    #include <windows.h>
    #include <string.h>

    #define ftruncate _chsize_s

    int fsi_os_directory_open(fsi_os_directory *d, const char *dirpath)
    {
        assert(d != NULL);
        wchar_t path_buffer[1000];
        int r = MultiByteToWideChar(CP_UTF8, 0, dirpath, -1, path_buffer, FSI_PATH_MAX - 2);
        if (r == 0)
            return -1;
        r--;    //because of last NULL counted
        path_buffer[r++] = '\\';
        path_buffer[r++] = '*';
        path_buffer[r] = '\0';

        HANDLE dir = FindFirstFileW(path_buffer, &d->data);
        if (dir == INVALID_HANDLE_VALUE && GetLastError() != ERROR_FILE_NOT_FOUND) {
            fsi_utils_report_error("Cannot open directory %s", dirpath);
            return -1;
        }

        d->dir = dir;
        d->next = 0;
        strncpy(d->name, dirpath, sizeof(d->name) - 1);
        return 0;
    }

    const int fsi_os_directory_next(fsi_os_directory *d, fsi_file_info* file_info)
    {
        assert(d != NULL);
        //on first iteration
        if(!d->next) {
            if(d->dir == INVALID_HANDLE_VALUE) {
                SetLastError(ERROR_NO_MORE_FILES);
                fsi_utils_report_error("ERROR_NO_MORE_FILES: no more files in directory %s", d->name);
			    return -1;
            }
            d->next = 1;
        } else {
            if (!FindNextFileW(d->dir, &d->data)) {
                return -1;
            }
        }

        //load utf8 filename to filename buffer
        char filename[FSI_PATH_MAX];
        if (0 == WideCharToMultiByte(CP_UTF8, 0, d->data.cFileName, -1, filename, sizeof(filename), NULL, NULL)) {
            fsi_utils_report_error("Err during filename UTF8 conversion.");
            return -1;
        }

        //load utf8 filepath to full_path buffer
        char full_path[FSI_PATH_MAX];
        int len = snprintf(full_path, sizeof(full_path), "%s%s%s", d->name, FSI_PATH_DELIMITER_STR, filename);
        if (len >= sizeof(full_path)) {
            fsi_utils_report_error("Warning: Path truncated for %s %s\n", d->name, filename);
            return -1;
        }

        strncpy(file_info->path, full_path, sizeof(file_info->path) - 1);
        strncpy(file_info->file_name, filename, sizeof(file_info->file_name) - 1);

        // Check if the path was truncated because it was too long
        if (len >= sizeof(full_path)) {
            fsi_utils_report_error("Warning: Path truncated for %s %s\n", d->name, d->data.cFileName);
            snprintf(file_info->path, sizeof(file_info->path), "TRUNCATE");
            return -1;
        }

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

    int fsi_os_get_file_id(fsi_file_info* file_info) {
        HANDLE h = CreateFile(file_info->path, 0, FILE_SHARE_READ, NULL,
                          OPEN_EXISTING, FILE_FLAG_BACKUP_SEMANTICS, NULL);
        if(h == INVALID_HANDLE_VALUE) return -1;
        
        BY_HANDLE_FILE_INFORMATION info;
        GetFileInformationByHandle(h, &info);
        CloseHandle(h);
        
        file_info->file_id = ((uint64_t)info.nFileIndexHigh << 32) | info.nFileIndexLow;
        return 0;
    }

#else
    #include <sys/mman.h>
    #include <unistd.h>
    #include <dirent.h>
    #include <sys/types.h>
    #include <string.h>

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
        
        //TODO: fix this like for windows!!!
        char full_path[FSI_PATH_MAX];
        int len = snprintf(file_info->path, sizeof(full_path), "%s%s%s", d->name, FSI_PATH_DELIMITER_STR, di->d_name);

        // Check if the path was truncated because it was too long
        if (len >= sizeof(full_path)) {
            fsi_utils_report_error("Warning: Path truncated for %s %s\n", d->name, di->d_name);
            snprintf(file_info->path, sizeof(file_info->path), "TRUNCATE");
            return -1;
        }
        strcpy(file_info->file_name, di->d_name);
        file_info->file_id = di->d_ino;
        file_info->file_id_set = 1;

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

    int fsi_os_get_file_id(fsi_file_info* file_info) {
        struct stat st;
        if(stat(path, &st) != 0) return -1;
        file_info->file_id = (uint64_t)st.st_ino;
        return 0;
    }

#endif