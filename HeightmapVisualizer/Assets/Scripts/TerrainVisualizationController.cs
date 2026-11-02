using System;
using System.Collections.Generic;
using UnityEngine;

[DefaultExecutionOrder(-50)]
[DisallowMultipleComponent]
[RequireComponent(typeof(TerrainGen))]
[AddComponentMenu("Terrain/Visualization Controller")]
public sealed class TerrainVisualizationController : MonoBehaviour
{
    [Tooltip("Preset components from scene objects or prefab assets, in menu order.")]
    [SerializeField] private List<TerrainVisualization> visualizations = new List<TerrainVisualization>();
    [Min(0)][SerializeField] private int defaultVisualization;

    public IReadOnlyList<TerrainVisualization> Visualizations => visualizations;
    public TerrainVisualization SelectedVisualization { get; private set; }
    public event Action SelectionChanged;

    private TerrainGen terrain;

    private void Awake()
    {
        terrain = GetComponent<TerrainGen>();
        if (defaultVisualization < visualizations.Count)
        {
            SelectedVisualization = visualizations[Mathf.Max(0, defaultVisualization)];
        }
        if (SelectedVisualization == null)
        {
            SelectedVisualization = visualizations.Find(preset => preset != null);
        }
    }

    private void OnEnable()
    {
        terrain.ChunkGenerated += ApplyToChunk;
        ApplyToExistingChunks();
    }

    public void SelectVisualization(TerrainVisualization visualization)
    {
        if (!isActiveAndEnabled || visualization == null || !visualizations.Contains(visualization)) return;
        if (SelectedVisualization == visualization) return;

        SelectedVisualization = visualization;
        ApplyToExistingChunks();
        SelectionChanged?.Invoke();
    }

    private void ApplyToExistingChunks()
    {
        foreach (Chunk chunk in GetComponentsInChildren<Chunk>())
        {
            ApplyToChunk(chunk);
        }
    }

    private void ApplyToChunk(Chunk chunk)
    {
        chunk.ApplyVisualization(SelectedVisualization);
    }

    private void OnDisable()
    {
        terrain.ChunkGenerated -= ApplyToChunk;
        foreach (Chunk chunk in GetComponentsInChildren<Chunk>())
        {
            chunk.ApplyVisualization(null);
        }
    }
}
