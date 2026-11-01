import math
import torch
from torch.utils.data import Dataset
from pathlib import Path

class HeightmapDataset(Dataset):
    def __init__(self, dataset_dir: Path):
        input_dir = dataset_dir / "in"
        output_dir = dataset_dir / "out"
        if not input_dir.exists() or not output_dir.exists():
            raise ValueError("Input or output directory does not exist")

        input_files = {path.stem: path for path in input_dir.glob("*.bin")}
        output_files = {path.stem: path for path in output_dir.glob("*.bin")}
        if input_files.keys() != output_files.keys():
            raise ValueError("Input/output sample mismatch")
        if not input_files:
            raise ValueError("Dataset does not contain any .bin samples")

        self.dataset_files = [
            (input_files[name], output_files[name]) for name in sorted(input_files)
        ]
        self.input_shape = None
        self.output_shape = None

        for input_path, output_path in self.dataset_files:
            input_shape = self._tensor_shape(input_path)
            output_shape = self._tensor_shape(output_path)
            if input_shape[1] < output_shape[1] or input_shape[2] < output_shape[2]:
                raise ValueError(f"Input image must be at least as large as output image: {input_path.name}")
            if self.input_shape is None:
                self.input_shape = input_shape
                self.output_shape = output_shape
            elif input_shape != self.input_shape or output_shape != self.output_shape:
                raise ValueError(f"File size mismatch: {input_path.name}")

        self.input_num_elements = math.prod(self.input_shape)
        self.output_num_elements = math.prod(self.output_shape)

    def _tensor_shape(self, path):
        element_size = torch.finfo(torch.float32).bits // 8
        file_size = path.stat().st_size
        if file_size == 0 or file_size % element_size != 0:
            raise ValueError(f"Invalid heightmap size: {path.name}")
        num_elements = file_size // element_size
        edge_size = math.isqrt(num_elements)
        return (1, edge_size, edge_size)

    def __len__(self):
        return len(self.dataset_files)

    def __getitem__(self, idx):
        input_path, output_path = self.dataset_files[idx]
        input_tensor = torch.from_file(str(input_path), size=self.input_num_elements, dtype=torch.float32)
        output_tensor = torch.from_file(str(output_path), size=self.output_num_elements, dtype=torch.float32)
        input_tensor = input_tensor.reshape(self.input_shape)
        output_tensor = output_tensor.reshape(self.output_shape)
        return input_tensor, output_tensor
