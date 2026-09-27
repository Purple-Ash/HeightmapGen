import onnxruntime
import numpy as np
from pathlib import Path
import sys
from dataset import HeightmapDataset
import matplotlib.pyplot as plt

parent_dir = Path(__file__).parent.resolve()
repo_root = parent_dir.parent.parent
viewer_dir = repo_root / "DatasetGenerator" / "src"
sys.path.insert(0, str(viewer_dir))
from viewer import visualize

def main():
    print(f"Parent directory: {parent_dir}")

    session = onnxruntime.InferenceSession(
        str(parent_dir / "models" / "eroded_heightmap.onnx"),
        providers=['CPUExecutionProvider']
    )

    dataset_dir = parent_dir.parent.parent / "DatasetGenerator" / "dataset"
    dataset = HeightmapDataset(Path(dataset_dir))
    print(f"Number of samples in dataset: {len(dataset)}")

    input_maps = []
    prediction_maps = []
    target_maps = []
    sample_numbers = list(range(min(len(dataset), 10)))

    for i in sample_numbers:
        input_tensor, target_tensor = dataset[i]
        input_tensor = np.expand_dims(input_tensor, axis=0).astype(np.float32)
        prediction = session.run(None, {"heightmap": input_tensor})[0]
 
        input_maps.append(input_tensor[0, 0])
        prediction_maps.append(prediction[0, 0])
        target_maps.append(target_tensor[0])

    visualize(
        [input_maps, prediction_maps, target_maps],
        ["Input", "Output", "Target"],
        sample_numbers
    )
    plt.show()

if __name__ == "__main__":
    main()
