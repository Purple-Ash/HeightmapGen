#pragma once
#include <string>

class NativeLib {
private:

	void* handle;

public:

	NativeLib(std::string path);
	~NativeLib();

	bool isOpen() const {
		return handle != nullptr;
	}

	void* lookup(const char* name);

};