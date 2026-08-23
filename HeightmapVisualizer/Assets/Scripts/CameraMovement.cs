using UnityEngine;
using UnityEngine.InputSystem;
using UnityEngine.EventSystems;

[RequireComponent(typeof(Camera))]
public class CameraMovement : MonoBehaviour
{
    [SerializeField] private float speed = 10f;
    [SerializeField] private float speedMultiplier = 3f;
    [SerializeField] private float sensitivity = 0.15f;
    [SerializeField] private float minRotation = -89f;
    [SerializeField] private float maxRotation = 89f;
    private float pitch;
    private float yaw;
    private bool cursorLocked;

    void Start()
    {
        yaw = transform.eulerAngles.y;
        pitch = transform.eulerAngles.x;
        SetCursorLocked(true);
    }

    void Update()
    {
        if (Keyboard.current == null || Mouse.current == null)
        {
            Debug.LogWarning("No mouse :(");
            return;
        }

        if (cursorLocked && Keyboard.current.escapeKey.wasPressedThisFrame)
        {
            SetCursorLocked(false);
        }

        if (!cursorLocked && Mouse.current.leftButton.wasPressedThisFrame)
        {
            if (EventSystem.current != null && !EventSystem.current.IsPointerOverGameObject())
            {
                SetCursorLocked(true);
            }
        }

        if (cursorLocked)
        {
            rotateCamera(Mouse.current);
            moveCamera(Keyboard.current);
        }
    }

    private void rotateCamera(Mouse mouse)
    {
        Vector2 delta = mouse.delta.ReadValue();

        yaw += delta.x * sensitivity;
        pitch -= delta.y * sensitivity;
        pitch = Mathf.Clamp(pitch, minRotation, maxRotation);

        transform.rotation = Quaternion.Euler(pitch, yaw, 0f);
    }

    private void moveCamera(Keyboard keyboard)
    {
        Vector3 moveDir = Vector3.zero;

        if (keyboard.wKey.isPressed) moveDir += transform.forward;
        if (keyboard.sKey.isPressed) moveDir -= transform.forward;
        if (keyboard.dKey.isPressed) moveDir += transform.right;
        if (keyboard.aKey.isPressed) moveDir -= transform.right;
        if (keyboard.spaceKey.isPressed) moveDir += Vector3.up;
        if (keyboard.ctrlKey.isPressed) moveDir -= Vector3.up;

        if (moveDir.sqrMagnitude > 0f)
        {
            moveDir.Normalize();
        }

        float currentSpeed = speed;
        if (keyboard.leftShiftKey.isPressed)
        {
            currentSpeed *= speedMultiplier;
        }

        transform.position += moveDir * currentSpeed * Time.deltaTime;
    }

    private void SetCursorLocked(bool isLocaked)
    {
        cursorLocked = isLocaked;
        Cursor.lockState = isLocaked ? CursorLockMode.Locked : CursorLockMode.None;
        Cursor.visible = !isLocaked;
    }
}