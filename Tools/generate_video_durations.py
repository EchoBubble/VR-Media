#!/usr/bin/env python3

import argparse
import json
import subprocess
from pathlib import Path


def get_duration(ffprobe: str, video_path: Path) -> float:
    result = subprocess.run(
        [
            ffprobe,
            "-v", "error",
            "-show_entries", "format=duration",
            "-of", "default=noprint_wrappers=1:nokey=1",
            str(video_path),
        ],
        capture_output=True,
        text=True,
        check=True,
    )

    duration = float(result.stdout.strip())

    if duration <= 0:
        raise RuntimeError(f"Invalid duration: {video_path}")

    return round(duration, 3)


def main():
    parser = argparse.ArgumentParser()

    parser.add_argument(
        "video_dir",
        help="视频所在目录，例如 D:/Project/Content/Movies"
    )

    parser.add_argument(
        "playlist_order",
        help="按 UE Playlist 顺序填写的视频文件名列表"
    )

    parser.add_argument(
        "--ffprobe",
        default="ffprobe",
        help="ffprobe.exe 地址"
    )

    args = parser.parse_args()

    video_dir = Path(args.video_dir)
    order_file = Path(args.playlist_order)

    names = [
        line.strip()
        for line in order_file.read_text(
            encoding="utf-8-sig"
        ).splitlines()
        if line.strip()
    ]

    durations = []

    for index, name in enumerate(names):
        video_path = video_dir / name

        if not video_path.is_file():
            raise SystemExit(
                f"Missing video: {video_path}"
            )

        duration = get_duration(
            args.ffprobe,
            video_path
        )

        durations.append(duration)

        print(
            f"[{index}] {name} = {duration:.3f}s"
        )

    print()
    print("JSON:")
    print(
        json.dumps(
            {
                "videoDurations": durations
            },
            ensure_ascii=False,
            indent=2
        )
    )

    print()
    print("DefaultGame.ini:")
    print("[VRMedia]")

    for duration in durations:
        print(
            f"+VideoDurations={duration:.3f}"
        )


if __name__ == "__main__":
    main()