#!/usr/bin/env python3
from pathlib import Path

readme_path = Path("README.md")
image_block = "![Brown Help plugin UI](docs/plugin-ui.png)\n\n"
anchor = "Brown Help is a VST3 tonal balancer for podcasts and voice-over work by **D35P Audio**.\n\n"

text = readme_path.read_text()

if image_block not in text:
    if anchor not in text:
        raise SystemExit("README anchor not found")

    text = text.replace(anchor, anchor + image_block, 1)
    readme_path.write_text(text)
