package net.darktree.heightmapclient.api.binding;

import com.sun.jna.Library;
import com.sun.jna.Pointer;
import com.sun.jna.Structure;
import net.darktree.heightmapclient.api.HgCommonSettings;

import java.nio.Buffer;
import java.util.List;

public interface HeightmapGen extends Library {

	@Structure.FieldOrder({"seed", "scale", "amplitude", "resolution", "cacheable"})
	class CommonSettings extends Structure implements Structure.ByValue {
		public long seed;
		public float scale;
		public float amplitude;
		public int resolution;
		public boolean cacheable;
	}

	@Structure.FieldOrder({"seed", "numIterations", "erosionRadius", "maxDropletLifetime", "inertia", "sedimentCapacityFactor", "minSedimentCapacity", "erodeSpeed", "depositSpeed", "evaporateSpeed", "gravity", "initialSpeed", "initialWaterVolume", "baseGeneratorImpl"})
	class HydraulicErosionSettings extends Structure implements Structure.ByValue {
		public int seed = 0;
		public int numIterations = 1;
		public int erosionRadius = 3;
		public int maxDropletLifetime = 30;
		public float inertia = 0.05f;
		public float sedimentCapacityFactor = 4.0f;
		public float minSedimentCapacity = 0.01f;
		public float erodeSpeed = 0.3f;
		public float depositSpeed = 0.3f;
		public float evaporateSpeed = 0.01f;
		public float gravity = 4.0f;
		public float initialSpeed = 1.0f;
		public float initialWaterVolume = 1.0f;
		public Pointer baseGeneratorImpl = null;
	}

	Pointer smokeTest(Pointer data);

	Pointer createContext();
	void destroyContext(Pointer ctx);

	Pointer createCoordinateGenerator(Pointer ctx, CommonSettings settings);
	Pointer createRandomGenerator(Pointer ctx, CommonSettings settings);
	Pointer createVoronoiGenerator(Pointer ctx, CommonSettings settings);
	Pointer createHydraulicErosionGenerator(Pointer ctx, CommonSettings settings, HydraulicErosionSettings erosionSettings);
	Pointer createPerlinGenerator(Pointer ctx, CommonSettings settings);
	Pointer createBrownianPerlinGenerator(Pointer ctx, CommonSettings settings);
	void destroyGenerator(Pointer generator);

	void getChunk(Pointer generator, int x, int y, Buffer buffer);
	float getPoint(Pointer generator, int x, int y);
	void requestChunk(Pointer generator, int x, int y);
	void requestPoint(Pointer generator, int x, int y);
	boolean probeChunk(Pointer generator, int x, int y, Buffer buffer);
	boolean probePoint(Pointer generator, int x, int y, Buffer point);

	void cleanChunkFromCache(Pointer generator, int x, int y);
	void clearAllCache(Pointer generator);

}
