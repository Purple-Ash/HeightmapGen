package net.darktree.heightmapclient.mixin;

import com.mojang.serialization.MapCodec;
import net.darktree.heightmapclient.HgMod;
import net.darktree.heightmapclient.impl.HgChunkGenerator;
import net.minecraft.core.Registry;
import net.minecraft.world.level.chunk.ChunkGenerator;
import net.minecraft.world.level.chunk.ChunkGenerators;
import org.spongepowered.asm.mixin.Mixin;
import org.spongepowered.asm.mixin.injection.At;
import org.spongepowered.asm.mixin.injection.Inject;
import org.spongepowered.asm.mixin.injection.callback.CallbackInfoReturnable;

@Mixin(ChunkGenerators.class)
public class ChunkGeneratorsMixin {

	@Inject(method = "bootstrap", at = @At("TAIL"))
	private static void injectGenerators(Registry<MapCodec<? extends ChunkGenerator>> registry, CallbackInfoReturnable<MapCodec<? extends ChunkGenerator>> cir) {
		Registry.register(registry, HgMod.id("generator"), HgChunkGenerator.CODEC);
	}

}
