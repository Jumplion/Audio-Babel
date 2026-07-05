# Index Algorithm Animation

A [Manim](https://www.manim.community/) explainer for the bijective-numeration
index algorithm described in `docs/INDEX_FORMAT.md`: how a PCM sample payload
is folded into a single big integer (and back), and how that integer is
rendered as a bijective base-64 string.

The scene only uses Pango-rendered `Text`/`Code` mobjects, so **no LaTeX
install is required**.

## Setup

```bash
python3 -m venv .venv
source .venv/bin/activate
pip install -r animations/requirements.txt
```

Manim also needs system Cairo/Pango and ffmpeg:

```bash
sudo apt-get install --no-install-recommends libcairo2-dev libpango1.0-dev pkg-config ffmpeg
```

## Render

```bash
cd animations
manim -qh index_algorithm.py IndexAlgorithm   # 1080p60 final
manim -ql index_algorithm.py IndexAlgorithm   # 480p15 fast draft
manim -pql index_algorithm.py IndexAlgorithm  # draft + auto-preview
```

Output is written under `animations/media/videos/index_algorithm/`.

## What it covers

1. Why plain positional (non-bijective) digit encoding collides on leading
   zero samples.
2. The fix: `digit = value + 1`, worked by hand with a small `B = 10` example
   (encode and decode, step by step).
3. Why each sample count owns an exclusive range ("band") of integers, so the
   payload length never needs to be stored separately.
4. How the real algorithm scales to `B = 65536` and the `n = V + S_L`
   closed-form identity that makes encode/decode `O(N)`.
5. The identical bijective-numeration trick applied a second time, in base 64,
   to turn the integer into a short URL-safe string.
