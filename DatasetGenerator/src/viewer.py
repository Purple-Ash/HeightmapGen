
import argparse
import math
from pathlib import Path

import matplotlib.pyplot as plt
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


def visualize(image_groups, image_types, sample_numbers):
    figure, axes = plt.subplots(1, len(image_types), figsize=(4 * len(image_types), 4), constrained_layout=True)
    cmap = "terrain"
    first_maps = [group[0] for group in image_groups]
    images = [axis.imshow(heightmap, cmap=cmap) for axis, heightmap in zip(axes, first_maps)]

    for axis, image, label in zip(axes, images, image_types):
        axis.set_title(label)
        axis.set_xlabel("x")
        axis.set_ylabel("y")
        figure.colorbar(image, ax=axis, fraction=0.046, pad=0.04)

    def show_sample(index):
        maps = [group[index] for group in image_groups]
        minimum = min(float(heightmap.min()) for heightmap in maps)
        maximum = max(float(heightmap.max()) for heightmap in maps)
        for image, heightmap in zip(images, maps):
            image.set_data(heightmap)
            image.set_clim(minimum, maximum)
        figure.suptitle(
            f"Sample {sample_numbers[index]} ({index + 1}/{len(sample_numbers)}) - scroll to switch samples"
        )
        figure.canvas.draw_idle()

    current_index = 0

    def on_scroll(event):
        nonlocal current_index
        if event.button == "up":
            current_index = (current_index + 1) % len(image_groups[0])
        elif event.button == "down":
            current_index = (current_index - 1) % len(image_groups[0])
        else:
            return
        show_sample(current_index)

    figure.canvas.mpl_connect("scroll_event", on_scroll)
    show_sample(current_index)
    return figure


def parse_arguments():
    parent_dir = Path(__file__).parent.resolve()
    default_dataset = parent_dir.parent.parent / "DatasetGenerator" / "dataset"

    parser = argparse.ArgumentParser()
    parser.add_argument("-d", "--dataset", nargs="?", default=default_dataset, type=Path, help="Dataset directory containing in/ and out/")
    parser.add_argument("-s", "--sample", nargs="?", default="*", type=str, help="Sample index (for example 8) or range (for example 0-9)")
    return parser.parse_args()


def main() -> int:
    arguments = parse_arguments()

    try:
        samples = available_samples(arguments.dataset)

        selected_samples = []
        if "-" in arguments.sample:
            start, end = map(int, arguments.sample.split("-"))
            selected_samples = list(range(start, end + 1))
        elif arguments.sample == "*":
            selected_samples = [int(sample.stem) for sample in samples[:10]]
        else:
            selected_samples = [int(arguments.sample)]

        input_paths = [resolve_sample(samples, sample) for sample in selected_samples]
        output_paths = [arguments.dataset / "out" / input_path.name for input_path in input_paths]

        input_maps = []
        output_maps = []
        for input_path, output_path in zip(input_paths, output_paths):
            input_maps.append(read_heightmap(input_path))
            output_maps.append(read_heightmap(output_path))
        figure = visualize([input_maps, output_maps], ["Input", "Output"], selected_samples)
    except (OSError, ValueError) as error:
        print(f"{error}")
        return 1

    plt.show()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
