#include <vector>

#include "SDL.h"

#include "voronoi.hpp"

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


void write_image_data(void* data, int w, int h, int pitch, Sampler::Function sampler) {
	uint8_t* pixels = static_cast<uint8_t*>(data);

	for (int y = 0; y < h; y ++) {
		for (int x = 0; x < w; x ++) {
			uint8_t* pixel = pixels + x * 4;
			const uint8_t normalized = static_cast<uint8_t>(255 * sampler(x, y));

			pixel[0] = 255; // A
			pixel[1] = normalized; // R
			pixel[2] = normalized; // G
			pixel[3] = normalized; // B
		}

		pixels += pitch;
	}
}

int main(int argc, char *argv[]) {
	const int w = 800;
	const int h = 800;

	SDL_Window *window;
	SDL_Renderer *renderer;
	SDL_Event event;

	if (SDL_Init(SDL_INIT_VIDEO) < 0) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't initialize SDL: %s", SDL_GetError());
		return 3;
	}

	if (SDL_CreateWindowAndRenderer(w, h, SDL_WINDOW_RESIZABLE, &window, &renderer)) {
		SDL_LogError(SDL_LOG_CATEGORY_APPLICATION, "Couldn't create window and renderer: %s", SDL_GetError());
		return 3;
	}

	SDL_Texture* buffer = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_BGRA8888, SDL_TEXTUREACCESS_STREAMING, w, h);

	int index = 1;

	std::vector<Sampler> samplers = {
		{voronoi_sampler, "Voronoi"},
		{bitwise_sampler, "Bitwise"},
		{white_sampler, "White"}
	};

	while (true) {

		while (SDL_PollEvent(&event)) {
			if (event.type == SDL_QUIT) {
				return 0;
			}

			if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_TAB && event.key.repeat == 0) {
				index = (index + 1) % samplers.size();

				void* pixels;
				int pitch;
				SDL_LockTexture(buffer, nullptr, &pixels, &pitch);
				write_image_data(pixels, w, h, pitch, samplers[index].function);
				SDL_UnlockTexture(buffer);

				SDL_SetWindowTitle(window, samplers[index].name);
			}
		};

		SDL_SetRenderDrawColor(renderer, 0x00, 0x00, 0x00, 0x00);
		SDL_RenderClear(renderer);
		SDL_RenderCopy(renderer, buffer, nullptr, nullptr);

		SDL_RenderPresent(renderer);
	}

	SDL_DestroyRenderer(renderer);
	SDL_DestroyWindow(window);
	SDL_Quit();

	return 0;
}