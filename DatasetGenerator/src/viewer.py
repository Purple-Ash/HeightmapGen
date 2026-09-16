
import argparse
import math
import sys
from pathlib import Path

import matplotlib.pyplot as plt
from matplotlib.figure import Figure
import numpy as np


def read_heightmap(path: Path):
    values = np.fromfile(path, dtype=np.float32)
    if values.size == 0:
        raise ValueError("file is empty")
    side = math.isqrt(values.size)
    if side * side != values.size:
        raise ValueError("file size is not a perfect square")
    return values.reshape((side, side))


def available_samples(dataset: Path):
    input_dir = dataset / "in"
    output_dir = dataset / "out"
    if not input_dir.is_dir() or not output_dir.is_dir():
        raise ValueError("dataset must contain both 'in' and 'out' directories")

    matching_paths = [
        path for path in input_dir.glob("*.bin") 
        if (output_dir / path.name).is_file()
    ]
    samples = sorted(matching_paths)
    if not samples:
        raise ValueError("no matching .bin files found")
    return samples


def resolve_sample(samples: list[Path], requested: int):
    name = f"{requested:06}.bin"
    for sample in samples:
        if sample.name == name:
            return sample
    raise ValueError(f"sample '{requested}' was not found")


def visualize(input_maps, output_maps, title):
    minimum = [min(float(input_map.min()), float(output_map.min())) for input_map, output_map in zip(input_maps, output_maps)]
    maximum = [max(float(input_map.max()), float(output_map.max())) for input_map, output_map in zip(input_maps, output_maps)]

    img_count = len(input_maps)
    figure, axes = plt.subplots(img_count, 2, figsize=(8, 4 * img_count), constrained_layout=True)
    if img_count == 1:
        axes = np.array([axes])
    figure.suptitle(title)

    cmap = "terrain"
    labels = ("Input", "Output")
    for i, (input_map, output_map) in enumerate(zip(input_maps, output_maps)):
        images = (
            axes[i, 0].imshow(input_map, cmap=cmap, vmin=minimum[i], vmax=maximum[i]),
            axes[i, 1].imshow(output_map, cmap=cmap, vmin=minimum[i], vmax=maximum[i]),
        )
        for axis, image, label in zip(axes[i], images, labels):
            axis.set_title(label)
            axis.set_xlabel("x")
            axis.set_ylabel("y")
            figure.colorbar(image, ax=axis, fraction=0.046, pad=0.04)
    return figure


def parse_arguments():
    parser = argparse.ArgumentParser()
    parser.add_argument("dataset", type=Path, help="Dataset directory containing in/ and out/")
    parser.add_argument("sample", nargs="?", default="0", type=str, help="Sample index (for example 12) or range (for example 2-4)")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()

    selected_samples = []
    if "-" in arguments.sample:
        start, end = map(int, arguments.sample.split("-"))
        selected_samples = list(range(start, end + 1))
    else:
        selected_samples = [int(arguments.sample)]

    try:
        samples = available_samples(arguments.dataset)
        input_paths = [resolve_sample(samples, sample) for sample in selected_samples]
        output_paths = [arguments.dataset / "out" / input_path.name for input_path in input_paths]

        input_maps = []
        output_maps = []
        for input_path, output_path in zip(input_paths, output_paths):
            input_maps.append(read_heightmap(input_path))
            output_maps.append(read_heightmap(output_path))
        figure = visualize(input_maps, output_maps, f"Samples {arguments.sample}")
    except (OSError, ValueError) as error:
        print(f"{error}")
        return 1

    plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
