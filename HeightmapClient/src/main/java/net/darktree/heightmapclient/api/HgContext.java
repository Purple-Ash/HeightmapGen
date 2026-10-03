package net.darktree.heightmapclient.api;

import com.sun.jna.Pointer;
import net.darktree.heightmapclient.api.binding.HeightmapGen;
import net.darktree.heightmapclient.api.binding.HeightmapGenAccess;

public final class HgContext implements AutoCloseable {

	private final HeightmapGen library;
	private final Pointer context;

	private HgContext() {
		this.library = HeightmapGenAccess.getInstance();
		this.context = library.createContext();
	}

	public static HgContext create() {
		return new HgContext();
	}

	public HgGenerator createCoordinateGenerator(HgCommonSettings settings) {
		return new HgGenerator(library.createCoordinateGenerator(context, settings.getSettings()));
	}

	public HgGenerator createRandomGenerator(HgCommonSettings settings) {
		return new HgGenerator(library.createRandomGenerator(context, settings.getSettings()));
	}

	public HgGenerator createVoronoiGenerator(HgCommonSettings settings) {
		return new HgGenerator(library.createVoronoiGenerator(context, settings.getSettings()));
	}

	public HgGenerator createHydraulicErosionGenerator(HgCommonSettings settings, HgHydraulicErosionSettings erosionSettings) {
		return new HgGenerator(library.createHydraulicErosionGenerator(context, settings.getSettings(), erosionSettings.getSettings()));
	}

	public HgGenerator createPerlinGenerator(HgCommonSettings settings) {
		return new HgGenerator(library.createPerlinGenerator(context, settings.getSettings()));
	}

	public HgGenerator createBrownianPerlinGenerator(HgCommonSettings settings) {
		return new HgGenerator(library.createBrownianPerlinGenerator(context, settings.getSettings()));
	}

	Pointer getHandle() {
		return context;
	}

	@Override
	public void close() {
		library.destroyContext(context);
	}


}
