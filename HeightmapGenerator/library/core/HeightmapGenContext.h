#pragma once
#include "HeightmapGenAPI.h"
#include "Helpers.h"

#include <condition_variable>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>

struct ContextImpl {
	ContextImpl(uint32_t workerCount = 0);
	~ContextImpl();

	Generator registerGenerator(std::shared_ptr<GeneratorImpl> generator);
	std::shared_ptr<GeneratorImpl> getGenerator(Generator generator);
	void removeGenerator(Generator generator);
	void enqueue(std::function<void()> task);

	uint32_t numThreads() const {
		return workers.size();
	}

private:
	std::mutex generatorsMutex;
	std::unordered_map<Generator, std::shared_ptr<GeneratorImpl>> generators;
	std::mutex queueMutex;
	std::condition_variable queueChanged;
	std::deque<std::function<void()>> tasks;
	bool closing = false;
	std::vector<std::thread> workers;
};
