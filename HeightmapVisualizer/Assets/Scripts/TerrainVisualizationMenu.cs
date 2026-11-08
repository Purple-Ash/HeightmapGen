using System.Collections.Generic;
using UnityEngine;
using UnityEngine.UI;

[DefaultExecutionOrder(-40)]
[DisallowMultipleComponent]
[RequireComponent(typeof(TerrainVisualizationController))]
[AddComponentMenu("Terrain/Visualization Menu")]
public sealed class TerrainVisualizationMenu : MonoBehaviour
{
    [Tooltip("Scale of the 100 x 150 icons. 1 = original size, 0.5 = half size.")]
    [Min(0.1f)][SerializeField] private float iconScale = 1f;
    [SerializeField] private Vector2 margin = new Vector2(16f, 16f);
    [SerializeField] private Color panelColor = new Color(0.055f, 0.075f, 0.105f, 0.94f);
    [SerializeField] private Color selectedColor = new Color(0.18f, 0.65f, 0.95f, 1f);

    private Vector2 IconSize => new Vector2(100f, 150f) * Mathf.Max(0.1f, iconScale);
    private float CardWidth => IconSize.x + 8f;
    private float CardHeight => IconSize.y + 32f;
    private const float Gap = 8f;
    private readonly List<(TerrainVisualization preset, Image background, RectTransform icon, RectTransform label)> cards = new List<(TerrainVisualization, Image, RectTransform, RectTransform)>();
    private TerrainVisualizationController controller;
    private GameObject canvasObject;
    private RectTransform panel;
    private RectTransform content;
    private float appliedIconScale = -1f;
    private float contentWidth;
    private Font font;
    private Text heading;

    private void OnEnable()
    {
        controller = GetComponent<TerrainVisualizationController>();
        controller.SelectionChanged += RefreshSelection;
        BuildMenu();
    }

    private void BuildMenu()
    {
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        canvasObject = new GameObject("Visualization Canvas", typeof(RectTransform), typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
        canvasObject.transform.SetParent(transform, false);
        Canvas canvas = canvasObject.GetComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 5;

        panel = CreateRect("Visualization Panel", canvasObject.transform);
        panel.anchorMin = panel.anchorMax = panel.pivot = Vector2.zero;
        panel.gameObject.AddComponent<Image>().color = panelColor;

        heading = CreateText("Title", panel, "Visualisation", 16);
        heading.rectTransform.anchorMin = new Vector2(0f, 1f);
        heading.rectTransform.anchorMax = Vector2.one;
        heading.rectTransform.pivot = new Vector2(0f, 1f);
        heading.rectTransform.offsetMin = new Vector2(12f, -32f);
        heading.rectTransform.offsetMax = new Vector2(-12f, -8f);

        RectTransform viewport = CreateRect("Presets", panel);
        viewport.anchorMin = Vector2.zero;
        viewport.anchorMax = Vector2.one;
        viewport.offsetMin = new Vector2(12f, 12f);
        viewport.offsetMax = new Vector2(-12f, -40f);
        viewport.gameObject.AddComponent<RectMask2D>();
        // Catch drags in the gaps as well as on cards.
        viewport.gameObject.AddComponent<Image>().color = Color.clear;
        ScrollRect scroll = viewport.gameObject.AddComponent<ScrollRect>();
        scroll.horizontal = true;
        scroll.vertical = false;
        scroll.movementType = ScrollRect.MovementType.Clamped;
        scroll.scrollSensitivity = 35f;
        scroll.viewport = viewport;

        content = CreateRect("Content", viewport);
        content.anchorMin = content.anchorMax = new Vector2(0f, 1f);
        content.pivot = new Vector2(0f, 1f);
        scroll.content = content;

        foreach (TerrainVisualization preset in controller.Visualizations)
        {
            if (preset == null) continue;
            CreateCard(content, preset, cards.Count);
        }
        appliedIconScale = -1f;
        panel.gameObject.SetActive(cards.Count > 0);
        ResizePanel();
        RefreshSelection();
    }

    private void CreateCard(RectTransform content, TerrainVisualization preset, int index)
    {
        RectTransform card = CreateRect(preset.DisplayName, content);
        card.anchorMin = card.anchorMax = card.pivot = new Vector2(0f, 1f);
        card.anchoredPosition = new Vector2(index * (CardWidth + Gap), 0f);
        card.sizeDelta = new Vector2(CardWidth, CardHeight);
        Image background = card.gameObject.AddComponent<Image>();
        Button button = card.gameObject.AddComponent<Button>();
        button.targetGraphic = background;
        button.onClick.AddListener(() => controller.SelectVisualization(preset));

        RectTransform iconRect = CreateRect("Icon", card);
        iconRect.anchorMin = iconRect.anchorMax = iconRect.pivot = new Vector2(0f, 1f);
        iconRect.anchoredPosition = new Vector2(4f, -4f);
        iconRect.sizeDelta = IconSize;
        Image icon = iconRect.gameObject.AddComponent<Image>();
        icon.sprite = preset.Icon;
        icon.preserveAspect = true;
        icon.color = preset.Icon != null ? Color.white : new Color(0.12f, 0.16f, 0.22f, 1f);
        icon.raycastTarget = false;

        string labelText = index < 10 ? $"{(index + 1) % 10}: {preset.DisplayName}" : preset.DisplayName;
        Text label = CreateText("Name", card, labelText, 12);
        label.alignment = TextAnchor.MiddleCenter;
        label.rectTransform.anchorMin = label.rectTransform.anchorMax = label.rectTransform.pivot = Vector2.zero;
        label.rectTransform.anchoredPosition = new Vector2(4f, 4f);
        label.rectTransform.sizeDelta = new Vector2(IconSize.x, 20f);
        cards.Add((preset, background, iconRect, label.rectTransform));
    }

    private void Update()
    {
        ResizePanel();
    }

    private void ResizePanel()
    {
        if (panel == null) return;
        float scale = Mathf.Max(0.1f, iconScale);
        if (!Mathf.Approximately(appliedIconScale, scale))
        {
            appliedIconScale = scale;
            for (int i = 0; i < cards.Count; i++)
            {
                var card = cards[i];
                card.background.rectTransform.anchoredPosition = new Vector2(i * (CardWidth + Gap), 0f);
                card.background.rectTransform.sizeDelta = new Vector2(CardWidth, CardHeight);
                card.icon.sizeDelta = IconSize;
                card.label.sizeDelta = new Vector2(IconSize.x, 20f);
            }
            contentWidth = Mathf.Max(0f, cards.Count * (CardWidth + Gap) - Gap);
            content.sizeDelta = new Vector2(contentWidth, CardHeight);
        }
        float desiredWidth = Mathf.Max(contentWidth, heading != null ? heading.preferredWidth : 0f) + 24f;
        float width = Mathf.Min(desiredWidth, Mathf.Max(24f, Screen.width - margin.x * 2f));
        panel.anchoredPosition = margin;
        panel.sizeDelta = new Vector2(width, CardHeight + 52f);
    }

    private void RefreshSelection()
    {
        if (heading != null)
        {
            string displayName = controller.SelectedVisualization != null ? controller.SelectedVisualization.DisplayName : "None";
            heading.text = $"Visualisation: {displayName}";
            ResizePanel();
        }
        foreach (var card in cards)
        {
            card.background.color = card.preset == controller.SelectedVisualization
                ? selectedColor : new Color(0.16f, 0.20f, 0.26f, 1f);
        }
    }

    private static RectTransform CreateRect(string name, Transform parent)
    {
        RectTransform rect = new GameObject(name, typeof(RectTransform)).GetComponent<RectTransform>();
        rect.SetParent(parent, false);
        return rect;
    }

    private Text CreateText(string name, Transform parent, string value, int size)
    {
        Text text = CreateRect(name, parent).gameObject.AddComponent<Text>();
        text.font = font;
        text.fontSize = size;
        text.text = value;
        text.color = Color.white;
        text.alignment = TextAnchor.MiddleLeft;
        text.horizontalOverflow = HorizontalWrapMode.Wrap;
        text.verticalOverflow = VerticalWrapMode.Truncate;
        text.raycastTarget = false;
        return text;
    }

    private void OnDisable()
    {
        if (controller != null) controller.SelectionChanged -= RefreshSelection;
        if (canvasObject != null)
        {
            canvasObject.SetActive(false);
            Destroy(canvasObject);
        }
        cards.Clear();
        panel = null;
        heading = null;
    }
}
