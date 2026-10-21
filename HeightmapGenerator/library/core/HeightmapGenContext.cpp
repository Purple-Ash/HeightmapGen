#include "HeightmapGenContext.h"
#include "Generators.h"

#include <stdexcept>

ContextImpl::ContextImpl(uint32_t workerCount) {
	workers.reserve(workerCount);
	for (uint32_t i = 0; i < workerCount; i++) {
		workers.emplace_back([this]() {
			while (true) {
				std::function<void()> task;
				{
					std::unique_lock lock(queueMutex);
					queueChanged.wait(lock, [this] { 
						return closing || !tasks.empty(); 
					});

					if (tasks.empty()) {
						break;
					}
					task = std::move(tasks.front());
					tasks.pop_front();
				}
				task();
			}
		});
	}
}

ContextImpl::~ContextImpl() {
	{
		std::lock_guard lock(queueMutex);
		closing = true;
	}
	queueChanged.notify_all();
	for (auto& worker : workers) { 
		worker.join();
	}
	generators.clear();
}

Generator ContextImpl::registerGenerator(std::shared_ptr<GeneratorImpl> generator) {
	std::lock_guard lock(generatorsMutex);
	auto* handle = generator.get();
	generators.emplace(handle, std::move(generator));
	return handle;
}

std::shared_ptr<GeneratorImpl> ContextImpl::getGenerator(Generator generator) {
	std::lock_guard lock(generatorsMutex);
	auto it = generators.find(generator);
	return it == generators.end() ? nullptr : it->second;
}

void ContextImpl::removeGenerator(Generator generator) {
	std::shared_ptr<GeneratorImpl> retained;
	{
		std::lock_guard lock(generatorsMutex);
		auto it = generators.find(generator);
		if (it == generators.end()) {
			return;
		}
		retained = std::move(it->second);
		generators.erase(it);
	}

	retained->fastDrain();
	retained->waitForAll();
}

void ContextImpl::enqueue(std::function<void()> task) {
	{
		std::lock_guard lock(queueMutex);
		if (closing) {
			return;
		}
		tasks.push_back(std::move(task));
	}
	queueChanged.notify_one();
}
