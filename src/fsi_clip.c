#include "stb_image.h"
#include "clip.h"

#include "../include/fsi_utils.h"
#include "../include/fsi_clip.h"
#include "../include/thr_thread.h"

#include <inttypes.h>

static thr_t_mutex* g_clip_load_mutex = NULL;

void fsi_clip_global_init(void) {
    g_clip_load_mutex = thr_mtx_new();
    if (g_clip_load_mutex == NULL) {
        fprintf(stderr, "fsi_clip: failed to create load mutex\n");
        return;
    }
}

void fsi_clip_global_shutdown(void) {
    if (g_clip_load_mutex != NULL) {
        thr_mtx_destroy(g_clip_load_mutex);
        g_clip_load_mutex = NULL;
    }
}

void fsi_clip_context_load(const char* embedding, struct clip_ctx** clip_ctx_out) {
    thr_mtx_lock(g_clip_load_mutex);
    *clip_ctx_out = clip_model_load(embedding, 1);
    thr_mtx_unlock(g_clip_load_mutex);
    if (*clip_ctx_out == NULL) {
        fprintf(stderr, "fsi_clip: failed to load model from '%s'\n", embedding);
    }
}

void fsi_clip_context_free(struct clip_ctx* clip_ctx) {
    clip_free(clip_ctx);
}

void fsi_clip_image_get_vector_f32(struct clip_ctx* clip_ctx, const char* image_path, float* vector_out, uint16_t vector_out_dim) {
    if (!clip_ctx || !image_path || !vector_out) {
        fprintf(stderr, "[CLIP ERROR] Null pointer passed to vector extraction.\n");
        return;
    }

    int width = 0, height = 0, channels = 0;
    // Force 3 channels (RGB)
    unsigned char *img_bytes = stbi_load(image_path, &width, &height, &channels, 3);

    // Check if image loaded successfully
    if (!img_bytes) {
        fprintf(stderr, "[CLIP ERROR] Failed to load image at path: '%s' (Reason: %s)\n", 
                image_path, stbi_failure_reason());
        // Zero out destination to prevent reading garbage memory
        memset(vector_out, 0, vector_out_dim * sizeof(float));
        return;
    }

    // Prepare image container
    struct clip_image_u8 img_input;
    img_input.nx = width;
    img_input.ny = height;
    img_input.data = img_bytes;

    // Preprocess image
    struct clip_image_f32 img_res;
    if (!clip_image_preprocess(clip_ctx, &img_input, &img_res)) {
        fprintf(stderr, "[CLIP ERROR] Preprocessing failed for: %s\n", image_path);
        stbi_image_free(img_bytes);
        memset(vector_out, 0, vector_out_dim * sizeof(float));
        return;
    }

    stbi_image_free(img_bytes);

    bool success = clip_image_encode(clip_ctx, 8, &img_res, vector_out, true);
    
    if (!success) {
        fprintf(stderr, "[CLIP ERROR] clip_image_encode failed for: %s\n", image_path);
        memset(vector_out, 0, vector_out_dim * sizeof(float));
        return;
    }

    printf("[CLIP SUCCESS] Vector generated for '%s': [%f, %f, %f...]\n", 
           image_path, vector_out[0], vector_out[1], vector_out[2]);
}

void fsi_clip_text_get_vector_f32(struct clip_ctx* clip_ctx, const char* text, float* vector_out, uint16_t vector_out_dim) {
    // tokenize the prompt
    struct clip_tokens tokens;
    clip_tokenize(clip_ctx, text, &tokens);

    // dimension should match embedding
    uint32_t vec_dim = clip_get_text_hparams(clip_ctx)->projection_dim;
    if(vector_out_dim != vec_dim) {
        printf("Dimension of embedding and provided dimension differ. Cannot get vector for text.");
        vector_out = NULL;
        return;
    }

    if(vector_out) {
        clip_text_encode(clip_ctx, 1, &tokens, vector_out, true);
    }
}

void fsi_clip_image_base64_get_vector_f32(struct clip_ctx* clip_ctx, char* image_base64, size_t image_byte_len, float* vector_out, uint16_t vector_out_dim) {
    if (!clip_ctx || !image_base64 || image_byte_len == 0 || !vector_out) {
        fprintf(stderr, "[CLIP ERROR] Null pointer or empty data passed to vector extraction.\n");
        return;
    }

    size_t decoded_len = 0;
    unsigned char* decoded_image = fsi_utils_base64_decode(image_base64, image_byte_len, &decoded_len);
    if(decoded_image == NULL) {
        printf("Something went wrong during base64 image decoding. Cannot get vector for image.");
        return;
    }

    int width = 0, height = 0, channels = 0;
    // Force 3 channels (RGB)
    unsigned char *img_bytes = stbi_load_from_memory(decoded_image, decoded_len, &width, &height, &channels, 3);

    free(decoded_image);

    // Check if image loaded successfully
    if (!img_bytes) {
        fprintf(stderr, "[CLIP ERROR] Failed to load base64 encoded image (Reason: %s)\n", 
                stbi_failure_reason());
        // Zero out destination to prevent reading garbage memory
        memset(vector_out, 0, vector_out_dim * sizeof(float));
        return;
    }

    // Prepare image container
    struct clip_image_u8 img_input;
    img_input.nx = width;
    img_input.ny = height;
    img_input.data = img_bytes;

    // Preprocess image
    struct clip_image_f32 img_res;
    if (!clip_image_preprocess(clip_ctx, &img_input, &img_res)) {
        fprintf(stderr, "[CLIP ERROR] Preprocessing failed for base64 decoded image\n");
        stbi_image_free(img_bytes);
        memset(vector_out, 0, vector_out_dim * sizeof(float));
        return;
    }

    stbi_image_free(img_bytes);

    bool success = clip_image_encode(clip_ctx, 8, &img_res, vector_out, true);
    
    if (!success) {
        fprintf(stderr, "[CLIP ERROR] clip_image_encode failed for base64 image\n");
        memset(vector_out, 0, vector_out_dim * sizeof(float));
        return;
    }

    printf("[CLIP SUCCESS] Vector generated for base64 image: [%f, %f, %f...]\n", 
           vector_out[0], vector_out[1], vector_out[2]);
}