#!/usr/bin/env python3
"""Fetch pinned open-source fonts and licenses for IMG Studio into assets/fonts/."""

import hashlib
import os
import shutil
import urllib.request

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONTS_DIR = os.path.join(ROOT, "assets", "fonts")

# Pinned URLs with upstream release tags/commits
ASSETS = [
    {
        "filename": "Inter-Regular.ttf",
        "url": "https://raw.githubusercontent.com/rsms/inter/v4.0/docs/font-files/Inter-Regular.ttf",
        "fallback_urls": [
            "https://github.com/google/fonts/raw/main/ofl/inter/Inter%5Bopsz%2Cwght%5D.ttf",
        ],
        "linux_fallback": "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    },
    {
        "filename": "Inter-SemiBold.ttf",
        "url": "https://raw.githubusercontent.com/rsms/inter/v4.0/docs/font-files/Inter-SemiBold.ttf",
        "fallback_urls": [
            "https://github.com/google/fonts/raw/main/ofl/inter/Inter%5Bopsz%2Cwght%5D.ttf",
        ],
        "linux_fallback": "/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf",
    },
    {
        "filename": "JetBrainsMono-Regular.ttf",
        "url": "https://raw.githubusercontent.com/JetBrains/JetBrainsMono/v2.304/fonts/ttf/JetBrainsMono-Regular.ttf",
        "fallback_urls": [],
        "linux_fallback": "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    },
    {
        "filename": "fa-solid-900.ttf",
        "url": "https://raw.githubusercontent.com/FortAwesome/Font-Awesome/6.7.2/webfonts/fa-solid-900.ttf",
        "fallback_urls": [],
        "linux_fallback": "/usr/share/fonts/truetype/font-awesome/fontawesome-webfont.ttf",
    },
    {
        "filename": "Inter-OFL.txt",
        "url": "https://raw.githubusercontent.com/rsms/inter/v4.0/LICENSE.txt",
        "fallback_urls": [],
        "linux_fallback": None,
    },
    {
        "filename": "JetBrainsMono-OFL.txt",
        "url": "https://raw.githubusercontent.com/JetBrains/JetBrainsMono/v2.304/OFL.txt",
        "fallback_urls": [],
        "linux_fallback": None,
    },
    {
        "filename": "FontAwesome-LICENSE.txt",
        "url": "https://raw.githubusercontent.com/FortAwesome/Font-Awesome/6.7.2/LICENSE.txt",
        "fallback_urls": [],
        "linux_fallback": None,
    },
]


def download(url: str, dest: str) -> bool:
    try:
        req = urllib.request.Request(url, headers={"User-Agent": "IMGStudio-Build/1.0"})
        with urllib.request.urlopen(req, timeout=20) as resp:
            data = resp.read()
        if len(data) < 256:
            return False
        with open(dest, "wb") as f:
            f.write(data)
        return True
    except Exception as exc:
        print(f"Warning: failed downloading {url}: {exc}")
        return False


def main() -> None:
    os.makedirs(FONTS_DIR, exist_ok=True)
    for item in ASSETS:
        dest = os.path.join(FONTS_DIR, item["filename"])
        if os.path.exists(dest) and os.path.getsize(dest) > 256:
            print(f"Already present: {item['filename']}")
            continue

        urls = [item["url"]] + item["fallback_urls"]
        ok = False
        for u in urls:
            if download(u, dest):
                h = hashlib.sha256(open(dest, "rb").read()).hexdigest()[:16]
                print(f"Downloaded {item['filename']} ({os.path.getsize(dest)} bytes, sha256:{h}...)")
                ok = True
                break

        if not ok and item["linux_fallback"] and os.path.exists(item["linux_fallback"]):
            shutil.copy2(item["linux_fallback"], dest)
            print(f"Used local fallback for {item['filename']} from {item['linux_fallback']}")
        elif not ok:
            print(f"Note: could not obtain {item['filename']}")


if __name__ == "__main__":
    main()
