using UnityEngine;
using UnityEngine.EventSystems;
using UnityEngine.UI;

[DisallowMultipleComponent]
public sealed class QuitConfirmationDialog : MonoBehaviour
{
    public bool IsOpen => canvasObject != null && canvasObject.activeSelf;

    private GameObject canvasObject;
    private GameObject previousSelection;
    private Button noButton;
    private TerrainGen terrain;
    private TerrainVisualizationController visualizations;
    private bool terrainShortcutsEnabled;
    private bool visualizationShortcutsEnabled;

    public void Show()
    {
        if (IsOpen) return;
        if (canvasObject == null) BuildDialog();

        terrain = FindFirstObjectByType<TerrainGen>();
        visualizations = FindFirstObjectByType<TerrainVisualizationController>();
        if (terrain != null)
        {
            terrainShortcutsEnabled = terrain.KeyboardShortcutsEnabled;
            terrain.KeyboardShortcutsEnabled = false;
        }
        if (visualizations != null)
        {
            visualizationShortcutsEnabled = visualizations.KeyboardShortcutsEnabled;
            visualizations.KeyboardShortcutsEnabled = false;
        }

        previousSelection = EventSystem.current != null ? EventSystem.current.currentSelectedGameObject : null;
        canvasObject.SetActive(true);
        noButton.Select();
    }

    public void Hide()
    {
        if (!IsOpen) return;
        canvasObject.SetActive(false);
        if (terrain != null) terrain.KeyboardShortcutsEnabled = terrainShortcutsEnabled;
        if (visualizations != null) visualizations.KeyboardShortcutsEnabled = visualizationShortcutsEnabled;
        if (EventSystem.current != null)
        {
            EventSystem.current.SetSelectedGameObject(
                previousSelection != null && previousSelection.activeInHierarchy ? previousSelection : null);
        }
        previousSelection = null;
    }

    private void ConfirmQuit()
    {
#if UNITY_EDITOR
        UnityEditor.EditorApplication.isPlaying = false;
#else
        Application.Quit();
#endif
    }

    private void BuildDialog()
    {
        canvasObject = new GameObject("Quit Confirmation", typeof(RectTransform), typeof(Canvas), typeof(CanvasScaler), typeof(GraphicRaycaster));
        canvasObject.transform.SetParent(transform, false);
        Canvas canvas = canvasObject.GetComponent<Canvas>();
        canvas.renderMode = RenderMode.ScreenSpaceOverlay;
        canvas.sortingOrder = 100;
        CanvasScaler scaler = canvasObject.GetComponent<CanvasScaler>();
        scaler.uiScaleMode = CanvasScaler.ScaleMode.ScaleWithScreenSize;
        scaler.referenceResolution = new Vector2(1280f, 720f);
        scaler.matchWidthOrHeight = 0.5f;

        RectTransform blocker = CreateRect("Backdrop", canvasObject.transform);
        blocker.anchorMin = Vector2.zero;
        blocker.anchorMax = Vector2.one;
        blocker.offsetMin = blocker.offsetMax = Vector2.zero;
        blocker.gameObject.AddComponent<Image>().color = new Color(0f, 0f, 0f, 0.65f);

        RectTransform panel = CreateRect("Dialog", blocker);
        panel.sizeDelta = new Vector2(420f, 180f);
        panel.gameObject.AddComponent<Image>().color = new Color(0.055f, 0.075f, 0.105f, 1f);

        Font font = Resources.GetBuiltinResource<Font>("LegacyRuntime.ttf");
        Text title = CreateText("Question", panel, "Do you want to quit?", font, 24);
        title.rectTransform.anchoredPosition = new Vector2(0f, 36f);
        title.rectTransform.sizeDelta = new Vector2(380f, 60f);

        Button yesButton = CreateButton("Yes", panel, new Vector2(-92f, -42f), new Color(0.55f, 0.18f, 0.18f, 1f), font);
        yesButton.onClick.AddListener(ConfirmQuit);
        noButton = CreateButton("No", panel, new Vector2(92f, -42f), new Color(0.16f, 0.30f, 0.42f, 1f), font);
        noButton.onClick.AddListener(Hide);

        // Keep keyboard navigation inside the modal, with No selected initially.
        SetNavigation(yesButton, noButton);
        SetNavigation(noButton, yesButton);
        canvasObject.SetActive(false);
    }

    private static void SetNavigation(Button button, Button other)
    {
        button.navigation = new Navigation
        {
            mode = Navigation.Mode.Explicit,
            selectOnLeft = other,
            selectOnRight = other,
            selectOnUp = other,
            selectOnDown = other
        };
    }

    private static Button CreateButton(string label, Transform parent, Vector2 position, Color color, Font font)
    {
        RectTransform rect = CreateRect(label, parent);
        rect.anchoredPosition = position;
        rect.sizeDelta = new Vector2(156f, 46f);
        Image image = rect.gameObject.AddComponent<Image>();
        image.color = color;
        Button button = rect.gameObject.AddComponent<Button>();
        button.targetGraphic = image;
        Text text = CreateText("Label", rect, label, font, 20);
        text.rectTransform.anchorMin = Vector2.zero;
        text.rectTransform.anchorMax = Vector2.one;
        text.rectTransform.offsetMin = text.rectTransform.offsetMax = Vector2.zero;
        return button;
    }

    private static Text CreateText(string name, Transform parent, string value, Font font, int size)
    {
        Text text = CreateRect(name, parent).gameObject.AddComponent<Text>();
        text.font = font;
        text.fontSize = size;
        text.text = value;
        text.color = Color.white;
        text.alignment = TextAnchor.MiddleCenter;
        text.raycastTarget = false;
        return text;
    }

    private static RectTransform CreateRect(string name, Transform parent)
    {
        RectTransform rect = new GameObject(name, typeof(RectTransform)).GetComponent<RectTransform>();
        rect.SetParent(parent, false);
        rect.anchorMin = rect.anchorMax = rect.pivot = new Vector2(0.5f, 0.5f);
        return rect;
    }

    private void OnDisable()
    {
        Hide();
    }

    private void OnDestroy()
    {
        if (canvasObject != null) Destroy(canvasObject);
    }
}
