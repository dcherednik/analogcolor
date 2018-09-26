#ifndef NTSC_H
#define NTSC_H

#include <stdio.h>

typedef struct ntsc_ctx ntsc_ctx;

ntsc_ctx* ntsc_create_context(int width, int encode);
void ntsc_free_context(ntsc_ctx* ctx);
void ntsc_process_encode(const float* input, float* output, ntsc_ctx* ctx);
void ntsc_process_decode(const float* input, float* output, ntsc_ctx* ctx);

void ntsc_enable_verbose(FILE* fout, ntsc_ctx* ctx);

#endif
