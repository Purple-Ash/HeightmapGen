package net.darktree.heightmapclient;

import com.mojang.serialization.Codec;
import net.darktree.heightmapclient.api.HgGenerator;
import net.minecraft.network.chat.Component;
import net.minecraft.util.StringRepresentable;
import org.jspecify.annotations.NonNull;

import java.util.Arrays;
import java.util.function.Supplier;

public enum GeneratorType implements StringRepresentable {
	PERLIN("Perlin", () -> HgMod.CONTEXT.createPerlinGenerator(HgMod.COMMON)),
	VORONOI("Voronoi", () -> HgMod.CONTEXT.createVoronoiGenerator(HgMod.COMMON)),
	RANDOM("Random", () -> HgMod.CONTEXT.createRandomGenerator(HgMod.COMMON)),
	COORDINATE("Coordinate", () -> HgMod.CONTEXT.createCoordinateGenerator(HgMod.COMMON)),
	BROWNIAN("Brownian", () -> HgMod.CONTEXT.createBrownianPerlinGenerator(HgMod.COMMON)),
	HYDRAULIC("Hydraulic", () -> HgMod.CONTEXT.createHydraulicErosionGenerator(HgMod.COMMON, HgMod.HYDRAULIC));

	private final String name;
	private final Supplier<HgGenerator> supplier;

	GeneratorType(String name, Supplier<HgGenerator> supplier) {
		this.name = name;
		this.supplier = supplier;
	}

	public HgGenerator createInstance() {
		return supplier.get();
	}

	public Component getCaption() {
		return Component.literal(name);
	}

	@Override
	public @NonNull String getSerializedName() {
		return name;
	}

	public static GeneratorType[] valuesExceptHydraulic() {
		return Arrays.stream(values()).filter(type -> type != GeneratorType.HYDRAULIC).toArray(GeneratorType[]::new);
	}

	public static final Codec<GeneratorType> CODEC = StringRepresentable.fromEnum(GeneratorType::values);

}
