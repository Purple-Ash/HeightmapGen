#include <algorithm>
#include <vector>
#include <cstdint>
#include <chrono>

#include <winx.h>
#include <glad/glad.h>

#include "../library/core/Generators.h"

struct Sampler {
	Generator generator;
	const char* name;
};

void write_image_data(void* data, int w, int h, Sampler& sampler) {
	uint8_t* pixels = static_cast<uint8_t*>(data);

	printf("Generating image for sampler: '%s'\n", sampler.name);
	std::chrono::steady_clock::time_point begin = std::chrono::steady_clock::now();

	for (int y = 0; y < h; y ++) {
		for (int x = 0; x < w; x ++) {
			float sample = std::clamp(0.0f, 1.0f, getPoint(sampler.generator, x, y));

			const uint8_t normalized = static_cast<uint8_t>(255 * sample);

			pixels[0] = normalized;
			pixels[1] = normalized;
			pixels[2] = normalized;

			pixels += 3;
		}
	}

	std::chrono::steady_clock::time_point end = std::chrono::steady_clock::now();
	printf("Done generating, took %dms\n", (int) std::chrono::duration_cast<std::chrono::milliseconds>(end - begin).count());

}

static bool should_run = true;
static int index = 0;
static Context ctx = createContext();

template <typename G>
Generator createSimple(float scale, float amplitude) {
	CommonSettings settings {};
	settings.seed = 42;
	settings.scale = scale;
	settings.amplitude = amplitude;
	settings.resolution = 1;
	settings.cacheable = false;

	return new G(ctx, settings);
}

Generator createHydraulicErosion(float scale, float amplitude, Generator generator) {
	CommonSettings settings {};
	settings.seed = 42;
	settings.scale = scale;
	settings.amplitude = amplitude;
	settings.resolution = 1;
	settings.cacheable = true;

	HydraulicErosionSettings hes {};
	hes.seed = settings.seed; // why does HEG has two seeds?
	hes.baseGeneratorImpl = generator;

	return new HydraulicErosionGeneratorImpl(ctx, settings, hes);
}

static std::vector<Sampler> samplers = {
	{createSimple<RandomGeneratorImpl>(1, 1), "Random Generator"},
	{createSimple<CoordinateGeneratorImpl>(0.5, 0.001), "Coordinate Generator"},
	{createSimple<VoronoiGeneratorImpl>(0.03, 1), "Voronoi Generator"},
	{createSimple<PerlinGeneratorImpl>(0.05, 1), "Perlin Generator"},
	{createSimple<BrownianPerlinGeneratorImpl>(0.01, 1), "Brownian Perlin Generator"},
	{createHydraulicErosion(0.01, 1, createSimple<BrownianPerlinGeneratorImpl>(0.01, 1)), "Hydraulic Erosion Generator"}
};

void window_close_handler() {
	should_run = false;
}

void window_keyboard_handler(int state, int keycode) {
	if (keycode == WXK_ENTER && state == WINX_RELEASED) {
		index = (index + 1) % samplers.size();
	}
}

int main(int argc, char *argv[]) {
	const int w = 800;
	const int h = 800;

	void* pixels = malloc(3 * w * h);

	winxOpen(w, h, "Noise Viewer");
	winxSetCloseEventHandler(window_close_handler);
	winxSetKeyboardEventHandler(window_keyboard_handler);

	gladLoadGL();

	int current = -1;

	GLuint texture = 0;
	glCreateTextures(GL_TEXTURE_2D, 1, &texture);
	glBindTexture(GL_TEXTURE_2D, texture);

	glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTextureParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

	GLuint read_fb;
	glGenFramebuffers(1, &read_fb);
	glBindFramebuffer(GL_READ_FRAMEBUFFER, read_fb);
	glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, texture, 0);

	while (should_run) {
		winxPollEvents();

		if (current != index) {
			current = index;

			write_image_data(pixels, w, h, samplers[index]);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,  GL_UNSIGNED_BYTE, pixels);

			winxSetTitle(samplers[index].name);
		}

		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		winxSwapBuffers();
	}

	winxClose();
	return 0;
}