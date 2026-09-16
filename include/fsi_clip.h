#ifndef FSI_CLIP_H
#define FSI_CLIP_H

#include "clip.h"
#include <inttypes.h>

void fsi_clip_global_init(void);
void fsi_clip_global_shutdown(void);
void fsi_clip_context_load(const char* embedding, struct clip_ctx** clip_ctx_out);
void fsi_clip_context_free(struct clip_ctx* clip_ctx);
void fsi_clip_image_get_vector_f32(struct clip_ctx* clip_ctx, const char* image_path, float* vector_out, uint16_t vector_out_dim);
void fsi_clip_image_base64_get_vector_f32(struct clip_ctx* clip_ctx, char* image_base64, size_t image_byte_len, float* vector_out, uint16_t vector_out_dim);
void fsi_clip_text_get_vector_f32(struct clip_ctx* clip_ctx, const char* text, float* vector_out, uint16_t vector_out_dim);

#endif