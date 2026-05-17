#ifndef FSI_UTILS_H
#define FSI_UTILS_H

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

static inline void fsi_utils_report_error(const char* input, ...) {
    va_list args;
    va_start(args, input);

    printf("Error: %s, Message: ", strerror(errno));

    vprintf(input, args);
    printf("\n");

    va_end(args);
}

static inline void fsi_utils_exit_program(char* exit_reason, uint64_t exit_code) {
    printf("Program exit: %s\n", exit_reason);
    
    //cleanup, free memory, etc...

    exit(exit_code);
}

#endif