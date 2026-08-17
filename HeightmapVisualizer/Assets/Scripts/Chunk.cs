using System.Collections.Generic;
using UnityEngine;

public class Chunk : MonoBehaviour
{
    public enum ColoringMode
    {
        None,
        Height
    }

    [SerializeField] private ColoringMode coloringMode = ColoringMode.Height;

    Color colorByHeight(Vector3 vertex)
    {
        return new Color(vertex.y / 2, 1.0f - vertex.y / 2, 0.0f, 1.0f);
    }

    Color colorVerticies(Vector3 vertex, ColoringMode mode, float amplitude)
    {
        return mode switch
        {
            ColoringMode.Height => colorByHeight(new Vector3(vertex.x, vertex.y / amplitude, vertex.z)),
            _ => new Color(1.0f, 1.0f, 1.0f, 1.0f)
        };
    }

    public void generateMesh(float[] newMesh, int sizeX, int sizeY, float scale, float amplitude)
    {
        Mesh mesh = new Mesh();
        mesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;

        int gridX = sizeX;
        int gridY = sizeY;
        int totalVertices = gridX * gridY;

        Vector3[] vertices = new Vector3[totalVertices];
        Color[] colors = new Color[totalVertices];
        Vector2[] UVs = new Vector2[totalVertices];
        List<int> triangles = new List<int>((gridX - 1) * (gridY - 1) * 6);

        for (int i = 0; i < gridX; i++)
        {
            for (int j = 0; j < gridY; j++)
            {
                int index = i * gridY + j;

                float height = (index < newMesh.Length) ? newMesh[index] : 0.0f;

                // Position vertices using the world scale factor
                Vector3 newVertex = new Vector3(i * scale, height, j * scale);

                vertices[index] = newVertex;
                colors[index] = colorVerticies(newVertex, coloringMode, amplitude);

                float u = (gridX > 1) ? (float)i / (gridX - 1) : 0.0f;
                float v = (gridY > 1) ? (float)j / (gridY - 1) : 0.0f;
                UVs[index] = new Vector2(u, v);

                if (i < gridX - 1 && j < gridY - 1)
                {
                    int current = i * gridY + j;
                    int right = (i + 1) * gridY + j;
                    int top = i * gridY + (j + 1);
                    int topRight = (i + 1) * gridY + (j + 1);

                    triangles.Add(current);
                    triangles.Add(top);
                    triangles.Add(right);

                    triangles.Add(right);
                    triangles.Add(top);
                    triangles.Add(topRight);
                }
            }
        }

        mesh.vertices = vertices;
        mesh.colors = colors;
        mesh.uv = UVs;
        mesh.triangles = triangles.ToArray();

        mesh.RecalculateNormals();
        mesh.RecalculateBounds();

        if (TryGetComponent<MeshFilter>(out var filter))
        {
            filter.mesh = mesh;
        }

        if (TryGetComponent<MeshCollider>(out var collider))
        {
            collider.sharedMesh = mesh;
        }
    }
}