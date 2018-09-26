
#define STB_IMAGE_IMPLEMENTATION
#include "io/stb/stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "io/stb/stb_image_write.h"

#include "libanalog/ntsc.h"

#include <stdio.h>


void print_usage(const char* self_name)
{
	fprintf(stdout, "Usage: %s <MODE> <INPUT> <OUTPUT>\n", self_name);
	fprintf(stdout, "<MODE>: e - encode, or d - decode\n");
	fprintf(stdout, "<INPUT>: Path to input file (bmp, jpeg, png)\n");
	fprintf(stdout, "<OUTPUT>: Path to output file, file will be saved in bmp format\n\n");
	fprintf(stdout, "Example: %s e ~/lena.jpg ~/lena_coded.bmp\n", self_name);
	fprintf(stdout, "         %s d ~/lena_coded.bmp ~/lena_decoded.bmp\n", self_name);
}

int main(int argc, char** argv)
{
	if (argc != 4 && argc != 5) {
		print_usage(argv[0]);
		return 1;
	}

	const char* input_file = argv[2];
	const char* output_file = argv[3];
	const int encode = *argv[1] == 'e';
	int verbose = 0;

	int width, height, components, out_components;
	int line = 0;
	int i = 0;
	int rv = 1;

	unsigned char* input_image = stbi_load(input_file,
		&width, &height, &components, encode ? STBI_rgb : STBI_grey);

	if (argc == 5)
		if (*argv[4] == 'v')
			verbose = 1;


	if (!input_image) {
		fprintf(stderr, "Unable to load image\n");
		goto exit;
	}

	// stbi returns component = 3 even for STBI_grey
	if ((encode && components != 3) || (!encode && components != 3)) {
		fprintf(stderr, "Unexpected image format, components: %d\n", components);
		goto exit;
	}

	out_components = encode ? 1 : 3;
	components = encode ? 3 : 1;

	ntsc_ctx* ctx = ntsc_create_context(width, encode);

	if (verbose)
		ntsc_enable_verbose(stderr, ctx);

	if (!ctx) {
		fprintf(stderr, "Unable to crete ntsc context\n");
		goto exit_free_input_image;
	}

	unsigned char* output_image = malloc(height * width * out_components);
	if (!output_image) {
		fprintf(stderr, "Unable to allocate memory for output image\n");
		goto exit_free_ctx;
	}

	float* input_line = malloc(sizeof(float) * width * components);
	if (!input_line) {
		fprintf(stderr, "Unable to allocate memory\n");
		goto exit_free_output_image;
	}

	float* output_line = malloc(sizeof(float) * width * out_components);
	if (!output_line) {
		fprintf(stderr, "Unable to allocate memory\n");
		goto exit_free_input_line;
	}


	for (line = 0; line < height; line++) {
		for (i = 0; i < width * components; i++) {
			input_line[i] = (float)input_image[i + line * width * components] / 255.0f - 0.5;
		}

		if (encode) {
			ntsc_process_encode(input_line, output_line, ctx);
		} else {
			ntsc_process_decode(input_line, output_line, ctx);
		}

		for (i = 0; i < width * out_components; i++) {
			int t = round((output_line[i] + 0.5) * 255.0f);
			output_image[i + line * width * out_components] = t < 0 ? 0 : ((t > 255) ? 255 : t);
		}
	}

	rv = !stbi_write_bmp(output_file, width, height, out_components, output_image);

	free(output_line);

exit_free_input_line:
	free(input_line);

exit_free_output_image:
	free(output_image);

exit_free_ctx:
	ntsc_free_context(ctx);

exit_free_input_image:
	stbi_image_free(input_image);

exit:
	if (rv) {
		return 1;
	}

	return 0;
}
