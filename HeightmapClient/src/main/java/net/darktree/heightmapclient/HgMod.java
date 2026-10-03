package net.darktree.heightmapclient;

import net.darktree.heightmapclient.api.HgCommonSettings;
import net.darktree.heightmapclient.api.HgContext;
import net.darktree.heightmapclient.api.HgGenerator;
import net.darktree.heightmapclient.api.HgHydraulicErosionSettings;
import net.fabricmc.api.ModInitializer;
import net.fabricmc.fabric.api.entity.event.v1.ServerPlayerEvents;
import net.minecraft.commands.CommandSourceStack;
import net.minecraft.commands.Commands;
import net.minecraft.resources.Identifier;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class HgMod implements ModInitializer {

	public static final String ID = "heightmapgen";
	public static final Logger LOGGER = LoggerFactory.getLogger("HeightmapGen");

	public static Identifier id(String path) {
		return Identifier.fromNamespaceAndPath(ID, path);
	}

	// settings
	public static boolean DIRTIZE_SURFACE = true;
	public static boolean DECORATE_SURFACE = true;

	public static HgContext CONTEXT;
	public static HgCommonSettings COMMON;
	public static HgHydraulicErosionSettings HYDRAULIC;

	public static GeneratorType TYPE = GeneratorType.VORONOI;
	private static HgGenerator GENERATOR = null;

	public static void markGeneratorDirty() {
		GENERATOR = null;
	}

	public synchronized static HgGenerator getGenerator() {
		HgGenerator generator = GENERATOR;

		if (generator == null) {
			generator = createGenerator();
			GENERATOR = generator;
		}

		return generator;
	}

	public static void setGeneratorType(GeneratorType type) {
		TYPE = type;
	}

	@Override
	public void onInitialize() {
		CONTEXT = HgContext.create();

		COMMON = HgCommonSettings.create()
				.setSeed(42)
				.setScale(0.03f)
				.setAmplitude(0.4f)
				.setResolution(100)
				.setCacheable(true);

		HYDRAULIC = HgHydraulicErosionSettings.create()
				.setSeed(42)
				.setBaseGeneratorImpl(GeneratorType.BROWNIAN);

		ServerPlayerEvents.JOIN.register(player -> {
			Commands commands = player.level().getServer().getCommands();
			CommandSourceStack source = player.createCommandSourceStack();

			// splendid
			commands.performPrefixedCommand(source, "/execute as @s in heightmapgen:test run tp @s 0 30 0");
			commands.performPrefixedCommand(source, "/gamemode spectator");
		});
	}

	private static HgGenerator createGenerator() {
		LOGGER.info("Reloading generator...");
		return TYPE.createInstance();
	}

}
