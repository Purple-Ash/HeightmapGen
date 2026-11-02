using System;
using System.Collections.Generic;
using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.InputSystem;
using UnityEngine.UI;

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
    public bool KeyboardShortcutsEnabled { get; set; } = true;

    private static readonly Key[] NumberKeys =
    {
        Key.Digit1, Key.Digit2, Key.Digit3, Key.Digit4, Key.Digit5,
        Key.Digit6, Key.Digit7, Key.Digit8, Key.Digit9, Key.Digit0
    };
    private static readonly Key[] NumpadKeys =
    {
        Key.Numpad1, Key.Numpad2, Key.Numpad3, Key.Numpad4, Key.Numpad5,
        Key.Numpad6, Key.Numpad7, Key.Numpad8, Key.Numpad9, Key.Numpad0
    };
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

    private void Update()
    {
        Keyboard keyboard = Keyboard.current;
        if (!KeyboardShortcutsEnabled || keyboard == null) return;
        if (keyboard.ctrlKey.isPressed || keyboard.altKey.isPressed
            || keyboard.leftMetaKey.isPressed || keyboard.rightMetaKey.isPressed) return;

        // Number entry in the settings panel must not change the visualization.
        GameObject selected = EventSystem.current != null ? EventSystem.current.currentSelectedGameObject : null;
        InputField input = selected != null ? selected.GetComponentInParent<InputField>() : null;
        if (input != null && input.isFocused) return;

        int index = 0;
        foreach (TerrainVisualization visualization in visualizations)
        {
            // The menu skips empty list entries, so keyboard selection must do the same.
            if (visualization == null) continue;
            if (index >= NumberKeys.Length) break;
            if (keyboard[NumberKeys[index]].wasPressedThisFrame || keyboard[NumpadKeys[index]].wasPressedThisFrame)
            {
                SelectVisualization(visualization);
                return;
            }
            index++;
        }
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
