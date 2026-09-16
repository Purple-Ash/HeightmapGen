#include <vector>

#include <cstdint>
#include "voronoi.hpp"

#include <winx.h>
#include <glad/glad.h>

static float voronoi_sampler(int x, int y) {
	VoronoiNoise noise;
	float s = 0.03;

	return noise.get(s * x, s * y, 0);
}

static float bitwise_sampler(int x, int y) {
	return (x & y) * 0.01;
}

static float white_sampler(int x, int y) {
	float s = 0.3;
	return white_noise(x * s, y * s, 0);
}

struct Sampler {
	using Function = float (*) (int, int);

	Function function;
	const char* name;
};


void write_image_data(void* data, int w, int h, Sampler::Function sampler) {
	uint8_t* pixels = static_cast<uint8_t*>(data);

	for (int y = 0; y < h; y ++) {
		for (int x = 0; x < w; x ++) {
			const uint8_t normalized = static_cast<uint8_t>(255 * sampler(x, y));

			pixels[0] = normalized;
			pixels[1] = normalized;
			pixels[2] = normalized;

			pixels += 3;
		}
	}
}

static bool should_run = true;
static int index = 0;

static std::vector<Sampler> samplers = {
	{voronoi_sampler, "Voronoi"},
	{bitwise_sampler, "Bitwise"},
	{white_sampler, "White"}
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
	winxSetCloseEventHandle(window_close_handler);
	winxSetKeyboardEventHandle(window_keyboard_handler);

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

			write_image_data(pixels, w, h, samplers[index].function);
			glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, w, h, 0, GL_RGB,  GL_UNSIGNED_BYTE, pixels);

			winxSetTitle(samplers[index].name);
		}

		glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
		winxSwapBuffers();
	}

	winxClose();
	return 0;
}