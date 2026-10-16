#!/usr/bin/env python3
import json
import subprocess


def run(*args):
    return subprocess.run(args, capture_output=True, text=True)


status = run("playerctl", "status")
if status.returncode != 0:
    raise SystemExit

state = status.stdout.strip()
icons = {"Playing": "󰐊", "Paused": "󰏤"}
icon = icons.get(state)
if icon is None:
    raise SystemExit

meta = run("playerctl", "metadata", "--format", "{{artist}}\t{{title}}")
artist, _, title = meta.stdout.strip().partition("\t")
tip = " - ".join(part for part in (artist, title) if part)
print(json.dumps({"text": icon, "tooltip": tip, "class": state.lower()}, ensure_ascii=False))
