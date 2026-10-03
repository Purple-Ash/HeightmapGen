using System;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using UnityEngine;
using UnityEngine.UI;

/// <summary>
/// Builds the runtime terrain settings panel from the serializable fields on TerrainGen.
/// The panel is installed automatically after a scene loads, so no prefab or scene setup is needed.
/// </summary>
[DefaultExecutionOrder(-100)]
public sealed class TerrainRuntimeSettingsUI : MonoBehaviour
{
    private const float PanelWidth = 390f;
    private const float PanelMargin = 16f;
    private const float PanelPadding = 12f;
    private const float HeaderHeight = 25f;
    private const float HintHeight = 18f;
    private const float ApplyHeight = 36f;
    private const float RowHeight = 34f;
    private const float SectionHeight = 24f;
    private const float RowSpacing = 6f;

    private static readonly Color PanelColor = new Color(0.055f, 0.075f, 0.105f, 0.94f);
    private static readonly Color ViewportColor = new Color(0.02f, 0.03f, 0.045f, 0.68f);
    private static readonly Color FieldColor = new Color(0.12f, 0.16f, 0.22f, 1f);
    private static readonly Color AccentColor = new Color(0.18f, 0.48f, 0.78f, 1f);
    private static readonly Color TextColor = new Color(0.92f, 0.95f, 1f, 1f);
    private static readonly Color MutedTextColor = new Color(0.67f, 0.72f, 0.8f, 1f);

    private readonly List<Action> pendingInputCommits = new List<Action>();

    private TerrainGen terrain;
    private Font font;
    private Canvas rootCanvas;
    private RectTransform panel;
    private RectTransform content;
    private LayoutElement viewportLayout;
    private ScrollRect scrollRect;
    private Button applyButton;
    private Text applyButtonText;
    private GameObject enumDropdown;
    private GameObject enumDropdownBlocker;
    private Button statisticsToggle;
    private RectTransform statisticsPanel;
    private Text statisticsText;
    private int screenWidth;
    private int screenHeight;
    private bool wasGenerating;

    [RuntimeInitializeOnLoadMethod(RuntimeInitializeLoadType.AfterSceneLoad)]
    private static void CreateForLoadedScene()
    {
        if (FindFirstObjectByType<TerrainRuntimeSettingsUI>() != null ||
            FindFirstObjectByType<TerrainGen>() == null)
        {
            return;
        }

        new GameObject("Terrain Runtime Settings").AddComponent<TerrainRuntimeSettingsUI>();
    }

    private void Awake()
    {
        terrain = FindFirstObjectByType<TerrainGen>();
        if (terrain == null)
        {
            enabled = false;
            return;
        }

        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        BuildPanel();
        RefreshPanelSize();
    }

    private void Update()
    {
        if (screenWidth != Screen.width || screenHeight != Screen.height)
        {
            RefreshPanelSize();
        }

        bool isGenerating = TerrainGen.IsGenerating;
        if (isGenerating != wasGenerating)
        {
            wasGenerating = isGenerating;
            applyButton.interactable = !isGenerating;
            applyButtonText.text = isGenerating ? "Generating..." : "Apply & Regenerate";
        }

        if (statisticsText != null)
        {
            UpdateStatisticsText();
        }
    }

    private void BuildPanel()
    {
        Canvas canvas = FindFirstObjectByType<Canvas>();
        if (canvas == null)
        {
            GameObject canvasObject = CreateObject("Runtime UI Canvas", null);
            canvas = canvasObject.AddComponent<Canvas>();
            canvas.renderMode = RenderMode.ScreenSpaceOverlay;
            canvasObject.AddComponent<CanvasScaler>();
            canvasObject.AddComponent<GraphicRaycaster>();
        }

        rootCanvas = canvas.rootCanvas;
        if (rootCanvas.GetComponent<GraphicRaycaster>() == null)
        {
            rootCanvas.gameObject.AddComponent<GraphicRaycaster>();
        }

        panel = CreateObject("Terrain Settings Panel", canvas.transform).GetComponent<RectTransform>();
        panel.anchorMin = Vector2.one;
        panel.anchorMax = Vector2.one;
        panel.pivot = Vector2.one;
        panel.anchoredPosition = new Vector2(-PanelMargin, -PanelMargin);
        panel.SetSizeWithCurrentAnchors(RectTransform.Axis.Horizontal, PanelWidth);

        Image panelImage = panel.gameObject.AddComponent<Image>();
        panelImage.color = PanelColor;

        VerticalLayoutGroup panelLayout = panel.gameObject.AddComponent<VerticalLayoutGroup>();
        panelLayout.padding = new RectOffset(
            Mathf.RoundToInt(PanelPadding),
            Mathf.RoundToInt(PanelPadding),
            Mathf.RoundToInt(PanelPadding),
            Mathf.RoundToInt(PanelPadding));
        panelLayout.spacing = RowSpacing;
        panelLayout.childControlWidth = true;
        panelLayout.childControlHeight = true;
        panelLayout.childForceExpandWidth = true;
        panelLayout.childForceExpandHeight = false;

        Text title = CreateText("Title", panel, "Terrain Settings", 18, TextColor, TextAnchor.MiddleLeft);
        SetPreferredHeight(title.rectTransform, HeaderHeight);

        Text hint = CreateText(
            "Hint",
            panel,
            "Press Esc to use the menu. Click terrain to resume camera control.",
            11,
            MutedTextColor,
            TextAnchor.MiddleLeft);
        SetPreferredHeight(hint.rectTransform, HintHeight);

        BuildScrollView(panel);
        BuildFieldControls();

        applyButton = CreateButton("Apply", panel, "Apply & Regenerate", AccentColor, ApplyChanges);
        SetPreferredHeight(applyButton.GetComponent<RectTransform>(), ApplyHeight);
        applyButtonText = applyButton.GetComponentInChildren<Text>();
        wasGenerating = TerrainGen.IsGenerating;
        applyButton.interactable = !wasGenerating;
        if (wasGenerating)
        {
            applyButtonText.text = "Generating...";
        }

        BuildStatisticsOverlay();
    }

    private void BuildScrollView(Transform parent)
    {
        GameObject scrollObject = CreateObject("Settings Scroll View", parent);
        Image scrollBackground = scrollObject.AddComponent<Image>();
        scrollBackground.color = ViewportColor;
        viewportLayout = scrollObject.AddComponent<LayoutElement>();

        scrollRect = scrollObject.AddComponent<ScrollRect>();
        scrollRect.horizontal = false;
        scrollRect.movementType = ScrollRect.MovementType.Clamped;
        scrollRect.scrollSensitivity = 28f;

        RectTransform viewport = CreateObject("Viewport", scrollObject.transform).GetComponent<RectTransform>();
        Stretch(viewport, Vector2.zero, Vector2.zero);
        Image viewportImage = viewport.gameObject.AddComponent<Image>();
        viewportImage.color = Color.white;
        Mask mask = viewport.gameObject.AddComponent<Mask>();
        mask.showMaskGraphic = false;

        content = CreateObject("Content", viewport).GetComponent<RectTransform>();
        content.anchorMin = new Vector2(0f, 1f);
        content.anchorMax = new Vector2(1f, 1f);
        content.pivot = new Vector2(0.5f, 1f);
        content.anchoredPosition = Vector2.zero;
        content.sizeDelta = Vector2.zero;

        VerticalLayoutGroup contentLayout = content.gameObject.AddComponent<VerticalLayoutGroup>();
        contentLayout.padding = new RectOffset(8, 8, 8, 8);
        contentLayout.spacing = RowSpacing;
        contentLayout.childControlWidth = true;
        contentLayout.childControlHeight = true;
        contentLayout.childForceExpandWidth = true;
        contentLayout.childForceExpandHeight = false;

        ContentSizeFitter contentSize = content.gameObject.AddComponent<ContentSizeFitter>();
        contentSize.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        contentSize.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

        scrollRect.viewport = viewport;
        scrollRect.content = content;
        CreateScrollbar(scrollObject.transform);
    }

    private void CreateScrollbar(Transform parent)
    {
        GameObject scrollbarObject = CreateObject("Scrollbar", parent);
        RectTransform scrollbarRect = scrollbarObject.GetComponent<RectTransform>();
        scrollbarRect.anchorMin = new Vector2(1f, 0f);
        scrollbarRect.anchorMax = new Vector2(1f, 1f);
        scrollbarRect.pivot = new Vector2(1f, 1f);
        scrollbarRect.anchoredPosition = new Vector2(-2f, -2f);
        scrollbarRect.sizeDelta = new Vector2(10f, -4f);

        Image scrollbarBackground = scrollbarObject.AddComponent<Image>();
        scrollbarBackground.color = new Color(0f, 0f, 0f, 0.32f);
        Scrollbar scrollbar = scrollbarObject.AddComponent<Scrollbar>();
        scrollbar.direction = Scrollbar.Direction.BottomToTop;

        RectTransform slidingArea = CreateObject("Sliding Area", scrollbarObject.transform).GetComponent<RectTransform>();
        Stretch(slidingArea, Vector2.zero, Vector2.zero);
        slidingArea.offsetMin = new Vector2(1f, 2f);
        slidingArea.offsetMax = new Vector2(-1f, -2f);

        RectTransform handle = CreateObject("Handle", slidingArea).GetComponent<RectTransform>();
        Stretch(handle, Vector2.zero, Vector2.zero);
        Image handleImage = handle.gameObject.AddComponent<Image>();
        handleImage.color = AccentColor;
        scrollbar.targetGraphic = handleImage;
        scrollbar.handleRect = handle;

        scrollRect.verticalScrollbar = scrollbar;
        scrollRect.verticalScrollbarVisibility = ScrollRect.ScrollbarVisibility.AutoHideAndExpandViewport;
        scrollRect.verticalScrollbarSpacing = 2f;
    }

    private void BuildFieldControls()
    {
        FieldInfo[] fields = typeof(TerrainGen).GetFields(
            BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
        Array.Sort(fields, (left, right) => left.MetadataToken.CompareTo(right.MetadataToken));

        string currentHeader = null;
        string pendingHeader = null;
        foreach (FieldInfo field in fields)
        {
            HeaderAttribute header = field.GetCustomAttribute<HeaderAttribute>();
            if (header != null)
            {
                pendingHeader = header.header;
            }

            if (!IsRuntimeEditable(field))
            {
                continue;
            }

            if (pendingHeader != null && pendingHeader != currentHeader)
            {
                currentHeader = pendingHeader;
                Text section = CreateText("Section", content, currentHeader, 14, AccentColor, TextAnchor.LowerLeft);
                SetPreferredHeight(section.rectTransform, SectionHeight);
            }

            AddControlForField(field);
        }
    }

    private static bool IsRuntimeEditable(FieldInfo field)
    {
        if (field.IsStatic || field.IsNotSerialized || field.GetCustomAttribute<HideInInspector>() != null)
        {
            return false;
        }

        bool serialized = field.IsPublic || field.GetCustomAttribute<SerializeField>() != null;
        if (!serialized || typeof(UnityEngine.Object).IsAssignableFrom(field.FieldType))
        {
            return false;
        }

        Type type = field.FieldType;
        return type.IsEnum || type == typeof(bool) || type == typeof(string) || IsNumeric(type) ||
               type == typeof(Vector2Int) || type == typeof(Vector3Int) ||
               type == typeof(Vector2) || type == typeof(Vector3) || type == typeof(Vector4) ||
               type == typeof(Color);
    }

    private void AddControlForField(FieldInfo field)
    {
        Type type = field.FieldType;
        if (type == typeof(bool))
        {
            AddBooleanControl(field);
        }
        else if (type.IsEnum)
        {
            AddEnumControl(field);
        }
        else if (type == typeof(Vector2Int))
        {
            AddVectorControl(field, new[] { "X", "Y" }, true);
        }
        else if (type == typeof(Vector3Int))
        {
            AddVectorControl(field, new[] { "X", "Y", "Z" }, true);
        }
        else if (type == typeof(Vector2))
        {
            AddVectorControl(field, new[] { "X", "Y" }, false);
        }
        else if (type == typeof(Vector3))
        {
            AddVectorControl(field, new[] { "X", "Y", "Z" }, false);
        }
        else if (type == typeof(Vector4))
        {
            AddVectorControl(field, new[] { "X", "Y", "Z", "W" }, false);
        }
        else if (type == typeof(Color))
        {
            AddVectorControl(field, new[] { "R", "G", "B", "A" }, false);
        }
        else
        {
            AddTextControl(field);
        }
    }

    private void AddBooleanControl(FieldInfo field)
    {
        RectTransform row = CreateRow(field);
        bool initialValue = (bool)field.GetValue(terrain);
        Button button = CreateButton("Value", row, initialValue ? "On" : "Off", FieldColor, null);
        SetFlexibleWidth(button.GetComponent<RectTransform>(), 1f);
        Text valueText = button.GetComponentInChildren<Text>();

        button.onClick.AddListener(() =>
        {
            bool value = !(bool)field.GetValue(terrain);
            field.SetValue(terrain, value);
            valueText.text = value ? "On" : "Off";
        });
    }

    private void AddEnumControl(FieldInfo field)
    {
        RectTransform row = CreateRow(field);
        Button button = CreateButton("Value", row, field.GetValue(terrain).ToString(), FieldColor, null);
        SetFlexibleWidth(button.GetComponent<RectTransform>(), 1f);
        Text valueText = button.GetComponentInChildren<Text>();

        button.onClick.AddListener(() => ShowEnumDropdown(field, button.GetComponent<RectTransform>(), valueText));
    }

    private void AddTextControl(FieldInfo field)
    {
        RectTransform row = CreateRow(field);
        InputField input = CreateInputField(row, FormatValue(field.GetValue(terrain)), "Value");
        SetFlexibleWidth(input.GetComponent<RectTransform>(), 1f);

        Action commit = () =>
        {
            if (TryParseValue(input.text, field.FieldType, out object parsedValue))
            {
                object constrainedValue = ApplyNumericConstraints(field, parsedValue);
                field.SetValue(terrain, constrainedValue);
                input.text = FormatValue(constrainedValue);
            }
            else
            {
                input.text = FormatValue(field.GetValue(terrain));
            }
        };

        input.onEndEdit.AddListener(_ => commit());
        pendingInputCommits.Add(commit);
    }

    private void AddVectorControl(FieldInfo field, string[] labels, bool integerValues)
    {
        RectTransform row = CreateRow(field);
        GameObject valuesObject = CreateObject("Values", row);
        HorizontalLayoutGroup valuesLayout = valuesObject.AddComponent<HorizontalLayoutGroup>();
        valuesLayout.spacing = 4f;
        valuesLayout.childControlWidth = true;
        valuesLayout.childControlHeight = true;
        valuesLayout.childForceExpandWidth = true;
        valuesLayout.childForceExpandHeight = true;
        SetFlexibleWidth(valuesObject.GetComponent<RectTransform>(), 1f);

        float[] initialValues = GetVectorValues(field.GetValue(terrain));
        InputField[] inputs = new InputField[labels.Length];
        for (int i = 0; i < labels.Length; i++)
        {
            inputs[i] = CreateInputField(valuesObject.transform, FormatFloat(initialValues[i]), labels[i]);
            SetFlexibleWidth(inputs[i].GetComponent<RectTransform>(), 1f);
        }

        Action commit = () =>
        {
            float[] values = new float[inputs.Length];
            for (int i = 0; i < inputs.Length; i++)
            {
                if (!TryParseFloat(inputs[i].text, out values[i]))
                {
                    values = GetVectorValues(field.GetValue(terrain));
                    for (int j = 0; j < inputs.Length; j++)
                    {
                        inputs[j].text = integerValues
                            ? Mathf.RoundToInt(values[j]).ToString(CultureInfo.InvariantCulture)
                            : FormatFloat(values[j]);
                    }
                    return;
                }
            }

            object vectorValue = CreateVectorValue(field.FieldType, values, integerValues);
            field.SetValue(terrain, vectorValue);
        };

        foreach (InputField input in inputs)
        {
            input.onEndEdit.AddListener(_ => commit());
        }
        pendingInputCommits.Add(commit);
    }

    private RectTransform CreateRow(FieldInfo field)
    {
        RectTransform row = CreateObject(field.Name, content).GetComponent<RectTransform>();
        HorizontalLayoutGroup rowLayout = row.gameObject.AddComponent<HorizontalLayoutGroup>();
        rowLayout.spacing = 8f;
        rowLayout.childControlWidth = true;
        rowLayout.childControlHeight = true;
        rowLayout.childForceExpandWidth = false;
        rowLayout.childForceExpandHeight = true;
        SetPreferredHeight(row, RowHeight);

        Text label = CreateText("Label", row, FormatFieldName(field.Name), 13, TextColor, TextAnchor.MiddleLeft);
        LayoutElement labelLayout = label.gameObject.AddComponent<LayoutElement>();
        labelLayout.preferredWidth = 155f;
        labelLayout.minWidth = 120f;
        return row;
    }

    private void ShowEnumDropdown(FieldInfo field, RectTransform control, Text caption)
    {
        CloseEnumDropdown();

        Array values = Enum.GetValues(field.FieldType);
        enumDropdownBlocker = CreateClickBlocker("Generator Dropdown Blocker", CloseEnumDropdown);
        enumDropdown = CreateObject("Generator Dropdown", rootCanvas.transform);
        enumDropdown.transform.SetAsLastSibling();

        Image background = enumDropdown.AddComponent<Image>();
        background.color = new Color(0.08f, 0.11f, 0.16f, 1f);
        VerticalLayoutGroup layout = enumDropdown.AddComponent<VerticalLayoutGroup>();
        layout.padding = new RectOffset(4, 4, 4, 4);
        layout.spacing = 3f;
        layout.childControlWidth = true;
        layout.childControlHeight = true;
        layout.childForceExpandWidth = true;
        layout.childForceExpandHeight = true;

        RectTransform dropdownRect = enumDropdown.GetComponent<RectTransform>();
        float dropdownHeight = values.Length * RowHeight + (values.Length - 1) * layout.spacing + layout.padding.top + layout.padding.bottom;
        PositionPopup(dropdownRect, control, dropdownHeight);

        foreach (object value in values)
        {
            object selectedValue = value;
            Button option = CreateButton("Option", enumDropdown.transform, selectedValue.ToString(), FieldColor, () =>
            {
                field.SetValue(terrain, selectedValue);
                caption.text = selectedValue.ToString();
                CloseEnumDropdown();
            });
            SetPreferredHeight(option.GetComponent<RectTransform>(), RowHeight);
        }
    }

    private void CloseEnumDropdown()
    {
        if (enumDropdown != null)
        {
            Destroy(enumDropdown);
            enumDropdown = null;
        }

        if (enumDropdownBlocker != null)
        {
            Destroy(enumDropdownBlocker);
            enumDropdownBlocker = null;
        }
    }

    private void BuildStatisticsOverlay()
    {
        statisticsToggle = CreateButton("Statistics Toggle", rootCanvas.transform, string.Empty, FieldColor, ShowStatistics);
        RectTransform toggleRect = statisticsToggle.GetComponent<RectTransform>();
        toggleRect.anchorMin = new Vector2(0f, 1f);
        toggleRect.anchorMax = new Vector2(0f, 1f);
        toggleRect.pivot = new Vector2(0f, 1f);
        toggleRect.anchoredPosition = new Vector2(PanelMargin, -PanelMargin);
        toggleRect.sizeDelta = new Vector2(38f, 36f);
        CreateStatisticsIcon(statisticsToggle);

        statisticsPanel = CreateObject("Statistics Panel", rootCanvas.transform).GetComponent<RectTransform>();
        statisticsPanel.anchorMin = new Vector2(0f, 1f);
        statisticsPanel.anchorMax = new Vector2(0f, 1f);
        statisticsPanel.pivot = new Vector2(0f, 1f);
        statisticsPanel.anchoredPosition = new Vector2(PanelMargin, -PanelMargin);
        statisticsPanel.sizeDelta = new Vector2(300f, 118f);

        Image background = statisticsPanel.gameObject.AddComponent<Image>();
        background.color = PanelColor;
        VerticalLayoutGroup layout = statisticsPanel.gameObject.AddComponent<VerticalLayoutGroup>();
        layout.padding = new RectOffset(12, 12, 10, 10);
        layout.spacing = 6f;
        layout.childControlWidth = true;
        layout.childControlHeight = true;
        layout.childForceExpandWidth = true;
        layout.childForceExpandHeight = true;

        RectTransform heading = CreateObject("Heading", statisticsPanel).GetComponent<RectTransform>();
        HorizontalLayoutGroup headingLayout = heading.gameObject.AddComponent<HorizontalLayoutGroup>();
        headingLayout.spacing = 8f;
        headingLayout.childControlWidth = true;
        headingLayout.childControlHeight = true;
        headingLayout.childForceExpandWidth = false;
        headingLayout.childForceExpandHeight = true;
        SetPreferredHeight(heading, HeaderHeight);

        Text title = CreateText("Title", heading, "Terrain Statistics", 16, TextColor, TextAnchor.MiddleLeft);
        SetFlexibleWidth(title.rectTransform, 1f);
        Button hideButton = CreateButton("Hide", heading, "Hide", FieldColor, HideStatistics);
        LayoutElement hideLayout = hideButton.gameObject.AddComponent<LayoutElement>();
        hideLayout.preferredWidth = 54f;
        hideLayout.minWidth = 54f;

        statisticsText = CreateText("Values", statisticsPanel, string.Empty, 14, TextColor, TextAnchor.UpperLeft);
        statisticsText.verticalOverflow = VerticalWrapMode.Overflow;
        SetPreferredHeight(statisticsText.rectTransform, 60f);
        UpdateStatisticsText();
        statisticsToggle.gameObject.SetActive(false);
    }

    private void ShowStatistics()
    {
        statisticsPanel.gameObject.SetActive(true);
        statisticsToggle.gameObject.SetActive(false);
        UpdateStatisticsText();
    }

    private void HideStatistics()
    {
        statisticsPanel.gameObject.SetActive(false);
        statisticsToggle.gameObject.SetActive(true);
    }

    private void UpdateStatisticsText()
    {
        if (!terrain.HasHeightStatistics)
        {
            statisticsText.text = "Highest point:\nLowest point:";
            return;
        }

        statisticsText.text = "Highest point: " + terrain.HighestHeight.ToString("F2", CultureInfo.InvariantCulture) +
                              "\nLowest point: " + terrain.LowestHeight.ToString("F2", CultureInfo.InvariantCulture);
    }

    private GameObject CreateClickBlocker(string name, UnityEngine.Events.UnityAction onClick)
    {
        GameObject blocker = CreateObject(name, rootCanvas.transform);
        RectTransform blockerRect = blocker.GetComponent<RectTransform>();
        Stretch(blockerRect, Vector2.zero, Vector2.zero);
        Image image = blocker.AddComponent<Image>();
        image.color = new Color(0f, 0f, 0f, 0f);
        Button button = blocker.AddComponent<Button>();
        button.targetGraphic = image;
        button.onClick.AddListener(onClick);
        return blocker;
    }

    private void PositionPopup(RectTransform popup, RectTransform control, float height)
    {
        RectTransform canvasRect = rootCanvas.transform as RectTransform;
        Camera eventCamera = rootCanvas.renderMode == RenderMode.ScreenSpaceOverlay ? null : rootCanvas.worldCamera;
        Vector3[] controlCorners = new Vector3[4];
        control.GetWorldCorners(controlCorners);

        RectTransformUtility.ScreenPointToLocalPointInRectangle(
            canvasRect,
            RectTransformUtility.WorldToScreenPoint(eventCamera, controlCorners[0]),
            eventCamera,
            out Vector2 lowerLeft);
        RectTransformUtility.ScreenPointToLocalPointInRectangle(
            canvasRect,
            RectTransformUtility.WorldToScreenPoint(eventCamera, controlCorners[1]),
            eventCamera,
            out Vector2 upperLeft);
        RectTransformUtility.ScreenPointToLocalPointInRectangle(
            canvasRect,
            RectTransformUtility.WorldToScreenPoint(eventCamera, controlCorners[3]),
            eventCamera,
            out Vector2 lowerRight);

        const float edgePadding = 4f;
        float width = lowerRight.x - lowerLeft.x;
        popup.anchorMin = new Vector2(0.5f, 0.5f);
        popup.anchorMax = new Vector2(0.5f, 0.5f);
        popup.sizeDelta = new Vector2(width, height);

        float minimumX = canvasRect.rect.xMin + edgePadding;
        float maximumX = canvasRect.rect.xMax - width - edgePadding;
        float x = maximumX < minimumX ? minimumX : Mathf.Clamp(lowerLeft.x, minimumX, maximumX);
        bool openAbove = lowerLeft.y - height < canvasRect.rect.yMin + edgePadding;

        popup.pivot = openAbove ? new Vector2(0f, 0f) : new Vector2(0f, 1f);
        popup.anchoredPosition = new Vector2(x, openAbove ? upperLeft.y : lowerLeft.y);
    }

    private void ApplyChanges()
    {
        foreach (Action commit in pendingInputCommits)
        {
            commit();
        }

        terrain.Regenerate();
    }

    private void RefreshPanelSize()
    {
        screenWidth = Screen.width;
        screenHeight = Screen.height;

        Canvas.ForceUpdateCanvases();
        LayoutRebuilder.ForceRebuildLayoutImmediate(content);
        float desiredContentHeight = LayoutUtility.GetPreferredHeight(content);
        float maximumPanelHeight = Screen.height * (2f / 3f);
        float panelOverhead = PanelPadding * 2f + HeaderHeight + HintHeight + ApplyHeight + RowSpacing * 3f;
        float maximumViewportHeight = Mathf.Max(20f, maximumPanelHeight - panelOverhead);
        float viewportHeight = Mathf.Min(desiredContentHeight, maximumViewportHeight);

        viewportLayout.preferredHeight = viewportHeight;
        panel.SetSizeWithCurrentAnchors(RectTransform.Axis.Vertical, panelOverhead + viewportHeight);
        Canvas.ForceUpdateCanvases();
        scrollRect.verticalNormalizedPosition = 1f;
    }

    private static GameObject CreateObject(string name, Transform parent)
    {
        GameObject result = new GameObject(name, typeof(RectTransform));
        if (parent != null)
        {
            result.transform.SetParent(parent, false);
        }
        return result;
    }

    private Text CreateText(string name, Transform parent, string value, int size, Color color, TextAnchor alignment)
    {
        Text text = CreateObject(name, parent).AddComponent<Text>();
        text.font = font;
        text.text = value;
        text.fontSize = size;
        text.color = color;
        text.alignment = alignment;
        text.supportRichText = false;
        text.horizontalOverflow = HorizontalWrapMode.Overflow;
        text.verticalOverflow = VerticalWrapMode.Truncate;
        text.raycastTarget = false;
        return text;
    }

    private InputField CreateInputField(Transform parent, string value, string placeholder)
    {
        GameObject inputObject = CreateObject("Input", parent);
        Image background = inputObject.AddComponent<Image>();
        background.color = FieldColor;
        InputField input = inputObject.AddComponent<InputField>();
        input.targetGraphic = background;

        Text text = CreateText("Text", inputObject.transform, value, 13, TextColor, TextAnchor.MiddleLeft);
        Stretch(text.rectTransform, new Vector2(8f, 4f), new Vector2(-8f, -4f));
        text.verticalOverflow = VerticalWrapMode.Overflow;
        input.textComponent = text;

        Text placeholderText = CreateText("Placeholder", inputObject.transform, placeholder, 12, MutedTextColor, TextAnchor.MiddleLeft);
        Stretch(placeholderText.rectTransform, new Vector2(8f, 4f), new Vector2(-8f, -4f));
        placeholderText.verticalOverflow = VerticalWrapMode.Overflow;
        input.placeholder = placeholderText;
        input.SetTextWithoutNotify(value);
        input.ForceLabelUpdate();
        return input;
    }

    private Button CreateButton(string name, Transform parent, string value, Color color, UnityEngine.Events.UnityAction action)
    {
        GameObject buttonObject = CreateObject(name, parent);
        Image background = buttonObject.AddComponent<Image>();
        background.color = color;
        Button button = buttonObject.AddComponent<Button>();
        button.targetGraphic = background;
        if (action != null)
        {
            button.onClick.AddListener(action);
        }

        Text text = CreateText("Text", buttonObject.transform, value, 13, TextColor, TextAnchor.MiddleCenter);
        Stretch(text.rectTransform, Vector2.zero, Vector2.zero);
        return button;
    }

    private static void CreateStatisticsIcon(Button button)
    {
        Sprite icon = LoadStatisticsIcon();
        if (icon == null)
        {
            Debug.LogWarning(
                "Could not load statistics icon. Expected Assets/Sprites/Statistics.png " +
                "(editor) or Resources/Sprites/Statistics (builds). Falling back to text label.");
            Text label = button.GetComponentInChildren<Text>();
            if (label != null)
            {
                label.text = "Stats";
            }
            return;
        }

        RectTransform iconRect = CreateObject("Statistics Icon", button.transform).GetComponent<RectTransform>();
        Stretch(iconRect, new Vector2(7f, 7f), new Vector2(-7f, -7f));
        Image iconImage = iconRect.gameObject.AddComponent<Image>();
        iconImage.sprite = icon;
        iconImage.preserveAspect = true;
        iconImage.raycastTarget = false;
    }

    private static Sprite LoadStatisticsIcon()
    {
#if UNITY_EDITOR
        // Works in the editor / play mode straight from Assets/Sprites/Statistics.png.
        const string assetPath = "Assets/Sprites/Statistics.png";
        Sprite editorSprite = UnityEditor.AssetDatabase.LoadAssetAtPath<Sprite>(assetPath);
        if (editorSprite != null)
        {
            return editorSprite;
        }

        // Texture not imported as "Sprite (2D and UI)" - wrap it in a sprite instead.
        Texture2D editorTexture = UnityEditor.AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
        if (editorTexture != null)
        {
            return Sprite.Create(
                editorTexture,
                new Rect(0f, 0f, editorTexture.width, editorTexture.height),
                new Vector2(0.5f, 0.5f));
        }
#endif
        // Player builds can't read from Assets/ directly; this needs Assets/Resources/Sprites/Statistics.png.
        return Resources.Load<Sprite>("Sprites/Statistics");
    }

    private static void Stretch(RectTransform rect, Vector2 minOffset, Vector2 maxOffset)
    {
        rect.anchorMin = Vector2.zero;
        rect.anchorMax = Vector2.one;
        rect.offsetMin = minOffset;
        rect.offsetMax = maxOffset;
    }

    private static void SetPreferredHeight(RectTransform rect, float height)
    {
        LayoutElement element = rect.gameObject.GetComponent<LayoutElement>() ?? rect.gameObject.AddComponent<LayoutElement>();
        element.preferredHeight = height;
        element.minHeight = height;
    }

    private static void SetFlexibleWidth(RectTransform rect, float flexibleWidth)
    {
        LayoutElement element = rect.gameObject.GetComponent<LayoutElement>() ?? rect.gameObject.AddComponent<LayoutElement>();
        element.flexibleWidth = flexibleWidth;
        element.minWidth = 0f;
    }

    private static bool IsNumeric(Type type)
    {
        switch (Type.GetTypeCode(type))
        {
            case TypeCode.SByte:
            case TypeCode.Byte:
            case TypeCode.Int16:
            case TypeCode.UInt16:
            case TypeCode.Int32:
            case TypeCode.UInt32:
            case TypeCode.Int64:
            case TypeCode.UInt64:
            case TypeCode.Single:
            case TypeCode.Double:
            case TypeCode.Decimal:
                return true;
            default:
                return false;
        }
    }

    private static bool TryParseValue(string text, Type type, out object value)
    {
        value = null;
        if (type == typeof(string))
        {
            value = text;
            return true;
        }

        switch (Type.GetTypeCode(type))
        {
            case TypeCode.SByte:
                if (sbyte.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out sbyte sbyteValue)) { value = sbyteValue; return true; }
                break;
            case TypeCode.Byte:
                if (byte.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out byte byteValue)) { value = byteValue; return true; }
                break;
            case TypeCode.Int16:
                if (short.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out short shortValue)) { value = shortValue; return true; }
                break;
            case TypeCode.UInt16:
                if (ushort.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out ushort ushortValue)) { value = ushortValue; return true; }
                break;
            case TypeCode.Int32:
                if (int.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out int intValue)) { value = intValue; return true; }
                break;
            case TypeCode.UInt32:
                if (uint.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out uint uintValue)) { value = uintValue; return true; }
                break;
            case TypeCode.Int64:
                if (long.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out long longValue)) { value = longValue; return true; }
                break;
            case TypeCode.UInt64:
                if (ulong.TryParse(text, NumberStyles.Integer, CultureInfo.InvariantCulture, out ulong ulongValue)) { value = ulongValue; return true; }
                break;
            case TypeCode.Single:
                if (TryParseFloat(text, out float floatValue)) { value = floatValue; return true; }
                break;
            case TypeCode.Double:
                if (double.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out double doubleValue)) { value = doubleValue; return true; }
                break;
            case TypeCode.Decimal:
                if (decimal.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out decimal decimalValue)) { value = decimalValue; return true; }
                break;
        }
        return false;
    }

    private static bool TryParseFloat(string text, out float value)
    {
        return float.TryParse(text, NumberStyles.Float, CultureInfo.InvariantCulture, out value) ||
               float.TryParse(text, NumberStyles.Float, CultureInfo.CurrentCulture, out value);
    }

    private static object ApplyNumericConstraints(FieldInfo field, object value)
    {
        Type type = field.FieldType;
        RangeAttribute range = field.GetCustomAttribute<RangeAttribute>();
        MinAttribute min = field.GetCustomAttribute<MinAttribute>();

        if (type == typeof(int))
        {
            int result = (int)value;
            if (range != null) result = Mathf.Clamp(result, Mathf.CeilToInt(range.min), Mathf.FloorToInt(range.max));
            if (min != null) result = Mathf.Max(result, Mathf.CeilToInt(min.min));
            return result;
        }

        if (type == typeof(float))
        {
            float result = (float)value;
            if (range != null) result = Mathf.Clamp(result, range.min, range.max);
            if (min != null) result = Mathf.Max(result, min.min);
            return result;
        }

        if (type == typeof(double))
        {
            double result = (double)value;
            if (range != null) result = Math.Max(range.min, Math.Min(range.max, result));
            if (min != null) result = Math.Max(result, min.min);
            return result;
        }

        return value;
    }

    private static string FormatValue(object value)
    {
        if (value is float floatValue) return FormatFloat(floatValue);
        if (value is double doubleValue) return doubleValue.ToString("G9", CultureInfo.InvariantCulture);
        return Convert.ToString(value, CultureInfo.InvariantCulture);
    }

    private static string FormatFloat(float value)
    {
        return value.ToString("G7", CultureInfo.InvariantCulture);
    }

    private static string FormatFieldName(string fieldName)
    {
        if (string.IsNullOrEmpty(fieldName))
        {
            return fieldName;
        }

        System.Text.StringBuilder name = new System.Text.StringBuilder(fieldName.Length + 4);
        name.Append(char.ToUpperInvariant(fieldName[0]));
        for (int i = 1; i < fieldName.Length; i++)
        {
            char current = fieldName[i];
            char previous = fieldName[i - 1];
            bool nextIsLower = i + 1 < fieldName.Length && char.IsLower(fieldName[i + 1]);
            if (char.IsUpper(current) && (char.IsLower(previous) || nextIsLower))
            {
                name.Append(' ');
            }
            name.Append(current);
        }
        return name.ToString();
    }

    private static float[] GetVectorValues(object value)
    {
        if (value is Vector2Int vector2Int) return new[] { (float)vector2Int.x, vector2Int.y };
        if (value is Vector3Int vector3Int) return new[] { (float)vector3Int.x, vector3Int.y, vector3Int.z };
        if (value is Vector2 vector2) return new[] { vector2.x, vector2.y };
        if (value is Vector3 vector3) return new[] { vector3.x, vector3.y, vector3.z };
        if (value is Vector4 vector4) return new[] { vector4.x, vector4.y, vector4.z, vector4.w };
        if (value is Color color) return new[] { color.r, color.g, color.b, color.a };
        throw new ArgumentException("Unsupported vector value.", nameof(value));
    }

    private static object CreateVectorValue(Type type, float[] values, bool integerValues)
    {
        if (type == typeof(Vector2Int)) return new Vector2Int(Mathf.RoundToInt(values[0]), Mathf.RoundToInt(values[1]));
        if (type == typeof(Vector3Int)) return new Vector3Int(Mathf.RoundToInt(values[0]), Mathf.RoundToInt(values[1]), Mathf.RoundToInt(values[2]));
        if (type == typeof(Vector2)) return new Vector2(values[0], values[1]);
        if (type == typeof(Vector3)) return new Vector3(values[0], values[1], values[2]);
        if (type == typeof(Vector4)) return new Vector4(values[0], values[1], values[2], values[3]);
        if (type == typeof(Color)) return new Color(values[0], values[1], values[2], values[3]);
        throw new ArgumentException("Unsupported vector type.", nameof(type));
    }
}