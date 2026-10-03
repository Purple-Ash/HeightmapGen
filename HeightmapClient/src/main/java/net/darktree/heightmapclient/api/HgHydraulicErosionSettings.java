package net.darktree.heightmapclient.api;

import net.darktree.heightmapclient.GeneratorType;
import net.darktree.heightmapclient.api.binding.HeightmapGen;

public class HgHydraulicErosionSettings {

	private final HeightmapGen.HydraulicErosionSettings settings;

	private HgHydraulicErosionSettings() {
		settings = new HeightmapGen.HydraulicErosionSettings();
	}

	public GeneratorType type;

	public static HgHydraulicErosionSettings create() {
		return new HgHydraulicErosionSettings();
	}

	public HgHydraulicErosionSettings setBaseGeneratorImpl(GeneratorType type) {
		this.type = type;
		return this;
	}

	public HgHydraulicErosionSettings setInitialWaterVolume(float initialWaterVolume) {
		settings.initialWaterVolume = initialWaterVolume;
		return this;
	}

	public HgHydraulicErosionSettings setInitialSpeed(float initialSpeed) {
		settings.initialSpeed = initialSpeed;
		return this;
	}

	public HgHydraulicErosionSettings setGravity(float gravity) {
		settings.gravity = gravity;
		return this;
	}

	public HgHydraulicErosionSettings setEvaporateSpeed(float evaporateSpeed) {
		settings.evaporateSpeed = evaporateSpeed;
		return this;
	}

	public HgHydraulicErosionSettings setDepositSpeed(float depositSpeed) {
		settings.depositSpeed = depositSpeed;
		return this;
	}

	public HgHydraulicErosionSettings setErodeSpeed(float erodeSpeed) {
		settings.erodeSpeed = erodeSpeed;
		return this;
	}

	public HgHydraulicErosionSettings setMinSedimentCapacity(float minSedimentCapacity) {
		settings.minSedimentCapacity = minSedimentCapacity;
		return this;
	}

	public HgHydraulicErosionSettings setSedimentCapacityFactor(float sedimentCapacityFactor) {
		settings.sedimentCapacityFactor = sedimentCapacityFactor;
		return this;
	}

	public HgHydraulicErosionSettings setInertia(float inertia) {
		settings.inertia = inertia;
		return this;
	}

	public HgHydraulicErosionSettings setMaxDropletLifetime(int maxDropletLifetime) {
		settings.maxDropletLifetime = maxDropletLifetime;
		return this;
	}

	public HgHydraulicErosionSettings setErosionRadius(int erosionRadius) {
		settings.erosionRadius = erosionRadius;
		return this;
	}

	public HgHydraulicErosionSettings setNumIterations(int numIterations) {
		settings.numIterations = numIterations;
		return this;
	}

	public HgHydraulicErosionSettings setSeed(int seed) {
		settings.seed = seed;
		return this;
	}

	public float getInitialWaterVolume() {
		return settings.initialWaterVolume;
	}

	public float getInitialSpeed() {
		return settings.initialSpeed;
	}

	public float getGravity() {
		return settings.gravity;
	}

	public float getEvaporateSpeed() {
		return settings.evaporateSpeed;
	}

	public float getDepositSpeed() {
		return settings.depositSpeed;
	}

	public float getErodeSpeed() {
		return settings.erodeSpeed;
	}

	public float getMinSedimentCapacity() {
		return settings.minSedimentCapacity;
	}

	public float getSedimentCapacityFactor() {
		return settings.sedimentCapacityFactor;
	}

	public float getInertia() {
		return settings.inertia;
	}

	public int getMaxDropletLifetime() {
		return settings.maxDropletLifetime;
	}

	public int getErosionRadius() {
		return settings.erosionRadius;
	}

	public int getNumIterations() {
		return settings.numIterations;
	}

	public int getSeed() {
		return settings.seed;
	}

	HeightmapGen.HydraulicErosionSettings getSettings() {
		settings.baseGeneratorImpl = type.createInstance().getHandle();
		return settings;
	}

}
