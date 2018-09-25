#include "ntsc.h"

#include <stdlib.h>
#include <stdint.h>

#include <stdio.h>

#include <math.h>

struct ntsc_iq {
	float ei;
	float eq;
};

struct biquad_filter_ctx {
	float a0, a1, a2, b1, b2;
	float z1, z2;
};

struct ntsc_ctx {

	struct generator_ctx {
		float* sin_table;
		float* cos_table;
		int shift;
	} generator;
	uint16_t width;
	struct ntsc_iq* iq;

	struct biquad_filter_ctx i_filter;
	struct biquad_filter_ctx q_filter;
};

static void ntsc_filter_init(struct biquad_filter_ctx* filter, float fc)
{
	float k = tan(M_PI * fc);
	float q = 0.7;
	float norm = 1 / (1 + k / q + k * k);
	filter->a0 = k * k * norm;
	filter->a1 = 2 * filter->a0;
	filter->a2 = filter->a0;
	filter->b1 = 2 * (k * k - 1) * norm;
	filter->b2 = (1 - k / q + k * k) * norm;
	filter->z1 = 0;
	filter->z2 = 0;
}

static float ntsc_process_filter(struct biquad_filter_ctx* filter, float in)
{
	float out = in * filter->a0 + filter->z1;
	filter->z1 = in * filter->a1 + filter->z2 - filter->b1 * out;
	filter->z2 = in * filter->a2 - filter->b2 * out;
	//fprintf(stderr, "filter in: %f, out: %f\n", in, out);
	return out;
}

static inline float ntsc_rgb_to_y(const float* rgb) {
	return rgb[0] * 0.30 + rgb[1] * 0.59 + rgb[2] * 0.11;
}

static inline void ntsc_rgb_to_iq(const float* rgb, struct ntsc_iq* iq)
{
	iq->ei = rgb[0] * 0.6 - rgb[1] * 0.28 - rgb[2] * 0.32;
	iq->eq = rgb[0] * 0.21 - rgb[1] * 0.52 + rgb[2] * 0.31;
}

static inline void ntsc_iqy_to_rgb(const struct ntsc_iq* iq, float y, float* rgb)
{
	rgb[0] = iq->ei * 0.96 + iq->eq * 0.62 + y;
	rgb[1] = iq->ei * (-0.27) - iq->eq * 0.65 + y;
	rgb[2] = iq->ei * (-1.11) + iq->eq * 1.7 + y;
}

static void ntsc_create_generator(struct ntsc_ctx* ctx)
{
	int i = 0;
	int n = 2 * ctx->width;
	float width = ctx->width;
	//TODO: tune the freq
	int f = (int)(width / 2.5) | 0x01;
	ctx->generator.sin_table = malloc(sizeof(float) * n);
	ctx->generator.cos_table = malloc(sizeof(float) * n);
	for (i = 0; i < n; i++) {
		ctx->generator.sin_table[i] = sin(f * i * M_PI/width);
		ctx->generator.cos_table[i] = cos(f * i * M_PI/width);
	}
	ctx->generator.shift = 0;
}

static void ntsc_free_generator(struct ntsc_ctx* ctx)
{
	free(ctx->generator.sin_table);
	free(ctx->generator.cos_table);
}

ntsc_ctx* ntsc_create_context(int width, int encode)
{
	if (width >= 65535) {
		return NULL;
	}
	ntsc_ctx* p = malloc(sizeof(ntsc_ctx));
	if (!p)
		return NULL;

	p->width = width;
	ntsc_create_generator(p);
	p->iq = malloc(sizeof(struct ntsc_iq) * width);

	ntsc_filter_init(&p->i_filter, 0.08);
	ntsc_filter_init(&p->q_filter, 0.05);
	return p;
}

void ntsc_free_context(ntsc_ctx* ctx)
{
	ntsc_free_generator(ctx);
	free(ctx->iq);
	free(ctx);
}

static void ntsc_next_line(ntsc_ctx* ctx)
{
	if (ctx->generator.shift) {
		ctx->generator.shift = 0;
	} else {
		ctx->generator.shift = 1;
	}
}

static void ntsc_modulate_line(ntsc_ctx* ctx, const struct ntsc_iq* in, float* out)
{
	int i;
	float* sin = ctx->generator.sin_table;
	float* cos = ctx->generator.cos_table;

	const int chroma_shift = 3;
	if (ctx->generator.shift) {
		sin += ctx->width;
		cos += ctx->width;
	}

	for (i = 0; i < chroma_shift; i++) {
		const struct ntsc_iq* iq = in + i;
		(void)(ntsc_process_filter(&ctx->i_filter, iq->ei * sin[i]));
		(void)(ntsc_process_filter(&ctx->q_filter, iq->eq * cos[i]));
	}

	for (i = chroma_shift; i < ctx->width; i++) {
		const struct ntsc_iq* iq = in + i;
		int j = i - chroma_shift;
		out[j] = ntsc_process_filter(&ctx->i_filter, iq->ei) * sin[j] + ntsc_process_filter(&ctx->q_filter, iq->eq) * cos[j];
	}

	for (i = 0; i < chroma_shift; i++) {
		int j = i - chroma_shift + ctx->width;
		out[j] = ntsc_process_filter(&ctx->i_filter, 0.0) * sin[j] + ntsc_process_filter(&ctx->q_filter, 0.0) * cos[j];
	}
}

static void ntsc_demodulate_line(const float* input, ntsc_ctx* ctx)
{
	int i;
	float* sin = ctx->generator.sin_table;
	float* cos = ctx->generator.cos_table;

	// In theory we should have different delay for I and Q channel
	const int chroma_shift = 3;
	if (ctx->generator.shift) {
		sin += ctx->width;
		cos += ctx->width;
	}


	// Feed filter
	for (i = 0; i < chroma_shift; i++) {
		(void)(ntsc_process_filter(&ctx->i_filter, input[i] * sin[i]) * 2.0);
		(void)(ntsc_process_filter(&ctx->q_filter, input[i] * cos[i]) * 2.0);
	}

	// Shift color components for chroma_shift pixels left to compensate low pass filter delay
	for (i = chroma_shift; i < ctx->width; i++) {
		struct ntsc_iq* iq = ctx->iq + i - chroma_shift;
		iq->ei = ntsc_process_filter(&ctx->i_filter, input[i] * sin[i]) * 2.0;
		iq->eq = ntsc_process_filter(&ctx->q_filter, input[i] * cos[i]) * 2.0;
	}

	// Drain last delayed values
	for (i = 0; i < chroma_shift; i++) {
		struct ntsc_iq* iq = ctx->iq + i - chroma_shift + ctx->width;
		iq->ei = ntsc_process_filter(&ctx->i_filter, 0.0) * 2.0;
		iq->eq = ntsc_process_filter(&ctx->q_filter, 0.0) * 2.0;
	}
}

void ntsc_process_encode(const float* input, float* output, ntsc_ctx* ctx)
{
	int i;
	for (i = 0; i < ctx->width; i++) {
		ntsc_rgb_to_iq(input + i * 3, (ctx->iq + i));
	}

	ntsc_modulate_line(ctx, ctx->iq, output);

	for (i = 0; i < ctx->width; i++) {
		output[i] += ntsc_rgb_to_y(input + i * 3);
	}

	ntsc_next_line(ctx);
}

void ntsc_process_decode(const float* input, float* output, ntsc_ctx* ctx)
{
	int i;

	ntsc_demodulate_line(input, ctx);

	for (i = 0; i < ctx->width; i++) {
		struct ntsc_iq* iq = ctx->iq + i;
		ntsc_iqy_to_rgb(iq, input[i], output + i * 3);
	}

	ntsc_next_line(ctx);
}
