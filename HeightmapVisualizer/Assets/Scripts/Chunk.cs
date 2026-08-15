using System.Collections.Generic;
using UnityEngine;

public class Chunk : MonoBehaviour
{
    public enum ColoringMode
    {
        None,
        Height
    }

    Color colorByHeight(Vector3 vertex)
    {
        return new Color(vertex.y/2, 1.0f-vertex.y/2, 0.0f,1.0f);
    }

    Color colorVerticies(Vector3 vertex,ColoringMode mode, float amplitude)
    {
        return mode switch
        {
            ColoringMode.Height => colorByHeight(new Vector3(vertex.x,vertex.y/amplitude,vertex.z)),
            _ => new Color(1.0f, 1.0f, 1.0f, 1.0f)
        };
    }

    public void generateMesh(float[] newMesh, int sizeX, int sizeY, int resolution, float amplitude)
    {
        Mesh mesh = new Mesh();
        mesh.indexFormat = UnityEngine.Rendering.IndexFormat.UInt32;
        int actualSizeX = sizeX * resolution + 1;
        int actualSizeY = sizeY * resolution + 1;
        Vector3[] vertices = new Vector3[actualSizeX * actualSizeY];
        Color[] colors = new Color[actualSizeX * actualSizeY];
        Vector2[] UVs = new Vector2[actualSizeX * actualSizeY];
        List<int> triangles = new List<int>();
        for (int i = 0; i < actualSizeX; i++)
        {
            for (int j = 0; j < actualSizeY; j++)
            {
                Vector3 newVertex = new Vector3(((float)i)/resolution, newMesh[i * actualSizeY + j], ((float)j) / resolution);
                vertices[i * actualSizeY + j] = newVertex;
                colors[i * actualSizeY + j] = colorVerticies(newVertex, ColoringMode.Height, amplitude);
                UVs[i * actualSizeY + j] = new Vector2(i / actualSizeX, j / actualSizeY);
                if (i + 1 < actualSizeX && j + 1 < actualSizeY)
                {
                    triangles.Add(i * actualSizeY + j);
                    triangles.Add(i * actualSizeY + j + 1);
                    triangles.Add((i + 1) * actualSizeY + j);
                }
                if (i > 0 && j > 0)
                {
                    triangles.Add(i * actualSizeY + j);
                    triangles.Add(i * actualSizeY + j - 1);
                    triangles.Add((i - 1) * actualSizeY + j);
                }
            }
        }
        mesh.vertices = vertices;
        mesh.colors = colors;
        mesh.triangles = triangles.ToArray();

        mesh.RecalculateNormals();

        GetComponent<MeshFilter>().mesh = mesh;
    }
}
