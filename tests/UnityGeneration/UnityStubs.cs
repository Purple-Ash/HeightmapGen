using System;
using System.Collections;
using System.Collections.Generic;
using System.Diagnostics;
using System.Linq;
using System.Reflection;
using System.Threading;
using System.Threading.Tasks;
using UnityEngine;
using Debug = UnityEngine.Debug;

namespace UnityEngine
{
    public class SerializeField : Attribute { }
    public class HeaderAttribute : Attribute { public HeaderAttribute(string value) { } }
    public class MinAttribute : Attribute { public MinAttribute(float value) { } }
    public class RangeAttribute : Attribute { public RangeAttribute(float min, float max) { } }
    public struct Vector2Int { public int x, y; public Vector2Int(int x, int y) { this.x = x; this.y = y; } }
    public struct Vector3 { public float x, y, z; public Vector3(float x, float y, float z) { this.x = x; this.y = y; this.z = z; } }
    public struct Quaternion { public static Quaternion identity => new Quaternion(); }
    public class Coroutine { public IEnumerator Routine; }
    public class GameObject
    {
        public bool Active = true, Destroyed;
        public void SetActive(bool value) => Active = value;
        public bool TryGetComponent<T>(out T component) where T : new() { component = new T(); return true; }
    }
    public class MonoBehaviour
    {
        public object transform = new object();
        public static bool DisposeOnStop;
        public readonly List<Coroutine> Routines = new List<Coroutine>();
        public Coroutine StartCoroutine(IEnumerator routine)
        {
            var handle = new Coroutine { Routine = routine };
            if (routine.MoveNext()) Routines.Add(handle);
            return handle;
        }
        public void StopCoroutine(Coroutine handle)
        {
            Routines.Remove(handle);
            if (DisposeOnStop) (handle.Routine as IDisposable)?.Dispose();
        }
        public void Frame()
        {
            foreach (var handle in Routines.ToArray())
                if (!handle.Routine.MoveNext()) Routines.Remove(handle);
        }
        protected GameObject Instantiate(GameObject prefab, Vector3 offset, Quaternion rotation, object parent) => new GameObject();
        protected void Destroy(GameObject value) => value.Destroyed = true;
    }
    public static class Debug
    {
        public static int Errors;
        public static void LogError(object value) => Errors++;
        public static void LogException(Exception value) => Errors++;
    }
}
namespace UnityEngine.InputSystem
{
    public enum Key { R }
    public class KeyState { public bool wasPressedThisFrame; }
    public class Keyboard
    {
        public static Keyboard current { get; set; }
        private readonly KeyState rKey = new KeyState();
        public KeyState this[Key key] => rKey;
    }
}
public class Chunk
{
    public static readonly List<(int Size, float Scale, float Amplitude)> Meshes = new();
    public void generateMesh(float[] data, int x, int y, float scale, float amplitude)
    {
        Check.That(data.Length == x * y, "mesh buffer dimensions");
        Meshes.Add((x, scale, amplitude));
    }
}
static class Check
{
    public static void That(bool condition, string description) { if (!condition) throw new Exception(description); }
}
