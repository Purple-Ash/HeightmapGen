from pathlib import Path
from dataset import HeightmapDataset
import torch
from torch.utils.data import DataLoader, random_split
from torch import nn
import torch.onnx

class ErosionCNN(nn.Module):
    def __init__(self):
        super(ErosionCNN, self).__init__()
        self.conv1 = nn.Conv2d(1, 32, kernel_size=3, padding=1)
        self.conv2 = nn.Conv2d(32, 32, kernel_size=3, padding=1)
        self.conv3 = nn.Conv2d(32, 1, kernel_size=3, padding=1)
        self.activation = nn.ReLU()

    def forward(self, x):
        x = self.activation(self.conv1(x))
        x = self.activation(self.conv2(x))
        x = self.conv3(x)
        return x

def export_onnx(model, file_path):
    if not file_path.parent.exists():
        print(f"Creating directory: {file_path.parent}")
        file_path.parent.mkdir(parents=False, exist_ok=True)

    model.cpu().eval()
    example_input = torch.rand(1, 1, 128, 128, dtype=torch.float32)
    torch.onnx.export(model, example_input, file_path, 
                      input_names=['heightmap'], output_names=['eroded_heightmap'], 
                      dynamo=True)
    
    print(f"Model exported to {file_path}")

def main():
    parent_dir = Path(__file__).parent.resolve()
    dataset_dir = parent_dir.parent.parent / "DatasetGenerator" / "dataset"

    dataset_full = HeightmapDataset(dataset_dir)
    dataset_train, dataset_val = random_split(dataset_full, [0.8, 0.2])
    dataloader_train = DataLoader(dataset_train, batch_size=4, shuffle=True)
    dataloader_val = DataLoader(dataset_val, batch_size=4, shuffle=False)

    print(f"Number of training samples: {len(dataset_train)}")
    print(f"Number of validation samples: {len(dataset_val)}")
    print(f"Number of training batches: {len(dataloader_train)}")
    print(f"Number of validation batches: {len(dataloader_val)}")
    print(f"Tensor shape: {dataset_train[0][0].shape}")

    device = torch.device("cuda" if torch.cuda.is_available() else "cpu")
    model = ErosionCNN().to(device)
    print(model)

    criterion = nn.MSELoss()
    optimizer = torch.optim.Adam(model.parameters(), lr=0.001)

    num_epochs = 10
    for epoch in range(num_epochs):
        model.train()
        for input_tensor, output_tensor in dataloader_train:
            input_tensor, output_tensor = input_tensor.to(device), output_tensor.to(device)
            optimizer.zero_grad()
            predictions = model(input_tensor)
            loss = criterion(predictions, output_tensor)
            loss.backward()
            optimizer.step()

        model.eval()
        val_loss = 0.0
        with torch.no_grad():
            for input_tensor, output_tensor in dataloader_val:
                input_tensor, output_tensor = input_tensor.to(device), output_tensor.to(device)
                predictions = model(input_tensor)
                loss = criterion(predictions, output_tensor)
                val_loss += loss.item()
        val_loss /= len(dataloader_val)
        print(f"Epoch {epoch+1}/{num_epochs}, Validation Loss: {val_loss}")

    model_export_path = parent_dir / "models" / "eroded_heightmap.onnx"
    export_onnx(model, model_export_path)

if __name__ == "__main__":
    main()
