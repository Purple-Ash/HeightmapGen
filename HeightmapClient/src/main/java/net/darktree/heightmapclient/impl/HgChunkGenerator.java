package net.darktree.heightmapclient.impl;

import com.mojang.serialization.MapCodec;
import com.mojang.serialization.codecs.RecordCodecBuilder;
import net.darktree.heightmapclient.HgMod;
import net.darktree.heightmapclient.api.HgGenerator;
import net.minecraft.core.BlockPos;
import net.minecraft.core.Holder;
import net.minecraft.core.SectionPos;
import net.minecraft.resources.RegistryOps;
import net.minecraft.server.level.WorldGenRegion;
import net.minecraft.world.level.*;
import net.minecraft.world.level.biome.Biome;
import net.minecraft.world.level.biome.BiomeManager;
import net.minecraft.world.level.biome.Biomes;
import net.minecraft.world.level.biome.FixedBiomeSource;
import net.minecraft.world.level.block.Block;
import net.minecraft.world.level.block.Blocks;
import net.minecraft.world.level.block.state.BlockState;
import net.minecraft.world.level.chunk.ChunkAccess;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.levelgen.Heightmap;
import net.minecraft.world.level.levelgen.RandomState;
import net.minecraft.world.level.levelgen.blending.Blender;
import net.minecraft.world.level.levelgen.densityfunction.SamplerContext;
import org.jspecify.annotations.NullMarked;
import org.jspecify.annotations.Nullable;

import java.util.List;
import java.util.Set;
import java.util.concurrent.CompletableFuture;

@NullMarked
public final class HgChunkGenerator extends ChunkGenerator {

	// this needs to match with the dimension value!
	private static final int HEIGHT = 255;

	public static final MapCodec<HgChunkGenerator> CODEC = RecordCodecBuilder.mapCodec(
			(i) -> i.group(RegistryOps.retrieveElement(Biomes.PLAINS)).apply(i, i.stable(HgChunkGenerator::new))
	);

	public HgChunkGenerator(final Holder.Reference<Biome> plains) {
		super(new FixedBiomeSource(plains));
	}

	@Override
	protected MapCodec<HgChunkGenerator> codec() {
		return CODEC;
	}

	@Override
	public void spawnOriginalMobs(WorldGenRegion worldGenRegion) {
		// spawn nothing
	}

	@Override
	public CompletableFuture<ChunkAccess> buildTerrain(ChunkAccess chunk, Blender blender, RandomState randomState, StructureManager structureManager, BiomeManager biomeManager, @Nullable WorldGenRegion carverBiomeRegion, Set<Holder<Biome>> possibleBiomes) {
		return CompletableFuture.supplyAsync(() -> {
			generateChunkAsync(chunk);
			return chunk;

		});
	}

	@Override
	public void applyBiomeDecoration(final WorldGenLevel level, final ChunkAccess chunk, final StructureManager structureManager) {
		if (HgMod.DIRTIZE_SURFACE) {
			placeSurfaceDirt(level, chunk);
		}

		if (HgMod.DECORATE_SURFACE) {
			super.applyBiomeDecoration(level, chunk, structureManager);
		}

		placeBedrockLayer(level, chunk);
	}

	@Override
	public NoiseColumn getBaseColumn(int x, int z, LevelHeightAccessor heightAccessor, RandomState randomState) {
		return new NoiseColumn(0, new BlockState[0]); // doohickey
	}

	@Override
	public void addDebugScreenInfo(List<String> result, RandomState randomState, BlockPos feetPos, SamplerContext samplerContext) {
		// head empty
	}

	@Override
	public int getBaseHeight(int x, int z, Heightmap.Types type, LevelHeightAccessor heightAccessor, RandomState randomState) {
		return HEIGHT;
	}

	@Override
	public int getSeaLevel() {
		return 0;
	}

	@Override
	public int getMinY() {
		return 0;
	}

	@Override
	public int getGenDepth() {
		return HEIGHT;
	}

	private void generateChunkAsync(ChunkAccess chunk) {
		BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
		BlockState stone = Blocks.STONE.defaultBlockState();

		HgGenerator generator = HgMod.getGenerator();

		ChunkPos centerPos = chunk.getPos();
		int cx = centerPos.x();
		int cz = centerPos.z();

		for (int z = 0; z < 16; z ++) {
			for (int x = 0; x < 16; x ++) {
				final int wx = SectionPos.sectionToBlockCoord(cx, x);
				final int wz = SectionPos.sectionToBlockCoord(cz, z);

				int height = (int) (generator.getPoint(wx, wz) * HEIGHT);

				if (height > HEIGHT) {
					height = HEIGHT;
				}

				for (int i = 1; i < height; i ++) {
					chunk.setBlockState(pos.set(x, i, z), stone);
				}
			}
		}
	}

	private void placeSurfaceDirt(WorldGenLevel level, ChunkAccess chunk) {

		BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
		ChunkPos centerPos = chunk.getPos();
		int cx = centerPos.x();
		int cz = centerPos.z();

		for (int x = 0; x < 16; x++) {
			for (int z = 0; z < 16; z++) {

				final int wx = SectionPos.sectionToBlockCoord(cx, x);
				final int wz = SectionPos.sectionToBlockCoord(cz, z);

				int depth = 10;

				// skip first top layer and the last bottom one
				for (int y = chunk.getHeight() - 2; y > 1; y --) {

					pos.set(wx, y, wz);
					Block block = level.getBlockState(pos).getBlock();

					if (block == Blocks.AIR) {
						depth = 0;
					} else if ((block == Blocks.STONE) && (depth < 10)) {
						BlockState surface = getSurfaceBlock(depth);
						depth ++;

						level.setBlock(pos, surface, 0);
					}
				}
			}
		}
	}

	private void placeBedrockLayer(WorldGenLevel level, ChunkAccess chunk) {

		BlockPos.MutableBlockPos pos = new BlockPos.MutableBlockPos();
		ChunkPos centerPos = chunk.getPos();
		int cx = centerPos.x();
		int cz = centerPos.z();

		for (int x = 0; x < 16; x++) {
			for (int z = 0; z < 16; z++) {

				final int wx = SectionPos.sectionToBlockCoord(cx, x);
				final int wz = SectionPos.sectionToBlockCoord(cz, z);

				level.setBlock(pos.set(wx, 0, wz), Blocks.BEDROCK.defaultBlockState(), 0);
			}
		}
	}

	private BlockState getSurfaceBlock(int depth) {
		if (depth == 0) return Blocks.GRASS_BLOCK.defaultBlockState();
		if (depth == 1) return Blocks.DIRT.defaultBlockState();

		return Blocks.STONE.defaultBlockState();
	}

}
