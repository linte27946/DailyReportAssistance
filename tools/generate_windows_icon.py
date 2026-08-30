"""Generate the multi-resolution Windows icon used by the executable.

The drawing mirrors src/ui/AppIcon.cpp so the taskbar, tray and executable
icons remain visually consistent. Requires Pillow.
"""

from pathlib import Path

from PIL import Image, ImageDraw


SIZES = (16, 24, 32, 48, 64, 128, 256)


def render(size: int) -> Image.Image:
    scale = 4
    canvas_size = size * scale
    image = Image.new("RGBA", (canvas_size, canvas_size), (0, 0, 0, 0))
    draw = ImageDraw.Draw(image)

    def box(values):
        return tuple(round(value * canvas_size / 128) for value in values)

    # Pillow does not support gradient rounded rectangles directly. Draw the
    # gradient first and use a rounded rectangle as its alpha mask.
    gradient = Image.new("RGBA", image.size)
    pixels = gradient.load()
    start = (91, 134, 247)
    end = (40, 87, 214)
    for y in range(canvas_size):
        for x in range(canvas_size):
            ratio = min(1.0, max(0.0, (x + y) / (2 * canvas_size - 2)))
            pixels[x, y] = tuple(
                round(start[channel] * (1 - ratio) + end[channel] * ratio)
                for channel in range(3)
            ) + (255,)
    mask = Image.new("L", image.size, 0)
    ImageDraw.Draw(mask).rounded_rectangle(
        box((5, 5, 123, 123)), radius=round(27 * canvas_size / 128), fill=255
    )
    image.alpha_composite(Image.composite(gradient, Image.new("RGBA", image.size), mask))

    draw = ImageDraw.Draw(image)
    draw.rounded_rectangle(
        box((28, 20, 100, 108)),
        radius=round(11 * canvas_size / 128),
        fill=(255, 255, 255, 245),
    )
    line_width = max(1, round(5 * canvas_size / 128))
    for x1, y1, x2, y2 in ((43, 44, 84, 44), (43, 59, 78, 59), (43, 74, 68, 74)):
        draw.line(box((x1, y1, x2, y2)), fill="#B8C8EA", width=line_width)

    draw.ellipse(box((66, 66, 116, 116)), fill="#23B985")
    draw.line(
        [box((79, 91))[0:2], box((88, 100))[0:2], box((104, 82))[0:2]],
        fill="white",
        width=max(1, round(6 * canvas_size / 128)),
        joint="curve",
    )
    return image.resize((size, size), Image.Resampling.LANCZOS)


def main() -> None:
    destination = Path(__file__).resolve().parents[1] / "packaging" / "windows" / "dailyreport.ico"
    destination.parent.mkdir(parents=True, exist_ok=True)
    images = [render(size) for size in SIZES]
    images[-1].save(destination, format="ICO", append_images=images[:-1], sizes=[(s, s) for s in SIZES])
    print(destination)


if __name__ == "__main__":
    main()
