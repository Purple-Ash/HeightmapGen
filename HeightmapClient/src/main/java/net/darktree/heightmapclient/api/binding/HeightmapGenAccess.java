package net.darktree.heightmapclient.api.binding;

import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.sun.jna.Native;
import com.sun.jna.Platform;
import com.sun.jna.Pointer;
import net.fabricmc.loader.api.FabricLoader;

import java.nio.file.Files;
import java.nio.file.Path;

public class HeightmapGenAccess {

	private static Path getLibraryPath() {
		final Path game = FabricLoader.getInstance().getGameDir();
		final Path library = game.resolve("HeightmapGen." + (Platform.isLinux()
				? "so"
				: "dll"
		));

		if (!Files.exists(library)) {
			throw new RuntimeException("Library '" + library + "' not found!");
		}

		return library;
	}

	private static HeightmapGen loadLibrary() {
		HeightmapGen impl = Native.load(getLibraryPath().toString(), HeightmapGen.class);

		final long magic = 0x2137ff51L;
		final long result = Pointer.nativeValue(impl.smokeTest(Pointer.createConstant(magic)));

		if (result != magic) {
			throw new RuntimeException("Smoke test failed, expected " + magic + ", but got " + result + "!");
		}

		return impl;
	}

	private static final Supplier<HeightmapGen> LIBRARY = Suppliers.memoize(HeightmapGenAccess::loadLibrary);

	public static HeightmapGen getInstance() {
		return LIBRARY.get();
	}

}
