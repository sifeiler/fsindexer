#ifndef FSI_OS_H
#define FSI_OS_H

#ifdef _WIN32
    #include <windows.h>
    #include <io.h>
    #define ftruncate _chsize_s
    #define FS_PATH_MAX 32767   //with long path support enabled

    typedef struct {
        HANDLE dir;
        WIN32_FIND_DATAW data;  //current directory data
        char name[260 * 4];     //file name of WIN32_FIND_DATAW in UTF-8
        unsigned next;
    } directory;

    int fsi_os_directory_open(directory *d, const char *dirpath)
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

    const char* fsi_os_directory_next(directory *d)
    {
        assert(d != NULL);
        //on first iteration
        if(!d->next) {
            if(d->dir == INVALID_HANDLE_VALUE) {
                SetLastError(ERROR_NO_MORE_FILES);
			    return NULL;
            }
            d->next = 1;
        } else {
            if (!FindNextFileW(d->dir, &d->data)) {
                return NULL;
            }
        }
        if (0 == WideCharToMultiByte(CP_UTF8, 0, d->data.cFileName, -1, d->name, sizeof(d->name), NULL, NULL)) {
            return NULL;
        }
        return d->name;
    }

    void fsi_os_directory_close(directory *d)
    {
        assert(d != NULL);
        FindClose(d->dir);
    }

#else
    #include <sys/mman.h>
    #include <unistd.h>
    #include <limits.h>
    #define FS_PATH_MAX PATH_MAX

    typedef struct {
	    DIR *dir;
    } directory;

    int fsi_os_directory_open(directory *d, const char *dirpath)
    {
        assert(d != NULL);
        DIR *dir = opendir(dirpath);
        if (dir == NULL)
            return -1;
        d->dir = dir;
        return 0;
    }

    const char* fsi_os_directory_next(directory *d)
    {
        assert(d != NULL);
        const struct dirent *di;
        if (NULL == (di = readdir(d->dir)))
            return NULL;
        return di->d_name;
    }

    void fsi_os_directory_close(directory *d)
    {
        assert(d != NULL);
        closedir(d->dir);
        d->dir = NULL;
    }

#endif

#endif