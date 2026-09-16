#pragma once

#include "HeightmapGenAPI.h"

#include <filesystem>

#ifdef _WIN32
	#define WIN32_LEAN_AND_MEAN
	#define NOMINMAX
	#include <Windows.h>
#else
	#include <dlfcn.h>
#endif

#define HG_API_FUNCTIONS(X) \
	X(smokeTest) \
	X(createContext) \
	X(destroyContext) \
	X(createHydraulicErosionGenerator) \
	X(createPerlinGenerator) \
	X(createBrownianPerlinGenerator) \
	X(destroyGenerator) \
	X(getChunk) \
	X(getPoint) \
	X(requestChunk) \
	X(requestPoint) \
	X(probeChunk) \
	X(probePoint) \
	X(cleanChunkFromCache) \
	X(clearAllCache)

class HeightmapGenerator {
public:

	struct API {
		#define DECLARE_FUNCTION(name) decltype(&::name) name = nullptr;
		HG_API_FUNCTIONS(DECLARE_FUNCTION)
		#undef DECLARE_FUNCTION
	};

	bool load(const std::filesystem::path& path) {
		module = loadModule(path);
		if (module == nullptr) {
			return false;
		}

		bool allLoaded = true;
		#define LOAD_FUNCTION(name) allLoaded &= loadFunction(api_.name, #name);
		HG_API_FUNCTIONS(LOAD_FUNCTION)
		#undef LOAD_FUNCTION
		return allLoaded;
	}

	const API& api() const {
		return api_;
	}

	~HeightmapGenerator() {
		if (module) {
			freeModule(module);
		}
	}

private:

	API api_;

	template <typename Function>
	bool loadFunction(Function& function, const char* name) {
		function = resolve<Function>(name);
		return function != nullptr;
	}

#ifdef _WIN32
	HMODULE module = nullptr;

	HMODULE loadModule(const std::filesystem::path& path) const {
		return LoadLibraryExW(path.c_str(), nullptr, LOAD_LIBRARY_SEARCH_DLL_LOAD_DIR | LOAD_LIBRARY_SEARCH_DEFAULT_DIRS);
	}

	template <typename Function>
	Function resolve(const char* name) const {
		return reinterpret_cast<Function>(GetProcAddress(module, name));
	}

	void freeModule(HMODULE module) const {
		FreeLibrary(module);
	}
#else
	void* module = nullptr;

	void* loadModule(const std::filesystem::path& path) const {
		return dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
	}

	template <typename Function>
	Function resolve(const char* name) const {
		return reinterpret_cast<Function>(dlsym(module, name));
	}

	void freeModule(void* module) const {
		dlclose(module);
	}
#endif
};

