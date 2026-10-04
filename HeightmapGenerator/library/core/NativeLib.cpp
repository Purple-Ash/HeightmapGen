#include "NativeLib.hpp"

#include <filesystem>
#include <fstream>

static std::string gen_random(const int len) {
	static const char alphanum[] =
		"0123456789"
		"ABCDEFGHIJKLMNOPQRSTUVWXYZ"
		"abcdefghijklmnopqrstuvwxyz";
	std::string tmp_s;
	tmp_s.reserve(len);

	for (int i = 0; i < len; ++i) {
		tmp_s += alphanum[rand() % (sizeof(alphanum) - 1)];
	}

	return tmp_s;
}

#if defined(WIN32) || defined(_WIN32) || defined(__WIN32__) || defined(__NT__)

#include <windows.h>

NativeLib::NativeLib(std::string name) {
	name = ".\\" + name;
	name += ".dll";
	handle = (void*) LoadLibrary(name);
}

NativeLib::~NativeLib() {
	FreeLibrary((HMODULE) handle);
}

void* NativeLib::lookup(const char* name) {
	return (void*) GetProcAddress((HMODULE) handle, name);
}

#elif defined(__linux__)

#include <dlfcn.h>

NativeLib::NativeLib(std::string name) {
	name = "./lib" + name;
	name += ".so";

	auto tmp = std::filesystem::temp_directory_path().append(name + "-" + gen_random(8));

	std::ifstream source(name, std::ios::binary);
	std::ofstream dest(tmp, std::ios::binary | std::ios::trunc);

	dest << source.rdbuf();

	source.close();
	dest.close();

	printf("Path: %s\n", tmp.c_str());

	handle = dlopen(tmp.c_str(), RTLD_LAZY | RTLD_LOCAL);
}

NativeLib::~NativeLib() {
	dlclose(handle);
}

void* NativeLib::lookup(const char* name) {
	return dlsym(handle, name);
}

#else
#error "Unsupported platform!"
#endif

