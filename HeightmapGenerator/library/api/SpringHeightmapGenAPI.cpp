#include "SpringHeightmapGenAPI.h"
#include "HeightmapGenPfn.h"
#include "NativeLib.hpp"

#include <memory>
#include <mutex>
#include <shared_mutex>
#include <vector>

struct ContextImpl {};
struct GeneratorImpl {};

std::shared_mutex mutex;

enum SpringGeneratorType {
	COORDINATE,
	RANDOM,
	VORONOI,
	PERLIN,
	BROWNIAN,
	HYDRAULIC
};

struct SpringContext;

struct SprintGenerator {
	SpringContext* context;
	SpringGeneratorType type;
	CommonSettings common_settings;
	HydraulicErosionSettings hydraulic_erosion_settings;

	SprintGenerator(SpringContext* context, SpringGeneratorType type, CommonSettings common, HydraulicErosionSettings hydraulic) {
		this->context = context;
		this->type = type;
		this->common_settings = common;
		this->hydraulic_erosion_settings = hydraulic;

		load();
	}

	void load();
	void unload();

	// link to the current managed implementation object
	Generator impl = nullptr;
};

struct SpringContext {
	std::vector<std::unique_ptr<SprintGenerator>> generators;

	void load();
	void unload();

	// link to the current managed implementation object
	Context impl = nullptr;
};

struct SpringNative {
	std::vector<std::unique_ptr<SpringContext>> contexts;
	std::unique_ptr<NativeLib> lib;

	PFN_smokeTest* hg_smokeTest = nullptr;

	PFN_createContext* hg_createContext = nullptr;
	PFN_destroyContext* hg_destroyContext = nullptr;

	PFN_createCoordinateGenerator* hg_createCoordinateGenerator = nullptr;
	PFN_createRandomGenerator* hg_createRandomGenerator = nullptr;
	PFN_createVoronoiGenerator* hg_createVoronoiGenerator = nullptr;
	PFN_createHydraulicErosionGenerator* hg_createHydraulicErosionGenerator = nullptr;
	PFN_createPerlinGenerator* hg_createPerlinGenerator = nullptr;
	PFN_createBrownianPerlinGenerator* hg_createBrownianPerlinGenerator = nullptr;
	PFN_destroyGenerator* hg_destroyGenerator = nullptr;

	PFN_getChunk* hg_getChunk = nullptr;
	PFN_getPoint* hg_getPoint = nullptr;
	PFN_requestChunk* hg_requestChunk = nullptr;
	PFN_requestPoint* hg_requestPoint = nullptr;
	PFN_probeChunk* hg_probeChunk = nullptr;
	PFN_probePoint* hg_probePoint = nullptr;
	PFN_cleanChunkFromCache* hg_cleanChunkFromCache = nullptr;
	PFN_clearAllCache* hg_clearAllCache = nullptr;

	SpringNative() {
		load();
	}

	void load();
	void unload();

	void* loadOrExplode(const char* name) const;

};

/*
 * State management
 */

SpringNative& getNative() {
	static SpringNative spring;
	return spring;
}

void SprintGenerator::load() {
	SpringNative& native = getNative();

	if (type == COORDINATE) this->impl = native.hg_createCoordinateGenerator(context->impl, common_settings);
	if (type == RANDOM) this->impl = native.hg_createRandomGenerator(context->impl, common_settings);
	if (type == VORONOI) this->impl = native.hg_createVoronoiGenerator(context->impl, common_settings);
	if (type == PERLIN) this->impl = native.hg_createPerlinGenerator(context->impl, common_settings);
	if (type == BROWNIAN) this->impl = native.hg_createBrownianPerlinGenerator(context->impl, common_settings);
	if (type == HYDRAULIC) this->impl = native.hg_createHydraulicErosionGenerator(context->impl, common_settings, hydraulic_erosion_settings);
}

void SprintGenerator::unload() {
	getNative().hg_destroyGenerator(impl);
}

void SpringContext::load() {
	this->impl = getNative().hg_createContext();

	for (auto& generator : generators) {
		generator->load();
	}
}

void SpringContext::unload() {
	for (auto& generator : generators) {
		generator->unload();
	}

	getNative().hg_destroyContext(impl);
}

void* SpringNative::loadOrExplode(const char* name) const {
	void* func = lib->lookup(name);

	if (func == nullptr) {
		printf("Unable to load function '%s' from the managed implementation!\n", name);
		exit(2);
	}

	return func;
}

void SpringNative::load() {
	lib = std::make_unique<NativeLib>("HeightmapGen");

	if (!lib->isOpen()) {
		printf("Unable to load the managed implementation of HeightmapGen!\n");
		exit(1);
	}

	this->hg_smokeTest = (PFN_smokeTest*) loadOrExplode("smokeTest");

	this->hg_createContext = (PFN_createContext*) loadOrExplode("createContext");
	this->hg_destroyContext = (PFN_destroyContext*) loadOrExplode("destroyContext");

	this->hg_createCoordinateGenerator = (PFN_createCoordinateGenerator*) loadOrExplode("createCoordinateGenerator");
	this->hg_createRandomGenerator = (PFN_createRandomGenerator*) loadOrExplode("createRandomGenerator");
	this->hg_createVoronoiGenerator = (PFN_createVoronoiGenerator*) loadOrExplode("createVoronoiGenerator");
	this->hg_createHydraulicErosionGenerator = (PFN_createHydraulicErosionGenerator*) loadOrExplode("createHydraulicErosionGenerator");
	this->hg_createPerlinGenerator = (PFN_createPerlinGenerator*) loadOrExplode("createPerlinGenerator");
	this->hg_createBrownianPerlinGenerator = (PFN_createBrownianPerlinGenerator*) loadOrExplode("createBrownianPerlinGenerator");
	this->hg_destroyGenerator = (PFN_destroyGenerator*) loadOrExplode("destroyGenerator");

	this->hg_getChunk = (PFN_getChunk*) loadOrExplode("getChunk");
	this->hg_getPoint = (PFN_getPoint*) loadOrExplode("getPoint");
	this->hg_requestChunk = (PFN_requestChunk*) loadOrExplode("requestChunk");
	this->hg_requestPoint = (PFN_requestPoint*) loadOrExplode("requestPoint");
	this->hg_probeChunk = (PFN_probeChunk*) loadOrExplode("probeChunk");
	this->hg_probePoint = (PFN_probePoint*) loadOrExplode("probePoint");
	this->hg_cleanChunkFromCache = (PFN_cleanChunkFromCache*) loadOrExplode("cleanChunkFromCache");
	this->hg_clearAllCache = (PFN_clearAllCache*) loadOrExplode("clearAllCache");

	for (auto& context : contexts) {
		context->load();
	}
}

void SpringNative::unload() {
	for (auto& context : contexts) {
		context->unload();
	}

	lib.reset(nullptr);
}

template <typename T>
static void erase_element(std::vector<std::unique_ptr<T>>& elements, const T* element) {
	std::erase_if(elements, [element] (auto& ptr) { return ptr.get() == element; } );
}

/*
 * API Implementation
 */

HG_API void reloadManagedImplementation() {
	std::unique_lock lock(mutex);
	SpringNative& native = getNative();

	native.unload();
	native.load();
}

HG_API const char* smokeTest(const char* data) {
	std::shared_lock lock(mutex);
    return getNative().hg_smokeTest(data);
}

HG_API Context createContext() {
	std::shared_lock lock(mutex);
	SpringNative& native = getNative();
	auto& context = native.contexts.emplace_back(std::make_unique<SpringContext>());
	context->load();
    return (Context) context.get();
}

HG_API void destroyContext(Context ctx) {
	std::shared_lock lock(mutex);
	SpringNative& native = getNative();
	auto* context = (SpringContext*) ctx;
	context->unload();
	erase_element(native.contexts, context);
}

HG_API Generator createCoordinateGenerator(Context ctx, CommonSettings common) {
	std::shared_lock lock(mutex);
	auto* context = (SpringContext*) ctx;
	auto& generator = context->generators.emplace_back(new SprintGenerator(context, COORDINATE, common, {}));
	return (Generator) generator.get();
}

HG_API Generator createRandomGenerator(Context ctx, CommonSettings common) {
	std::shared_lock lock(mutex);
	auto* context = (SpringContext*) ctx;
	auto& generator = context->generators.emplace_back(new SprintGenerator(context, RANDOM, common, {}));
	return (Generator) generator.get();
}

HG_API Generator createVoronoiGenerator(Context ctx, CommonSettings common) {
	std::shared_lock lock(mutex);
	auto* context = (SpringContext*) ctx;
	auto& generator = context->generators.emplace_back(new SprintGenerator(context, VORONOI, common, {}));
	return (Generator) generator.get();
}

HG_API Generator createPerlinGenerator(Context ctx, CommonSettings common) {
	std::shared_lock lock(mutex);
	auto* context = (SpringContext*) ctx;
	auto& generator = context->generators.emplace_back(new SprintGenerator(context, PERLIN, common, {}));
	return (Generator) generator.get();
}

HG_API Generator createBrownianPerlinGenerator(Context ctx, CommonSettings common) {
	std::shared_lock lock(mutex);
	auto* context = (SpringContext*) ctx;
	auto& generator = context->generators.emplace_back(new SprintGenerator(context, BROWNIAN, common, {}));
	return (Generator) generator.get();
}

HG_API Generator createHydraulicErosionGenerator(Context ctx, CommonSettings common, HydraulicErosionSettings erosion) {
	std::shared_lock lock(mutex);
	auto* context = (SpringContext*) ctx;
	auto& generator = context->generators.emplace_back(new SprintGenerator(context, HYDRAULIC, common, erosion));
	return (Generator) generator.get();
}

HG_API void destroyGenerator(Generator gen) {
	auto* generator = (SprintGenerator*) gen;
	if (generator == nullptr) {
    	return;
    }

	std::shared_lock lock(mutex);
	generator->unload();
	erase_element(generator->context->generators, generator);
}

HG_API void getChunk(Generator gen, int32_t x, int32_t y, float* buffer) {
	auto* generator = (SprintGenerator*) gen;

    if (generator == nullptr) {
	    return;
    }

	std::shared_lock lock(mutex);
	return getNative().hg_getChunk(generator->impl, x, y, buffer);
}

HG_API float getPoint(Generator gen, int32_t x, int32_t y) {
	auto* generator = (SprintGenerator*) gen;

	if (generator == nullptr) {
		return 0.0f;
	}

	std::shared_lock lock(mutex);
	return getNative().hg_getPoint(generator->impl, x, y);
}

HG_API void requestChunk(Generator gen, int32_t x, int32_t y) {
	// do nothing - we force all requests though the synchronous API
}

HG_API void requestPoint(Generator gen, int32_t x, int32_t y) {
	// do nothing - we force all requests though the synchronous API
}

HG_API bool probeChunk(Generator gen, int32_t x, int32_t y, float* buffer) {
	getChunk(gen, x, y, buffer);
	return true;
}

HG_API bool probePoint(Generator gen, int32_t x, int32_t y, float* point) {
	*point = getPoint(gen, x, y);
	return true;
}

HG_API void cleanChunkFromCache(Generator gen, int32_t x, int32_t y) {
	auto* generator = (SprintGenerator*) gen;

	if (generator == nullptr) {
		return;
	}

	std::shared_lock lock(mutex);
	return getNative().hg_cleanChunkFromCache(generator->impl, x, y);
}

HG_API void clearAllCache(Generator gen) {
	auto* generator = (SprintGenerator*) gen;

	if (generator == nullptr) {
		return;
	}

	std::shared_lock lock(mutex);
	return getNative().hg_clearAllCache(generator->impl);
}
