package net.darktree.heightmapclient;

import net.darktree.heightmapclient.api.HgCommonSettings;
import net.darktree.heightmapclient.api.HgContext;
import net.darktree.heightmapclient.api.HgGenerator;
import net.darktree.heightmapclient.api.HgHydraulicErosionSettings;
import net.darktree.heightmapclient.api.binding.HeightmapGen;
import net.darktree.heightmapclient.api.binding.HeightmapGenAccess;
import net.fabricmc.api.ModInitializer;
import net.minecraft.resources.Identifier;

public class HgMod implements ModInitializer {

	public static final String ID = "heightmapgen";

	public static Identifier id(String path) {
		return Identifier.fromNamespaceAndPath(ID, path);
	}

	// settings
	public static boolean DIRTIZE_SURFACE = true;
	public static boolean DECORATE_SURFACE = true;

	public static HgContext CONTEXT;
	public static HgGenerator GENERATOR;

	@Override
	public void onInitialize() {
		CONTEXT = HgContext.create();
		GENERATOR = createVoronoiGenerator();
	}

	/*
	 * Generators, pick one and assign to GENERATOR
	 */

	private HgGenerator createPerlinGenerator() {
		return CONTEXT.createPerlinGenerator(
				HgCommonSettings.create()
						.setSeed(42)
						.setScale(0.03f)
						.setAmplitude(0.4f)
						.setResolution(100)
						.setCacheable(true)
		);
	}

	private HgGenerator createVoronoiGenerator() {
		return CONTEXT.createVoronoiGenerator(
				HgCommonSettings.create()
						.setSeed(42)
						.setScale(0.03f)
						.setAmplitude(0.4f)
						.setResolution(100)
						.setCacheable(true)
		);
	}

	private HgGenerator createBrownianPerlinGenerator() {
		return CONTEXT.createBrownianPerlinGenerator(
				HgCommonSettings.create()
						.setSeed(42)
						.setScale(0.03f)
						.setAmplitude(0.3f)
						.setResolution(100)
						.setCacheable(true)
		);
	}

	// this one is unusable and crashes the JVM
	private HgGenerator createHydraulicErosion() {
		return CONTEXT.createHydraulicErosionGenerator(
				HgCommonSettings.create()
						.setSeed(42)
						.setScale(0.01f)
						.setAmplitude(0.1f)
						.setResolution(100)
						.setCacheable(true),

				HgHydraulicErosionSettings.create()
						.setSeed(42)
						.setBaseGeneratorImpl(createBrownianPerlinGenerator())
		);
	}

}
