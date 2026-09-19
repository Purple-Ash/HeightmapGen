import torch
from torch.utils.data import Dataset
from pathlib import Path

class HeightmapDataset(Dataset):
    def __init__(self, dataset_dir: Path):
        input_dir = dataset_dir / "in"
        output_dir = dataset_dir / "out"
        if not input_dir.exists() or not output_dir.exists():
            raise ValueError("Input or output directory does not exist")
        
        self.dataset_files = [
            (input_path, output_path) 
            for input_path, output_path in zip(sorted(input_dir.glob("*.bin")), sorted(output_dir.glob("*.bin")))
        ]

        tensor_size = None

        for input_path, output_path in self.dataset_files:
            if input_path.stem != output_path.stem:
                raise ValueError(f"Filename mismatch: {input_path.name} and {output_path.name}")
            if tensor_size is None:
                tensor_size = input_path.stat().st_size
            elif input_path.stat().st_size != tensor_size or output_path.stat().st_size != tensor_size:
                raise ValueError(f"File size mismatch: {input_path.name} and {output_path.name}")

        element_size = torch.finfo(torch.float32).bits // 8
        self.num_elements = tensor_size // element_size
        edge_size = int(self.num_elements ** 0.5)
        self.tensor_shape = (1, edge_size, edge_size)

    def __len__(self):
        return len(self.dataset_files)

    def __getitem__(self, idx):
        input_path, output_path = self.dataset_files[idx]
        input_tensor = torch.from_file(str(input_path), size=self.num_elements, dtype=torch.float32)
        output_tensor = torch.from_file(str(output_path), size=self.num_elements, dtype=torch.float32)
        input_tensor = input_tensor.reshape(self.tensor_shape)
        output_tensor = output_tensor.reshape(self.tensor_shape)
        return input_tensor, output_tensor
