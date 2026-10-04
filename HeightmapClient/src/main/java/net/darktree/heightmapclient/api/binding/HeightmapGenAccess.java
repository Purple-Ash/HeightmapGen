package net.darktree.heightmapclient.api.binding;

import com.google.common.base.Supplier;
import com.google.common.base.Suppliers;
import com.sun.jna.Native;
import com.sun.jna.Platform;
import com.sun.jna.Pointer;
import net.darktree.heightmapclient.HgMod;
import net.fabricmc.loader.api.FabricLoader;

import java.nio.file.Files;
import java.nio.file.Path;
import java.util.List;
import java.util.stream.Stream;

public class HeightmapGenAccess {

	private static Path getLibraryPath() {
		final Path game = FabricLoader.getInstance().getGameDir();

		return Stream.of(
				"SpringHeightmapGen.dll",
				"libSpringHeightmapGen.so",
				"HeightmapGen.dll",
				"libHeightmapGen.so"
		).map(game::resolve).filter(Files::exists).findFirst().orElseThrow(() -> new RuntimeException("Library not found!"));
	}

	private static HeightmapGen loadLibrary() {
		final String path = getLibraryPath().toString();

		final HeightmapGen impl = path.contains("Spring")
				? Native.load(path, SpringHeightmapGen.class)
				: Native.load(path, HeightmapGen.class);

		final long magic = 0x2137ff51L;
		final long result = Pointer.nativeValue(impl.smokeTest(Pointer.createConstant(magic)));

		if (result != magic) {
			throw new RuntimeException("Smoke test failed, expected " + magic + ", but got " + result + "!");
		}

		HgMod.LOGGER.info("Native library '{}' loaded (live-reloading supported: {})", path, isReloadable(impl));
		return impl;
	}

	private static final Supplier<HeightmapGen> LIBRARY = Suppliers.memoize(HeightmapGenAccess::loadLibrary);

	public static boolean isReloadable(HeightmapGen library) {
		return library instanceof SpringHeightmapGen;
	}

	public static boolean tryReload(HeightmapGen library) {
		if (library instanceof SpringHeightmapGen spring) {
			spring.reloadManagedImplementation();
			return true;
		}

		return false;
	}

	public static HeightmapGen getInstance() {
		return LIBRARY.get();
	}

}
