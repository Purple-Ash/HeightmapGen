using System;
using System.Collections.Generic;
using System.Globalization;
using System.Reflection;
using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.UI;

// This script dynamically generats settings UI using C# reflection system

[DefaultExecutionOrder(-100)]
public sealed class TerrainRuntimeSettingsUI : MonoBehaviour
{
    private readonly struct LoggedError
    {
        public readonly string Condition;
        public readonly string StackTrace;
        public readonly LogType Type;

        public LoggedError(string condition, string stackTrace, LogType type)
        {
            Condition = condition;
            StackTrace = stackTrace;
            Type = type;
        }
    }

    [SerializeField] private float PanelWidth;
    [SerializeField] private float PanelMargin;
    [SerializeField] private float PanelPadding;
    [SerializeField] private float HeaderHeight;
    [SerializeField] private float HintHeight;
    [SerializeField] private float ApplyHeight; 
    [SerializeField] private float RowHeight;
    [SerializeField] private float SectionHeight;
    [SerializeField] private float RowSpacing;

    [SerializeField] private Color PanelColor = new Color();
    [SerializeField] private Color BackgroundColor = new Color();
    [SerializeField] private Color FieldColor = new Color();
    [SerializeField] private Color AccentColor = new Color();
    [SerializeField] private Color TextColor = new Color();
    [SerializeField] private Color MutedTextColor = new Color();
    [Tooltip("Key to hide or show the in-game UI. Escape is reserved for restoring the UI while it is hidden.")]
    [SerializeField] private Key toggleUiKey = Key.F1;

    private readonly List<Action> pendingInputCommits = new List<Action>();
    private readonly Dictionary<FieldInfo, object> draftSettings = new Dictionary<FieldInfo, object>();
    private readonly Queue<LoggedError> pendingErrors = new Queue<LoggedError>();
    private readonly object pendingErrorsLock = new object();

    private TerrainGen terrain;
    private Font font;
    private Canvas rootCanvas;
    private Transform runtimeUiRoot;
    private RectTransform mainPanel;
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
    private RectTransform errorPopup;
    private Text errorPopupTitle;
    private ScrollRect errorPopupScrollRect;
    private RectTransform errorPopupContent;
    private RectTransform helpPopup;
    private bool allUiVisible = true;
    private bool escapeRestoredUiThisFrame;
    private TerrainVisualizationMenu visualizationMenu;
    private CameraMovement cameraMovement;
    private readonly List<LoggedError> recentErrors = new List<LoggedError>();
    private const int MaximumVisibleErrors = 12;
    private int screenWidth;
    private int screenHeight;
    private bool wasGenerating;

    public bool EscapeRestoredUiThisFrame => escapeRestoredUiThisFrame;

    private void Awake()
    {
        terrain = FindFirstObjectByType<TerrainGen>();
        if (terrain == null)
        {
            enabled = false;
            return;
        }

        if (toggleUiKey == Key.None || toggleUiKey == Key.Escape)
        {
            toggleUiKey = Key.F1;
        }

        Application.logMessageReceivedThreaded += OnLogMessageReceived;
        font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        BuildPanel();
        RefreshPanelSize();
        terrain.BeforeRegenerateChunks += ApplySettings;
        terrain.ResolutionAdjusted += OnResolutionAdjusted;
    }

    private void OnDestroy()
    {
        Application.logMessageReceivedThreaded -= OnLogMessageReceived;
        if (terrain != null)
        {
            terrain.BeforeRegenerateChunks -= ApplySettings;
            terrain.ResolutionAdjusted -= OnResolutionAdjusted;
        }
    }

    private void Update()
    {
        escapeRestoredUiThisFrame = false;
        Keyboard keyboard = Keyboard.current;
        if (keyboard != null)
        {
            bool togglePressed = toggleUiKey != Key.None && toggleUiKey != Key.Escape &&
                                 keyboard[toggleUiKey].wasPressedThisFrame;
            if (togglePressed)
            {
                SetAllUiVisible(!allUiVisible);
                return;
            }

            if (!allUiVisible && keyboard.escapeKey.wasPressedThisFrame)
            {
                SetAllUiVisible(true);
                escapeRestoredUiThisFrame = true;
                return;
            }
        }

        ProcessPendingErrors();

        if (screenWidth != Screen.width || screenHeight != Screen.height)
        {
            RefreshPanelSize();
        }

        bool isGenerating = TerrainGen.IsGenerating;
        if (isGenerating != wasGenerating)
        {
            wasGenerating = isGenerating;
            applyButtonText.text = isGenerating ? "Apply & Restart Generation" : "Apply & Regenerate";
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

        RectTransform runtimeRoot = CreateObject("Terrain Runtime UI", rootCanvas.transform).GetComponent<RectTransform>();
        Stretch(runtimeRoot, Vector2.zero, Vector2.zero);
        runtimeUiRoot = runtimeRoot;

        mainPanel = CreateObject("Terrain Settings Panel", runtimeUiRoot).GetComponent<RectTransform>();
        mainPanel.anchorMin = Vector2.one;
        mainPanel.anchorMax = Vector2.one;
        mainPanel.pivot = Vector2.one;
        mainPanel.anchoredPosition = new Vector2(-PanelMargin, -PanelMargin);
        mainPanel.SetSizeWithCurrentAnchors(RectTransform.Axis.Horizontal, PanelWidth);

        Image panelImage = mainPanel.gameObject.AddComponent<Image>();
        panelImage.color = PanelColor;

        VerticalLayoutGroup panelLayout = mainPanel.gameObject.AddComponent<VerticalLayoutGroup>();
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

        RectTransform titleRow = CreateObject("Title Row", mainPanel).GetComponent<RectTransform>();
        HorizontalLayoutGroup titleLayout = titleRow.gameObject.AddComponent<HorizontalLayoutGroup>();
        titleLayout.spacing = 8f;
        titleLayout.childControlWidth = true;
        titleLayout.childControlHeight = true;
        titleLayout.childForceExpandWidth = false;
        titleLayout.childForceExpandHeight = true;
        SetPreferredHeight(titleRow, HeaderHeight);

        Text title = CreateText("Title", titleRow, "Terrain Settings", 18, TextColor, TextAnchor.MiddleLeft);
        SetFlexibleWidth(title.rectTransform, 1f);
        Button helpButton = CreateButton("Help", titleRow, "Help", FieldColor, ShowHelpPopup);
        LayoutElement helpButtonLayout = helpButton.gameObject.AddComponent<LayoutElement>();
        helpButtonLayout.preferredWidth = 56f;
        helpButtonLayout.minWidth = 56f;

        Text hint = CreateText(
            "Hint",
            mainPanel,
            "Press Escape to restore hidden UI. Click terrain to resume camera control.",
            11,
            MutedTextColor,
            TextAnchor.MiddleLeft);
        SetPreferredHeight(hint.rectTransform, HintHeight);

        BuildScrollView(mainPanel);
        BuildFieldControls();

        applyButton = CreateButton("Apply", mainPanel, "Apply & Regenerate", AccentColor, terrain.Regenerate);
        SetPreferredHeight(applyButton.GetComponent<RectTransform>(), ApplyHeight);
        applyButtonText = applyButton.GetComponentInChildren<Text>();
        wasGenerating = TerrainGen.IsGenerating;
        if (wasGenerating)
        {
            applyButtonText.text = "Apply & Restart Generation";
        }

        BuildStatisticsOverlay();
        BuildErrorPopup();
        BuildHelpPopup();
    }

    private void BuildScrollView(Transform parent)
    {
        GameObject scrollObject = CreateObject("Settings Scroll View", parent);
        Image scrollBackground = scrollObject.AddComponent<Image>();
        scrollBackground.color = BackgroundColor;
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

        foreach (FieldInfo field in fields)
        {
            if (IsRuntimeEditable(field) && !draftSettings.ContainsKey(field))
            {
                draftSettings[field] = field.GetValue(terrain);
            }
        }

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

            if (!IsVisibleForCurrentSettings(field))
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

    private bool IsVisibleForCurrentSettings(FieldInfo field)
    {
        TerrainGen.GeneratorSpecificAttribute generatorSpecific =
            field.GetCustomAttribute<TerrainGen.GeneratorSpecificAttribute>();
        if (generatorSpecific != null)
        {
            FieldInfo generatorField = typeof(TerrainGen).GetField("generatorType",
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            TerrainGen.GeneratorType selectedGenerator =
                (TerrainGen.GeneratorType)draftSettings[generatorField];
            bool applies = false;
            foreach (TerrainGen.GeneratorType generator in generatorSpecific.Generators)
            {
                if (generator == selectedGenerator)
                {
                    applies = true;
                    break;
                }
            }
            if (!applies)
            {
                return false;
            }
        }

        TerrainGen.VisibleWhenAttribute visibleWhen = field.GetCustomAttribute<TerrainGen.VisibleWhenAttribute>();
        if (visibleWhen != null)
        {
            FieldInfo conditionField = typeof(TerrainGen).GetField(visibleWhen.FieldName,
                BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
            if (conditionField == null || !draftSettings.TryGetValue(conditionField, out object conditionValue) ||
                !Equals(conditionValue, visibleWhen.Value))
            {
                return false;
            }
        }

        return true;
    }

    private bool HasVisibilityDependency(FieldInfo field)
    {
        FieldInfo[] fields = typeof(TerrainGen).GetFields(
            BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
        foreach (FieldInfo candidate in fields)
        {
            TerrainGen.VisibleWhenAttribute visibleWhen =
                candidate.GetCustomAttribute<TerrainGen.VisibleWhenAttribute>();
            if (visibleWhen != null && visibleWhen.FieldName == field.Name)
            {
                return true;
            }
        }
        return false;
    }

    private void RebuildFieldControls(FieldInfo updatedField = null, object updatedValue = null)
    {
        foreach (Action commit in pendingInputCommits)
        {
            commit();
        }
        pendingInputCommits.Clear();
        if (updatedField != null)
        {
            draftSettings[updatedField] = updatedValue;
        }
        CloseEnumDropdown();

        for (int i = content.childCount - 1; i >= 0; i--)
        {
            Transform child = content.GetChild(i);
            child.gameObject.SetActive(false);
            Destroy(child.gameObject);
        }

        BuildFieldControls();
        RefreshPanelSize();
    }

    private void OnResolutionAdjusted(int requiredResolution)
    {
        FieldInfo resolutionField = typeof(TerrainGen).GetField("resolution",
            BindingFlags.Instance | BindingFlags.Public | BindingFlags.NonPublic);
        RebuildFieldControls(resolutionField, requiredResolution);
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
        bool initialValue = (bool)draftSettings[field];
        Button button = CreateButton("Value", row, initialValue ? "On" : "Off", FieldColor, null);
        SetFlexibleWidth(button.GetComponent<RectTransform>(), 1f);
        Text valueText = button.GetComponentInChildren<Text>();

        button.onClick.AddListener(() =>
        {
            bool value = !(bool)draftSettings[field];
            draftSettings[field] = value;
            valueText.text = value ? "On" : "Off";
            if (HasVisibilityDependency(field))
            {
                RebuildFieldControls();
            }
        });
    }

    private void AddEnumControl(FieldInfo field)
    {
        RectTransform row = CreateRow(field);
        Button button = CreateButton("Value", row, draftSettings[field].ToString(), FieldColor, null);
        SetFlexibleWidth(button.GetComponent<RectTransform>(), 1f);
        Text valueText = button.GetComponentInChildren<Text>();

        button.onClick.AddListener(() => ShowEnumDropdown(field, button.GetComponent<RectTransform>(), valueText));
    }

    private void AddTextControl(FieldInfo field)
    {
        RectTransform row = CreateRow(field);
        InputField input = CreateInputField(row, FormatValue(draftSettings[field]), "Value");
        SetFlexibleWidth(input.GetComponent<RectTransform>(), 1f);

        Action commit = () =>
        {
            if (TryParseValue(input.text, field.FieldType, out object parsedValue))
            {
                object constrainedValue = ApplyNumericConstraints(field, parsedValue);
                draftSettings[field] = constrainedValue;
                input.text = FormatValue(constrainedValue);
            }
            else
            {
                input.text = FormatValue(draftSettings[field]);
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

        float[] initialValues = GetVectorValues(draftSettings[field]);
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
                    values = GetVectorValues(draftSettings[field]);
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
            draftSettings[field] = vectorValue;
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
        enumDropdown = CreateObject("Generator Dropdown", runtimeUiRoot);
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
                draftSettings[field] = selectedValue;
                caption.text = selectedValue.ToString();
                CloseEnumDropdown();
                if (field.FieldType == typeof(TerrainGen.GeneratorType))
                {
                    RebuildFieldControls();
                }
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
        statisticsToggle = CreateButton("Statistics Toggle", runtimeUiRoot, string.Empty, FieldColor, ShowStatistics);
        RectTransform toggleRect = statisticsToggle.GetComponent<RectTransform>();
        toggleRect.anchorMin = new Vector2(0f, 1f);
        toggleRect.anchorMax = new Vector2(0f, 1f);
        toggleRect.pivot = new Vector2(0f, 1f);
        toggleRect.anchoredPosition = new Vector2(PanelMargin, -PanelMargin);
        toggleRect.sizeDelta = new Vector2(38f, 36f);
        CreateStatisticsIcon(statisticsToggle);

        statisticsPanel = CreateObject("Statistics Panel", runtimeUiRoot).GetComponent<RectTransform>();
        statisticsPanel.anchorMin = new Vector2(0f, 1f);
        statisticsPanel.anchorMax = new Vector2(0f, 1f);
        statisticsPanel.pivot = new Vector2(0f, 1f);
        statisticsPanel.anchoredPosition = new Vector2(PanelMargin, -PanelMargin);
        statisticsPanel.sizeDelta = new Vector2(330f, 0f);

        Image background = statisticsPanel.gameObject.AddComponent<Image>();
        background.color = PanelColor;
        VerticalLayoutGroup layout = statisticsPanel.gameObject.AddComponent<VerticalLayoutGroup>();
        layout.padding = new RectOffset(12, 12, 10, 10);
        layout.spacing = 6f;
        layout.childControlWidth = true;
        layout.childControlHeight = true;
        layout.childForceExpandWidth = true;
        layout.childForceExpandHeight = false;

        ContentSizeFitter statisticsSize = statisticsPanel.gameObject.AddComponent<ContentSizeFitter>();
        statisticsSize.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        statisticsSize.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

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
        ShowStatistics();
    }

    private void BuildErrorPopup()
    {
        errorPopup = CreateObject("Runtime Error Popup", runtimeUiRoot).GetComponent<RectTransform>();
        errorPopup.anchorMin = new Vector2(0.5f, 0.5f);
        errorPopup.anchorMax = new Vector2(0.5f, 0.5f);
        errorPopup.pivot = new Vector2(0.5f, 0.5f);
        RefreshErrorPopupSize();

        Image background = errorPopup.gameObject.AddComponent<Image>();
        background.color = new Color(0.045f, 0.035f, 0.055f, 0.98f);
        Outline outline = errorPopup.gameObject.AddComponent<Outline>();
        Color errorAccent = new Color(0.86f, 0.22f, 0.2f, 1f);
        outline.effectColor = new Color(errorAccent.r, errorAccent.g, errorAccent.b, 0.75f);
        outline.effectDistance = new Vector2(1f, -1f);

        RectTransform accentBar = CreateObject("Error Accent", errorPopup).GetComponent<RectTransform>();
        accentBar.anchorMin = new Vector2(0f, 1f);
        accentBar.anchorMax = new Vector2(1f, 1f);
        accentBar.pivot = new Vector2(0.5f, 1f);
        accentBar.anchoredPosition = Vector2.zero;
        accentBar.sizeDelta = new Vector2(0f, 4f);
        accentBar.gameObject.AddComponent<Image>().color = errorAccent;

        errorPopupTitle = CreateText("Error Title", errorPopup, "Error", 20, Color.white, TextAnchor.MiddleLeft);
        errorPopupTitle.fontStyle = FontStyle.Bold;
        SetHelpRect(errorPopupTitle.rectTransform, new Vector2(0f, 1f), new Vector2(0f, 1f),
            new Vector2(22f, -16f), new Vector2(340f, 34f));

        Button closeButton = CreateButton("Close Errors", errorPopup, "Close", FieldColor, HideErrorPopup);
        SetHelpRect(closeButton.GetComponent<RectTransform>(), new Vector2(1f, 1f), new Vector2(1f, 1f),
            new Vector2(-18f, -14f), new Vector2(76f, 34f));

        RectTransform divider = CreateObject("Error Divider", errorPopup).GetComponent<RectTransform>();
        divider.anchorMin = new Vector2(0f, 1f);
        divider.anchorMax = new Vector2(1f, 1f);
        divider.pivot = new Vector2(0.5f, 1f);
        divider.anchoredPosition = new Vector2(0f, -60f);
        divider.sizeDelta = new Vector2(-40f, 1f);
        divider.gameObject.AddComponent<Image>().color = new Color(errorAccent.r, errorAccent.g, errorAccent.b, 0.45f);

        GameObject scrollObject = CreateObject("Error Details Scroll View", errorPopup);
        RectTransform scrollRect = scrollObject.GetComponent<RectTransform>();
        scrollRect.anchorMin = Vector2.zero;
        scrollRect.anchorMax = Vector2.one;
        scrollRect.offsetMin = new Vector2(18f, 16f);
        scrollRect.offsetMax = new Vector2(-18f, -72f);

        errorPopupScrollRect = scrollObject.AddComponent<ScrollRect>();
        errorPopupScrollRect.horizontal = false;
        errorPopupScrollRect.vertical = true;
        errorPopupScrollRect.movementType = ScrollRect.MovementType.Clamped;
        errorPopupScrollRect.scrollSensitivity = 24f;

        RectTransform viewport = CreateObject("Error Details Viewport", scrollObject.transform).GetComponent<RectTransform>();
        Stretch(viewport, Vector2.zero, Vector2.zero);
        Image viewportImage = viewport.gameObject.AddComponent<Image>();
        viewportImage.color = new Color(0.025f, 0.025f, 0.04f, 0.55f);
        Mask viewportMask = viewport.gameObject.AddComponent<Mask>();
        viewportMask.showMaskGraphic = false;

        errorPopupContent = CreateObject("Error Entries", viewport).GetComponent<RectTransform>();
        errorPopupContent.anchorMin = new Vector2(0f, 1f);
        errorPopupContent.anchorMax = new Vector2(1f, 1f);
        errorPopupContent.pivot = new Vector2(0.5f, 1f);
        errorPopupContent.anchoredPosition = Vector2.zero;
        errorPopupContent.sizeDelta = new Vector2(0f, 0f);

        VerticalLayoutGroup contentLayout = errorPopupContent.gameObject.AddComponent<VerticalLayoutGroup>();
        contentLayout.padding = new RectOffset(8, 8, 8, 8);
        contentLayout.spacing = 8f;
        contentLayout.childControlWidth = true;
        contentLayout.childControlHeight = true;
        contentLayout.childForceExpandWidth = true;
        contentLayout.childForceExpandHeight = false;

        ContentSizeFitter contentSize = errorPopupContent.gameObject.AddComponent<ContentSizeFitter>();
        contentSize.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        contentSize.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

        errorPopupScrollRect.viewport = viewport;
        errorPopupScrollRect.content = errorPopupContent;
        errorPopup.gameObject.SetActive(false);
    }

    private void OnLogMessageReceived(string condition, string stackTrace, LogType type)
    {
        if (type != LogType.Error && type != LogType.Exception && type != LogType.Assert)
        {
            return;
        }

        lock (pendingErrorsLock)
        {
            pendingErrors.Enqueue(new LoggedError(condition, stackTrace, type));
            while (pendingErrors.Count > 64)
            {
                pendingErrors.Dequeue();
            }
        }
    }

    private void ProcessPendingErrors()
    {
        bool receivedErrors = false;
        while (true)
        {
            LoggedError error;
            lock (pendingErrorsLock)
            {
                if (pendingErrors.Count == 0)
                {
                    break;
                }
                error = pendingErrors.Dequeue();
            }

            AddErrorToPopup(error);
            receivedErrors = true;
        }

        if (receivedErrors)
        {
            if (allUiVisible)
            {
                errorPopup.gameObject.SetActive(true);
                errorPopup.SetAsLastSibling();
            }
            RefreshErrorPopupContent();
        }
    }

    private void AddErrorToPopup(LoggedError error)
    {
        recentErrors.Insert(0, error);
        if (recentErrors.Count > MaximumVisibleErrors)
        {
            recentErrors.RemoveAt(recentErrors.Count - 1);
        }

    }

    private void HideErrorPopup()
    {
        recentErrors.Clear();
        lock (pendingErrorsLock)
        {
            pendingErrors.Clear();
        }
        errorPopupTitle.text = "Error";
        ClearErrorPopupEntries();
        errorPopup.gameObject.SetActive(false);
    }

    private void RefreshErrorPopupContent()
    {
        ClearErrorPopupEntries();
        foreach (LoggedError error in recentErrors)
        {
            CreateErrorEntry(error);
        }

        errorPopupTitle.text = recentErrors.Count == 1 ? "Error" : "Errors (" + recentErrors.Count + ")";
        if (allUiVisible)
        {
            Canvas.ForceUpdateCanvases();
            errorPopupScrollRect.verticalNormalizedPosition = 1f;
        }
    }

    private void ClearErrorPopupEntries()
    {
        for (int i = errorPopupContent.childCount - 1; i >= 0; i--)
        {
            Transform child = errorPopupContent.GetChild(i);
            child.gameObject.SetActive(false);
            Destroy(child.gameObject);
        }
    }

    private void CreateErrorEntry(LoggedError error)
    {
        RectTransform card = CreateObject("Error Entry", errorPopupContent).GetComponent<RectTransform>();
        Image background = card.gameObject.AddComponent<Image>();
        background.color = new Color(0.09f, 0.055f, 0.075f, 0.92f);

        string typeLabel = error.Type == LogType.Exception ? "EXCEPTION"
            : error.Type == LogType.Assert ? "ASSERT" : "ERROR";
        Color typeColor = error.Type == LogType.Assert
            ? new Color(1f, 0.64f, 0.25f, 1f)
            : new Color(1f, 0.38f, 0.34f, 1f);
        Outline cardOutline = card.gameObject.AddComponent<Outline>();
        cardOutline.effectColor = new Color(typeColor.r, typeColor.g, typeColor.b, 0.35f);
        cardOutline.effectDistance = new Vector2(0.5f, -0.5f);

        VerticalLayoutGroup layout = card.gameObject.AddComponent<VerticalLayoutGroup>();
        layout.padding = new RectOffset(10, 10, 8, 8);
        layout.spacing = 5f;
        layout.childControlWidth = true;
        layout.childControlHeight = true;
        layout.childForceExpandWidth = true;
        layout.childForceExpandHeight = false;

        ContentSizeFitter cardSize = card.gameObject.AddComponent<ContentSizeFitter>();
        cardSize.horizontalFit = ContentSizeFitter.FitMode.Unconstrained;
        cardSize.verticalFit = ContentSizeFitter.FitMode.PreferredSize;

        RectTransform heading = CreateObject("Error Entry Header", card).GetComponent<RectTransform>();
        HorizontalLayoutGroup headingLayout = heading.gameObject.AddComponent<HorizontalLayoutGroup>();
        headingLayout.childControlWidth = true;
        headingLayout.childControlHeight = true;
        headingLayout.childForceExpandWidth = false;
        headingLayout.childForceExpandHeight = true;
        SetPreferredHeight(heading, 20f);

        RectTransform badge = CreateObject("Error Type Badge", heading).GetComponent<RectTransform>();
        Image badgeBackground = badge.gameObject.AddComponent<Image>();
        badgeBackground.color = new Color(typeColor.r, typeColor.g, typeColor.b, 0.16f);
        LayoutElement badgeLayout = badge.gameObject.AddComponent<LayoutElement>();
        badgeLayout.preferredWidth = error.Type == LogType.Exception ? 92f : 64f;
        badgeLayout.minWidth = badgeLayout.preferredWidth;
        SetPreferredHeight(badge, 20f);

        Text label = CreateText("Error Type", badge, typeLabel, 11, typeColor, TextAnchor.MiddleCenter);
        label.fontStyle = FontStyle.Bold;
        Stretch(label.rectTransform, Vector2.zero, Vector2.zero);

        string message = string.IsNullOrWhiteSpace(error.Condition) ? error.Type.ToString() : error.Condition.Trim();
        if (!string.IsNullOrWhiteSpace(error.StackTrace))
        {
            string[] stackLines = error.StackTrace.Split(new[] { '\r', '\n' }, StringSplitOptions.RemoveEmptyEntries);
            if (stackLines.Length > 0)
            {
                message += "\n" + stackLines[0].Trim();
            }
        }

        Text details = CreateText("Error Message", card, message, 13, TextColor, TextAnchor.UpperLeft);
        details.horizontalOverflow = HorizontalWrapMode.Wrap;
        details.verticalOverflow = VerticalWrapMode.Overflow;
    }

    private void BuildHelpPopup()
    {
        helpPopup = CreateObject("Runtime Help Popup", runtimeUiRoot).GetComponent<RectTransform>();
        helpPopup.anchorMin = new Vector2(0.5f, 0.5f);
        helpPopup.anchorMax = new Vector2(0.5f, 0.5f);
        helpPopup.pivot = new Vector2(0.5f, 0.5f);
        RefreshHelpPopupSize();

        Image background = helpPopup.gameObject.AddComponent<Image>();
        background.color = new Color(0.035f, 0.05f, 0.075f, 0.97f);
        Outline outline = helpPopup.gameObject.AddComponent<Outline>();
        outline.effectColor = new Color(AccentColor.r, AccentColor.g, AccentColor.b, 0.75f);
        outline.effectDistance = new Vector2(1f, -1f);

        RectTransform accentBar = CreateObject("Help Accent", helpPopup).GetComponent<RectTransform>();
        accentBar.anchorMin = new Vector2(0f, 1f);
        accentBar.anchorMax = new Vector2(1f, 1f);
        accentBar.pivot = new Vector2(0.5f, 1f);
        accentBar.anchoredPosition = Vector2.zero;
        accentBar.sizeDelta = new Vector2(0f, 4f);
        accentBar.gameObject.AddComponent<Image>().color = AccentColor;

        Text title = CreateText("Help Title", helpPopup, "Controls", 22, TextColor, TextAnchor.MiddleLeft);
        title.fontStyle = FontStyle.Bold;
        SetHelpRect(title.rectTransform, new Vector2(0f, 1f), new Vector2(0f, 1f),
            new Vector2(24f, -18f), new Vector2(260f, 34f));

        Button closeButton = CreateButton("Close Help", helpPopup, "Close", FieldColor, HideHelpPopup);
        RectTransform closeRect = closeButton.GetComponent<RectTransform>();
        SetHelpRect(closeRect, new Vector2(1f, 1f), new Vector2(1f, 1f),
            new Vector2(-20f, -16f), new Vector2(76f, 34f));

        RectTransform divider = CreateObject("Help Divider", helpPopup).GetComponent<RectTransform>();
        divider.anchorMin = new Vector2(0f, 1f);
        divider.anchorMax = new Vector2(1f, 1f);
        divider.pivot = new Vector2(0.5f, 1f);
        divider.anchoredPosition = new Vector2(0f, -64f);
        divider.sizeDelta = new Vector2(-44f, 1f);
        divider.gameObject.AddComponent<Image>().color = new Color(AccentColor.r, AccentColor.g, AccentColor.b, 0.45f);

        Text section = CreateText("Keyboard Controls Heading", helpPopup, "Keyboard controls", 14,
            AccentColor, TextAnchor.MiddleLeft);
        section.fontStyle = FontStyle.Bold;
        SetHelpRect(section.rectTransform, new Vector2(0f, 1f), new Vector2(0f, 1f),
            new Vector2(24f, -76f), new Vector2(260f, 22f));

        string[] keys =
        {
            toggleUiKey.ToString(), "Escape", "R", "W / A / S / D", "Space / Ctrl", "Shift", "1–6"
        };
        string[] descriptions =
        {
            "hide or show all in-game UI",
            "release the cursor or open the quit dialog",
            "regenerate terrain",
            "move the camera",
            "move up or down",
            "move faster",
            "select a visualization preset"
        };

        for (int i = 0; i < keys.Length; i++)
        {
            CreateHelpControlRow(i, keys[i], descriptions[i]);
        }

        helpPopup.gameObject.SetActive(false);
    }

    private void CreateHelpControlRow(int index, string key, string description)
    {
        const float rowHeight = 36f;
        const float rowSpacing = 5f;
        float top = 102f + index * (rowHeight + rowSpacing);

        RectTransform row = CreateObject("Help Control Row", helpPopup).GetComponent<RectTransform>();
        row.anchorMin = new Vector2(0f, 1f);
        row.anchorMax = new Vector2(1f, 1f);
        row.pivot = new Vector2(0.5f, 1f);
        row.anchoredPosition = new Vector2(0f, -top);
        row.sizeDelta = new Vector2(-44f, rowHeight);
        Image rowBackground = row.gameObject.AddComponent<Image>();
        rowBackground.color = index % 2 == 0
            ? new Color(0.09f, 0.12f, 0.17f, 0.82f)
            : new Color(0.07f, 0.095f, 0.14f, 0.82f);

        RectTransform keyBadge = CreateObject("Key Badge", row).GetComponent<RectTransform>();
        keyBadge.anchorMin = new Vector2(0f, 0.5f);
        keyBadge.anchorMax = new Vector2(0f, 0.5f);
        keyBadge.pivot = new Vector2(0f, 0.5f);
        keyBadge.anchoredPosition = new Vector2(10f, 0f);
        keyBadge.sizeDelta = new Vector2(124f, 28f);
        Image keyBackground = keyBadge.gameObject.AddComponent<Image>();
        keyBackground.color = new Color(AccentColor.r, AccentColor.g, AccentColor.b, 0.95f);

        Text keyText = CreateText("Key", keyBadge, key, 13, Color.white, TextAnchor.MiddleCenter);
        keyText.fontStyle = FontStyle.Bold;
        Stretch(keyText.rectTransform, Vector2.zero, Vector2.zero);

        Text descriptionText = CreateText("Description", row, description, 14, TextColor, TextAnchor.MiddleLeft);
        descriptionText.horizontalOverflow = HorizontalWrapMode.Wrap;
        descriptionText.verticalOverflow = VerticalWrapMode.Truncate;
        RectTransform descriptionRect = descriptionText.rectTransform;
        descriptionRect.anchorMin = Vector2.zero;
        descriptionRect.anchorMax = Vector2.one;
        descriptionRect.offsetMin = new Vector2(148f, 0f);
        descriptionRect.offsetMax = new Vector2(-10f, 0f);
    }

    private void ShowHelpPopup()
    {
        helpPopup.gameObject.SetActive(true);
        helpPopup.SetAsLastSibling();
    }

    private void HideHelpPopup()
    {
        helpPopup.gameObject.SetActive(false);
    }

    private void RefreshHelpPopupSize()
    {
        if (helpPopup == null)
        {
            return;
        }

        float width = Mathf.Max(1f, Mathf.Min(520f, Screen.width - 48f));
        float height = Mathf.Max(180f, Mathf.Min(400f, Screen.height - 48f));
        helpPopup.sizeDelta = new Vector2(width, height);
    }

    private static void SetHelpRect(RectTransform rect, Vector2 anchor, Vector2 pivot,
        Vector2 position, Vector2 size)
    {
        rect.anchorMin = rect.anchorMax = anchor;
        rect.pivot = pivot;
        rect.anchoredPosition = position;
        rect.sizeDelta = size;
    }

    private void SetAllUiVisible(bool visible)
    {
        allUiVisible = visible;
        if (!visible)
        {
            CloseEnumDropdown();
            HideHelpPopup();
            QuitConfirmationDialog quitDialog = FindFirstObjectByType<QuitConfirmationDialog>();
            if (quitDialog != null)
            {
                quitDialog.Hide();
            }
        }

        errorPopup.gameObject.SetActive(visible && recentErrors.Count > 0);
        runtimeUiRoot.gameObject.SetActive(visible);
        if (visible && recentErrors.Count > 0)
        {
            Canvas.ForceUpdateCanvases();
            errorPopupScrollRect.verticalNormalizedPosition = 1f;
        }

        if (visualizationMenu == null)
        {
            visualizationMenu = FindFirstObjectByType<TerrainVisualizationMenu>();
        }
        if (visualizationMenu != null)
        {
            visualizationMenu.SetMenuVisible(visible);
        }

        if (cameraMovement == null)
        {
            cameraMovement = FindFirstObjectByType<CameraMovement>();
        }
        if (cameraMovement != null)
        {
            cameraMovement.SetCursorLocked(!visible);
        }
    }

    private void RefreshErrorPopupSize()
    {
        if (errorPopup == null)
        {
            return;
        }

        float width = Mathf.Max(1f, Mathf.Min(520f, Screen.width - 48f));
        float height = Mathf.Max(180f, Mathf.Min(320f, Screen.height - 48f));
        errorPopup.sizeDelta = new Vector2(width, height);
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
        string heights = terrain.HasHeightStatistics
            ? "Highest point: " + terrain.HighestHeight.ToString("F2", CultureInfo.InvariantCulture) +
              "\nLowest point: " + terrain.LowestHeight.ToString("F2", CultureInfo.InvariantCulture) +
              "\nAverage height: " + terrain.AverageHeight.ToString("F2", CultureInfo.InvariantCulture)
            : "Highest point: —\nLowest point: —\nAverage height: —";
        string slopes = terrain.HasSlopeStatistics
            ? "\nAverage slope: " + terrain.AverageSlope.ToString("F2", CultureInfo.InvariantCulture) + "°" +
              "\nMaximum slope: " + terrain.MaximumSlope.ToString("F2", CultureInfo.InvariantCulture) + "°"
            : "\nAverage slope: —\nMaximum slope: —";
        string averageChunkTime = terrain.CompletedChunkCount > 0
            ? terrain.AverageChunkGenerationSeconds.ToString("F3", CultureInfo.InvariantCulture) + " s"
            : "—";

        statisticsText.text = heights + slopes +
                              "\nElapsed generation: " + terrain.ElapsedGenerationSeconds.ToString("F2", CultureInfo.InvariantCulture) + " s" +
                              "\nElapsed / completed chunk: " + averageChunkTime;
    }

    private GameObject CreateClickBlocker(string name, UnityEngine.Events.UnityAction onClick)
    {
        GameObject blocker = CreateObject(name, runtimeUiRoot);
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

    private void ApplySettings()
    {
        foreach (Action commit in pendingInputCommits)
        {
            commit();
        }

        foreach (KeyValuePair<FieldInfo, object> setting in draftSettings)
        {
            setting.Key.SetValue(terrain, setting.Value);
        }

        CloseEnumDropdown();
    }

    private void RefreshPanelSize()
    {
        screenWidth = Screen.width;
        screenHeight = Screen.height;

        RefreshErrorPopupSize();
        RefreshHelpPopupSize();

        Canvas.ForceUpdateCanvases();
        LayoutRebuilder.ForceRebuildLayoutImmediate(content);
        float desiredContentHeight = LayoutUtility.GetPreferredHeight(content);
        float maximumPanelHeight = Screen.height * (2f / 3f);
        float panelOverhead = PanelPadding * 2f + HeaderHeight + HintHeight + ApplyHeight + RowSpacing * 3f;
        float maximumViewportHeight = Mathf.Max(20f, maximumPanelHeight - panelOverhead);
        float viewportHeight = Mathf.Min(desiredContentHeight, maximumViewportHeight);

        viewportLayout.preferredHeight = viewportHeight;
        mainPanel.SetSizeWithCurrentAnchors(RectTransform.Axis.Vertical, panelOverhead + viewportHeight);
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
        const string assetPath = "Assets/Sprites/Statistics.png";
        Sprite editorSprite = UnityEditor.AssetDatabase.LoadAssetAtPath<Sprite>(assetPath);
        if (editorSprite != null)
        {
            return editorSprite;
        }

        Texture2D editorTexture = UnityEditor.AssetDatabase.LoadAssetAtPath<Texture2D>(assetPath);
        if (editorTexture != null)
        {
            return Sprite.Create(
                editorTexture,
                new Rect(0f, 0f, editorTexture.width, editorTexture.height),
                new Vector2(0.5f, 0.5f));
        }
#endif
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

    private static object CreateVectorValue(Type type, float[] values, bool integerValues)
    {
        if (type == typeof(Vector2Int)) return new Vector2Int(Mathf.RoundToInt(values[0]), Mathf.RoundToInt(values[1]));
        if (type == typeof(Vector3Int)) return new Vector3Int(Mathf.RoundToInt(values[0]), Mathf.RoundToInt(values[1]), Mathf.RoundToInt(values[2]));
        if (type == typeof(Vector2)) return new Vector2(values[0], values[1]);
        if (type == typeof(Vector3)) return new Vector3(values[0], values[1], values[2]);
        if (type == typeof(Vector4)) return new Vector4(values[0], values[1], values[2], values[3]);
        if (type == typeof(Color)) return new Color(values[0], values[1], values[2], values[3]);
       
        throw new ArgumentException("Unsupported vector type", nameof(type));
    }

    private static float[] GetVectorValues(object value)
    {
        if (value is Vector2Int vector2Int) return new[] { (float)vector2Int.x, vector2Int.y };
        if (value is Vector3Int vector3Int) return new[] { (float)vector3Int.x, vector3Int.y, vector3Int.z };
        if (value is Vector2 vector2) return new[] { vector2.x, vector2.y };
        if (value is Vector3 vector3) return new[] { vector3.x, vector3.y, vector3.z };
        if (value is Vector4 vector4) return new[] { vector4.x, vector4.y, vector4.z, vector4.w };
        if (value is Color color) return new[] { color.r, color.g, color.b, color.a };
        throw new ArgumentException("Unsupported vector value", nameof(value));
    }
}
