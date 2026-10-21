package net.darktree.heightmapclient.api;

import com.sun.jna.Pointer;
import net.darktree.heightmapclient.api.binding.HeightmapGen;
import net.darktree.heightmapclient.api.binding.HeightmapGenAccess;

public final class HgGenerator implements AutoCloseable {

	private final HeightmapGen library;
	private final Pointer generator;

	public HgGenerator(Pointer generator) {
		this.library = HeightmapGenAccess.getInstance();
		this.generator = generator;
	}

	public float getPoint(int x, int y) {
		return library.getPoint(generator, x, y);
	}

	public void cleanChunkFromCache(int x, int y) {
		library.cleanChunkFromCache(generator, x, y);
	}

	public void clearAllCache() {
		library.clearAllCache(generator);
	}

	Pointer getHandle() {
		return generator;
	}

	@Override
	public void close() {
		library.destroyGenerator(generator);
	}

}
