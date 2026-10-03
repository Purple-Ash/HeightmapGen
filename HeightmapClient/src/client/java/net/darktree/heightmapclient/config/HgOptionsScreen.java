package net.darktree.heightmapclient.config;

import com.mojang.serialization.Codec;
import net.darktree.heightmapclient.GeneratorType;
import net.darktree.heightmapclient.HgMod;
import net.minecraft.ChatFormatting;
import net.minecraft.client.Minecraft;
import net.minecraft.client.OptionInstance;
import net.minecraft.client.PreferredGraphicsApi;
import net.minecraft.client.gui.components.Tooltip;
import net.minecraft.client.gui.screens.Screen;
import net.minecraft.client.gui.screens.options.OptionsSubScreen;
import net.minecraft.network.chat.CommonComponents;
import net.minecraft.network.chat.Component;

import java.util.ArrayList;
import java.util.List;
import java.util.function.Consumer;
import java.util.function.Function;
import java.util.function.Supplier;

public class HgOptionsScreen extends OptionsSubScreen {

	private static final Component COMMON_HEADER = Component.translatable("config.heightmapgen.header.common").withStyle(ChatFormatting.UNDERLINE, ChatFormatting.BOLD);
	private static final Component HYDRAULIC_HEADER = Component.translatable("config.heightmapgen.header.hydraulic").withStyle(ChatFormatting.UNDERLINE, ChatFormatting.BOLD);
	private static final Component MINECRAFT_HEADER = Component.translatable("config.heightmapgen.header.minecraft").withStyle(ChatFormatting.UNDERLINE, ChatFormatting.BOLD);

	public HgOptionsScreen(Screen parent) {
		super(parent, Minecraft.getInstance().options, Component.translatable("config.heightmapgen.title"));
	}

	private static OptionInstance<Boolean> createBoolOption(String name, Supplier<Boolean> getter, Consumer<Boolean> setter) {
		return OptionInstance.createBoolean(
				"config.heightmapgen.option." + name,
				getter.get(),
				setter::accept
		);
	}

	private static OptionInstance<Float> createFloatOption(String name, float min, float max, Supplier<Float> getter, Consumer<Float> setter) {
		final float range = max - min;

		return new OptionInstance<>(
				"config.heightmapgen.option." + name,
				OptionInstance.noTooltip(),
				(text, value) -> Component.translatable("config.heightmapgen.option." + name, value),
				new OptionInstance.IntRange(0, 1000).xmap(
						value -> (value / 1000.0f * range + min),
						value -> (int) (((value - min) / range) * 1000),
						false
				),
				Codec.floatRange(min, max), getter.get(), setter::accept
		);
	}

	private static OptionInstance<Integer> createIntOption(String name, int min, int max, Supplier<Integer> getter, Consumer<Integer> setter) {
		return new OptionInstance<>(
				"config.heightmapgen.option." + name,
				OptionInstance.noTooltip(),
				(text, value) -> Component.translatable("config.heightmapgen.option." + name, value),
				new OptionInstance.IntRange(min, max),
				Codec.intRange(min, max), getter.get(), setter::accept
		);
	}

	private static <T extends Enum<T>> OptionInstance<T> createEnumOption(String name, T[] values, T fallback, Codec<T> codec, Function<T, Component> captioner, Consumer<T> setter) {
		return new OptionInstance<T>(
				"config.heightmapgen.option." + name,
				OptionInstance.noTooltip(),
				(caption, value) -> captioner.apply(value),
				new OptionInstance.Enum<T>(List.of(values), codec),
				codec,
				fallback,
				setter::accept
		);
	}

	@Override
	protected void addOptions() {

		final OptionInstance<GeneratorType> mainGenerator = createEnumOption("mainGenerator", GeneratorType.values(), HgMod.TYPE, GeneratorType.CODEC, GeneratorType::getCaption, HgMod::setGeneratorType);
		final OptionInstance<GeneratorType> baseGenerator = createEnumOption("baseGenerator", GeneratorType.valuesExceptHydraulic(), HgMod.HYDRAULIC.type, GeneratorType.CODEC, GeneratorType::getCaption, HgMod.HYDRAULIC::setBaseGeneratorImpl);

		list.addSmall(mainGenerator, baseGenerator);

		final OptionInstance<Float> scale = createFloatOption("common.scale", 0.0f, 1.0f, HgMod.COMMON::getScale, HgMod.COMMON::setScale);
		final OptionInstance<Float> amplitude = createFloatOption("common.amplitude", 0.0f, 1.0f, HgMod.COMMON::getAmplitude, HgMod.COMMON::setAmplitude);
		final OptionInstance<Integer> resolution = createIntOption("common.resolution", 1, 200, HgMod.COMMON::getResolution, HgMod.COMMON::setResolution);
		final OptionInstance<Boolean> cachable = createBoolOption("common.cachable", HgMod.COMMON::getCacheable, HgMod.COMMON::setCacheable);

		list.addHeader(COMMON_HEADER);
		list.addSmall(scale, amplitude, resolution, cachable);

		final OptionInstance<Float> initialWaterVolume = createFloatOption("hydraulic.initialWaterVolume", 0.0f, 10.0f, HgMod.HYDRAULIC::getInitialWaterVolume, HgMod.HYDRAULIC::setInitialWaterVolume);
		final OptionInstance<Float> initialSpeed = createFloatOption("hydraulic.initialSpeed", 0.0f, 10.0f, HgMod.HYDRAULIC::getInitialSpeed, HgMod.HYDRAULIC::setInitialSpeed);
		final OptionInstance<Float> gravity = createFloatOption("hydraulic.gravity", 0.0f, 10.0f, HgMod.HYDRAULIC::getGravity, HgMod.HYDRAULIC::setGravity);
		final OptionInstance<Float> evaporateSpeed = createFloatOption("hydraulic.evaporateSpeed", 0.0f, 0.1f, HgMod.HYDRAULIC::getEvaporateSpeed, HgMod.HYDRAULIC::setEvaporateSpeed);
		final OptionInstance<Float> depositSpeed = createFloatOption("hydraulic.depositSpeed", 0.0f, 1.0f, HgMod.HYDRAULIC::getDepositSpeed, HgMod.HYDRAULIC::setDepositSpeed);
		final OptionInstance<Float> erodeSpeed = createFloatOption("hydraulic.erodeSpeed", 0.0f, 1.0f, HgMod.HYDRAULIC::getErodeSpeed, HgMod.HYDRAULIC::setErodeSpeed);
		final OptionInstance<Float> minSedimentCapacity = createFloatOption("hydraulic.minSedimentCapacity", 0.0f, 0.1f, HgMod.HYDRAULIC::getMinSedimentCapacity, HgMod.HYDRAULIC::setMinSedimentCapacity);
		final OptionInstance<Float> sedimentCapacityFactor = createFloatOption("hydraulic.sedimentCapacityFactor", 0.0f, 10.0f, HgMod.HYDRAULIC::getSedimentCapacityFactor, HgMod.HYDRAULIC::setSedimentCapacityFactor);
		final OptionInstance<Float> inertia = createFloatOption("hydraulic.inertia", 0.0f, 1.0f, HgMod.HYDRAULIC::getInertia, HgMod.HYDRAULIC::setInertia);
		final OptionInstance<Integer> maxDropletLifetime = createIntOption("hydraulic.maxDropletLifetime", 1, 100, HgMod.HYDRAULIC::getMaxDropletLifetime, HgMod.HYDRAULIC::setMaxDropletLifetime);
		final OptionInstance<Integer> erosionRadius = createIntOption("hydraulic.erosionRadius", 1, 10, HgMod.HYDRAULIC::getErosionRadius, HgMod.HYDRAULIC::setErosionRadius);
		final OptionInstance<Integer> numIterations = createIntOption("hydraulic.numIterations", 1, 10, HgMod.HYDRAULIC::getNumIterations, HgMod.HYDRAULIC::setNumIterations);

		list.addHeader(HYDRAULIC_HEADER);
		list.addSmall(initialWaterVolume, initialSpeed, gravity, evaporateSpeed, depositSpeed, erodeSpeed, minSedimentCapacity, sedimentCapacityFactor, inertia, maxDropletLifetime, erosionRadius, numIterations);

		final OptionInstance<Boolean> dirtize = createBoolOption("minecraft.dirtize", () -> HgMod.DIRTIZE_SURFACE, v -> HgMod.DIRTIZE_SURFACE = v);
		final OptionInstance<Boolean> decorate = createBoolOption("minecraft.decorate", () -> HgMod.DECORATE_SURFACE, v -> HgMod.DECORATE_SURFACE = v);

		list.addHeader(MINECRAFT_HEADER);
		list.addSmall(dirtize, decorate);
	}

	@Override
	public void onClose() {
		super.onClose();

		HgMod.markGeneratorDirty();
	}
}
