#include "library.hpp"
#include "args.hpp"

#include <array>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <random>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

namespace fs = std::filesystem;

namespace opt {
	using OutputDirectory =            Option<fs::path,  "output",                  "Output directory for generated heightmaps", true>;
	using DllPath =                    Option<fs::path,  "dll",                     "HeightmapGen DLL path",                     false>;
	using Samples =                    OptionD<int32_t,  "samples",                 "Number of samples to generate",             false, 10>;
	using ImageSize =                  OptionD<int32_t,  "size",                    "Image width and height",                    false, 128>;
	using DatasetSeed =                OptionD<uint32_t, "dataset-seed",            "Dataset origin/scale seed",                 false, 0>;
	using ScaleMin =                   OptionD<float,    "scale-min",               "Minimum Brownian-Perlin frequency",         false, 0.005f>;
	using ScaleMax =                   OptionD<float,    "scale-max",               "Maximum Brownian-Perlin frequency",         false, 0.05f>;
	using Overwrite =                  OptionD<bool,     "overwrite",               "Overwrite existing output directory",       false, false>;
	namespace erosion {
		using Seed =                   OptionD<uint32_t, "erosion-seed",            "Erosion seed",                              false, 0>;
		using Iterations =             OptionD<int32_t,  "erosion-iterations",      "Erosion iterations",                        false, 1>;
		using Radius =                 OptionD<int32_t,  "erosion-radius",          "Erosion radius",                            false, 3>;
		using MaxDropletLifetime =     OptionD<int32_t,  "erosion-lifetime",        "Erosion max droplet lifetime",              false, 30>;
		using Inertia =                OptionD<float,    "erosion-inertia",         "Erosion inertia",                           false, 0.05f>;
		using SedimentCapacityFactor = OptionD<float,    "erosion-scf",             "Erosion sediment capacity factor",          false, 4.0f>;
		using MinSedimentCapacity =    OptionD<float,    "erosion-min-sediment",    "Erosion min sediment capacity",             false, 0.01f>;
		using ErodeSpeed =             OptionD<float,    "erosion-erode-speed",     "Erosion erode speed",                       false, 0.3f>;
		using DepositSpeed =           OptionD<float,    "erosion-deposit-speed",   "Erosion deposit speed",                     false, 0.3f>;
		using EvaporateSpeed =         OptionD<float,    "erosion-evaporate-speed", "Erosion evaporate speed",                   false, 0.01f>;
		using Gravity =                OptionD<float,    "erosion-gravity",         "Erosion gravity",                           false, 4.0f>;
		using InitialSpeed =           OptionD<float,    "erosion-initial-speed",   "Erosion initial speed",                     false, 1.0f>;
		using InitialWaterVolume =     OptionD<float,    "erosion-initial-water",   "Erosion initial water volume",              false, 1.0f>;
	}
}

using Options = OptionRegistry<
	opt::OutputDirectory, opt::DllPath, opt::Samples, opt::ImageSize, 
	opt::DatasetSeed, opt::ScaleMin, opt::ScaleMax, opt::Overwrite,
	opt::erosion::Seed, opt::erosion::Iterations, opt::erosion::Radius, opt::erosion::MaxDropletLifetime, 
	opt::erosion::Inertia, opt::erosion::SedimentCapacityFactor, opt::erosion::MinSedimentCapacity,
	opt::erosion::ErodeSpeed, opt::erosion::DepositSpeed, opt::erosion::EvaporateSpeed, 
	opt::erosion::Gravity, opt::erosion::InitialSpeed, opt::erosion::InitialWaterVolume
>;

HydraulicErosionSettings makeErosionSettings(const Options& options) {
	HydraulicErosionSettings settings{};
	settings.seed = options.get<opt::erosion::Seed>();
	settings.numIterations = options.get<opt::erosion::Iterations>();
	settings.erosionRadius = options.get<opt::erosion::Radius>();
	settings.maxDropletLifetime = options.get<opt::erosion::MaxDropletLifetime>();
	settings.inertia = options.get<opt::erosion::Inertia>();
	settings.sedimentCapacityFactor = options.get<opt::erosion::SedimentCapacityFactor>();
	settings.minSedimentCapacity = options.get<opt::erosion::MinSedimentCapacity>();
	settings.erodeSpeed = options.get<opt::erosion::ErodeSpeed>();
	settings.depositSpeed = options.get<opt::erosion::DepositSpeed>();
	settings.evaporateSpeed = options.get<opt::erosion::EvaporateSpeed>();
	settings.gravity = options.get<opt::erosion::Gravity>();
	settings.initialSpeed = options.get<opt::erosion::InitialSpeed>();
	settings.initialWaterVolume = options.get<opt::erosion::InitialWaterVolume>();
	return settings;
}

class ContextGuard {
public:
	ContextGuard(HeightmapGenerator& hg) : hg(hg) {
		context = hg.api().createContext();
	}
	~ContextGuard() {
		hg.api().destroyContext(context);
	}
	Context get() const { 
		return context;
	}

private:
	HeightmapGenerator& hg;
	Context context;
};

fs::path defaultDllPath() {
#ifdef _WIN32
	std::array<wchar_t, MAX_PATH> executablePath{};
	const auto length = GetModuleFileNameW(nullptr, executablePath.data(), executablePath.size());
	if (length == 0 || length == executablePath.size()) return {};
	return fs::path(executablePath.data()).parent_path() / L"HeightmapGen.dll";
#else
	std::error_code error;
	const auto executablePath = fs::read_symlink("/proc/self/exe", error);
	if (error) return {};
	return executablePath.parent_path() / "libHeightmapGen.so";
#endif
}

bool writeBin(const fs::path& path, const std::vector<float>& values) {
	std::ofstream output(path, std::ios::binary | std::ios::trunc);
	if (!output) return false;
	output.write(reinterpret_cast<const char*>(values.data()), values.size() * sizeof(float));
	return output.good();
}

std::string sampleName(int32_t index) {
	std::ostringstream name;
	name << std::setfill('0') << std::setw(6) << index << ".bin";
	return name.str();
}

bool prepareOutput(const fs::path& root) {
	std::error_code error;
	if (fs::exists(root, error)) {
		if (error
			|| !fs::is_directory(root, error) || error
			|| fs::directory_iterator(root) != fs::directory_iterator()) {
			return false;
		}
	}
	else if (!fs::create_directories(root, error) || error) {
		return false;
	}
	bool in = fs::create_directories(root / "in", error) && !error;
	bool out = fs::create_directories(root / "out", error) && !error;
	return in && out;
}

int main(int argc, char** argv) {
	Options options;
	if (!options.parseArguments(argc, argv)) {
		std::cout << "\nUsage: " << argv[0] << " [options]\n";
		options.help();
		return EXIT_FAILURE;
	}

	const auto dllPath = options.get<opt::DllPath>().empty() ? defaultDllPath() : options.get<opt::DllPath>();

	HeightmapGenerator hg;
	if (dllPath.empty() || !hg.load(dllPath)) {
		std::cout << "Could not load the DLL " << dllPath.string() << ".\n";
		return EXIT_FAILURE;
	}

	if (!prepareOutput(options.get<opt::OutputDirectory>()) && !options.get<opt::Overwrite>()) {
		std::cout << "Output directory must be absent or empty: " << options.get<opt::OutputDirectory>() << "\n";
		std::cout << "Use --overwrite to overwrite the output directory.\n";
		return EXIT_FAILURE;
	}

	std::mt19937 random(options.get<opt::DatasetSeed>());
	std::uniform_int_distribution<int32_t> originDistribution(-100000, 100000);
	std::uniform_real_distribution<float> logScaleDistribution(std::log(options.get<opt::ScaleMin>()), std::log(options.get<opt::ScaleMax>()));

	for (int32_t index = 0; index < options.get<opt::Samples>(); index++) {
		const int32_t regionX = originDistribution(random);
		const int32_t regionY = originDistribution(random);
		const float scale = std::exp(logScaleDistribution(random));
		const auto name = sampleName(index);

		ContextGuard context(hg);
		std::vector<float> preErosion;
		std::vector<float> eroded;
		preErosion.resize(options.get<opt::ImageSize>() * options.get<opt::ImageSize>());
		eroded.resize(options.get<opt::ImageSize>() * options.get<opt::ImageSize>());

		CommonSettings commonSettings{
			.seed = options.get<opt::DatasetSeed>(),
			.scale = scale,
			.amplitude = 1.0f,
			.resolution = options.get<opt::ImageSize>(),
			.cacheable = false 
		};

		CommonSettings baseSettings = commonSettings;
		baseSettings.amplitude = 1.0f;
		Generator baseGenerator = hg.api().createBrownianPerlinGenerator(context.get(), baseSettings);

		HydraulicErosionSettings erosionSettings = makeErosionSettings(options);
		erosionSettings.baseGeneratorImpl = baseGenerator;
		Generator erosionGenerator = hg.api().createHydraulicErosionGenerator(context.get(), commonSettings, erosionSettings);

		const bool generatorsReady = baseGenerator != nullptr && erosionGenerator != nullptr;
		if (generatorsReady) {
			hg.api().getChunk(baseGenerator, regionX, regionY, preErosion.data());
			hg.api().getChunk(erosionGenerator, regionX, regionY, eroded.data());
		}

		if (!generatorsReady
			|| !writeBin(options.get<opt::OutputDirectory>() / "in" / name, preErosion)
			|| !writeBin(options.get<opt::OutputDirectory>() / "out" / name, eroded)) {
			std::cout << "Failed while generating sample " << index << ".\n";
			return EXIT_FAILURE;
		}

		std::cout << "Generated " << index + 1 << '/' << options.get<opt::Samples>() << "\r" << std::flush;
	}

	std::cout << "\nWrote " << options.get<opt::Samples>() << " paired heightmaps to " << options.get<opt::OutputDirectory>().string() << ".\n";
	return EXIT_SUCCESS;
}
