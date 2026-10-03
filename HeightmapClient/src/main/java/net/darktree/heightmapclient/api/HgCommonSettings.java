package net.darktree.heightmapclient.api;

import net.darktree.heightmapclient.api.binding.HeightmapGen;

public final class HgCommonSettings {

	private final HeightmapGen.CommonSettings settings;

	private HgCommonSettings() {
		settings = new HeightmapGen.CommonSettings();
	}

	public static HgCommonSettings create() {
		return new HgCommonSettings();
	}

	public HgCommonSettings setSeed(long seed) {
		settings.seed = seed;
		return this;
	}

	public HgCommonSettings setCacheable(boolean cacheable) {
		settings.cacheable = cacheable;
		return this;
	}

	public HgCommonSettings setResolution(int resolution) {
		settings.resolution = resolution;
		return this;
	}

	public HgCommonSettings setAmplitude(float amplitude) {
		settings.amplitude = amplitude;
		return this;
	}

	public HgCommonSettings setScale(float scale) {
		settings.scale = scale;
		return this;
	}

	public long getSeed() {
		return settings.seed;
	}

	public boolean getCacheable() {
		return settings.cacheable;
	}

	public int getResolution() {
		return settings.resolution;
	}

	public float getAmplitude() {
		return settings.amplitude;
	}

	public float getScale() {
		return settings.scale;
	}

	HeightmapGen.CommonSettings getSettings() {
		return settings;
	}

}
