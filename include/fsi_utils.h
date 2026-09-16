#ifndef FSI_UTILS_H
#define FSI_UTILS_H

#include <stdint.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include "cdecode.h"

#define FSI_VECTOR_MAX_DIM 512

#define FSI_RETURN_VALID 0
#define FSI_RETURN_INVALID 1

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

static inline unsigned char* fsi_utils_base64_decode(const char *input, size_t input_len, size_t *out_len) {
    size_t max_len = base64_decode_maxlength(input_len);
    unsigned char *out = malloc(max_len);
    if (!out) return NULL;

    base64_decodestate state;
    base64_init_decodestate(&state);
    size_t decoded_len = base64_decode_block(input, input_len, out, &state);

    *out_len = decoded_len;
    return out;
}

#endif