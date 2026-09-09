#!/usr/bin/env python3
import argparse
import csv
import zlib
from pathlib import Path

import cv2
import numpy as np

from aria_gen2_pilot_dataset import AriaGen2PilotDataProvider
from projectaria_tools.core.sensor_data import TimeDomain, TimeQueryOptions

WIDTH = 640
HEIGHT = 480
FPS = 10
CLIP_SECONDS = 30
CLIP_COUNT = 10
FRAMES_PER_CLIP = FPS * CLIP_SECONDS
FRAME_BYTES = WIDTH * HEIGHT * 3
FILE_BYTES = FRAME_BYTES * FRAMES_PER_CLIP
FRAME_PERIOD_NS = int(1_000_000_000 / FPS)


def resize_letterbox_rgb(image: np.ndarray, width: int, height: int) -> np.ndarray:
    if image.ndim != 3 or image.shape[2] < 3:
        raise ValueError(f"Unsupported RGB image shape: {image.shape}")

    image = image[:, :, :3]
    if image.dtype != np.uint8:
        image = np.clip(image, 0, 255).astype(np.uint8)

    src_h, src_w = image.shape[:2]
    scale = min(width / src_w, height / src_h)
    dst_w = max(1, int(round(src_w * scale)))
    dst_h = max(1, int(round(src_h * scale)))

    resized = cv2.resize(
        image,
        (dst_w, dst_h),
        interpolation=cv2.INTER_AREA if scale < 1.0 else cv2.INTER_LINEAR,
    )

    canvas = np.zeros((height, width, 3), dtype=np.uint8)
    x0 = (width - dst_w) // 2
    y0 = (height - dst_h) // 2
    canvas[y0:y0 + dst_h, x0:x0 + dst_w] = resized
    return canvas


def main() -> None:
    parser = argparse.ArgumentParser(
        description="Convert Project Aria Gen2 video.vrs directly to RA8P1 RGB888 RAW clips."
    )
    parser.add_argument("--sequence", required=True, help="Sequence directory containing video.vrs.")
    parser.add_argument("--out", required=True, help="Output directory for VID00.RAW ... VID09.RAW.")
    args = parser.parse_args()

    sequence_dir = Path(args.sequence)
    output_dir = Path(args.out)

    vrs_path = sequence_dir / "video.vrs"
    if not vrs_path.is_file():
        raise FileNotFoundError(f"video.vrs not found: {vrs_path}")

    output_dir.mkdir(parents=True, exist_ok=True)

    print(f"[VRS] sequence: {sequence_dir}")
    print(f"[VRS] source:   {vrs_path}")

    provider = AriaGen2PilotDataProvider(str(sequence_dir))
    rgb_stream_id = provider.get_vrs_stream_id_from_label("camera-rgb")
    if rgb_stream_id is None:
        raise RuntimeError("camera-rgb stream was not found in video.vrs")

    timestamps = provider.get_vrs_timestamps_ns(rgb_stream_id, TimeDomain.DEVICE_TIME)
    if not timestamps:
        raise RuntimeError("camera-rgb stream contains no timestamps")

    first_ns = int(timestamps[0])
    last_ns = int(timestamps[-1])
    duration_s = (last_ns - first_ns) / 1e9

    required_last_target_ns = first_ns + (CLIP_COUNT * FRAMES_PER_CLIP - 1) * FRAME_PERIOD_NS

    print(f"[VRS] RGB samples: {len(timestamps)}")
    print(f"[VRS] duration:    {duration_s:.3f} s")
    print(f"[OUT] {WIDTH}x{HEIGHT} RGB888, {FPS} fps, {CLIP_SECONDS} s x {CLIP_COUNT} clips")
    print(f"[OUT] bytes/clip: {FILE_BYTES}")

    if last_ns < required_last_target_ns:
        available_s = (last_ns - first_ns) / 1e9
        required_s = (required_last_target_ns - first_ns) / 1e9
        raise RuntimeError(
            f"Sequence is too short: available={available_s:.3f}s, required through target={required_s:.3f}s"
        )

    manifest_rows = []
    global_frame = 0
    preview_written = False

    for clip_index in range(CLIP_COUNT):
        filename = f"VID{clip_index:02d}.RAW"
        output_path = output_dir / filename

        crc = 0
        duplicate_timestamp_count = 0
        previous_capture_ns = None

        with output_path.open("wb") as fp:
            for _frame_in_clip in range(FRAMES_PER_CLIP):
                target_ns = first_ns + global_frame * FRAME_PERIOD_NS
                image_data, image_record = provider.get_vrs_image_data_by_time_ns(
                    rgb_stream_id,
                    target_ns,
                    TimeDomain.DEVICE_TIME,
                    TimeQueryOptions.CLOSEST,
                )

                if not image_data.is_valid():
                    raise RuntimeError(f"Invalid RGB frame at target timestamp {target_ns}")

                capture_ns = int(image_record.capture_timestamp_ns)
                if previous_capture_ns == capture_ns:
                    duplicate_timestamp_count += 1
                previous_capture_ns = capture_ns

                rgb = image_data.to_numpy_array()
                rgb = resize_letterbox_rgb(rgb, WIDTH, HEIGHT)
                rgb = np.ascontiguousarray(rgb, dtype=np.uint8)

                frame_bytes = rgb.tobytes(order="C")
                if len(frame_bytes) != FRAME_BYTES:
                    raise RuntimeError(f"Unexpected frame byte count: {len(frame_bytes)}")

                fp.write(frame_bytes)
                crc = zlib.crc32(frame_bytes, crc)
                global_frame += 1

                if not preview_written:
                    preview_path = output_dir / "PREVIEW.JPG"
                    cv2.imwrite(str(preview_path), cv2.cvtColor(rgb, cv2.COLOR_RGB2BGR))
                    preview_written = True

        actual_size = output_path.stat().st_size
        if actual_size != FILE_BYTES:
            raise RuntimeError(
                f"{filename}: size mismatch: expected={FILE_BYTES}, actual={actual_size}"
            )

        crc &= 0xFFFFFFFF
        manifest_rows.append(
            {
                "index": clip_index,
                "file": filename,
                "width": WIDTH,
                "height": HEIGHT,
                "pixel_format": "RGB888",
                "fps": FPS,
                "frames": FRAMES_PER_CLIP,
                "bytes": actual_size,
                "crc32": f"{crc:08X}",
                "start_s": clip_index * CLIP_SECONDS,
                "end_s": (clip_index + 1) * CLIP_SECONDS,
            }
        )

        print(
            f"[OK] {filename}: bytes={actual_size} crc32=0x{crc:08X} "
            f"duplicate_queries={duplicate_timestamp_count}"
        )

    manifest_path = output_dir / "VIDEOS.CSV"
    with manifest_path.open("w", newline="", encoding="ascii") as fp:
        writer = csv.DictWriter(
            fp,
            fieldnames=[
                "index", "file", "width", "height", "pixel_format", "fps", "frames",
                "bytes", "crc32", "start_s", "end_s",
            ],
        )
        writer.writeheader()
        writer.writerows(manifest_rows)

    (output_dir / "SELECT.TXT").write_text("0\n", encoding="ascii")

    print()
    print(f"[DONE] output: {output_dir}")
    print(f"[DONE] manifest: {manifest_path}")
    print(f"[DONE] selector: {output_dir / 'SELECT.TXT'}")
    print(f"[DONE] preview:  {output_dir / 'PREVIEW.JPG'}")
    print("[DONE] Copy VID00.RAW..VID09.RAW and SELECT.TXT to the USB root for the next RA8P1 test.")


if __name__ == "__main__":
    main()