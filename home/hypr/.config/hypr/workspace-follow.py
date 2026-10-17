#!/usr/bin/env python3

import json
import os
import signal
import socket
import time
from pathlib import Path

EXT = "ext-"

runtime = Path(os.environ["XDG_RUNTIME_DIR"])
base = runtime / "hypr" / os.environ["HYPRLAND_INSTANCE_SIGNATURE"]
cmd_path = str(base / ".socket.sock")

laptop_name = None
external_name = None
laptop_ws = None
focused_name = None
last_main = None
place = {}
where = {}


def call(cmd):
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(cmd_path)
    try:
        sock.sendall(cmd.encode())
        chunks = []
        while True:
            data = sock.recv(65536)
            if not data:
                break
            chunks.append(data)
    finally:
        sock.close()
    return b"".join(chunks).decode()


def query(cmd):
    return json.loads(call(cmd))


def batch(cmds):
    if cmds:
        call("[[BATCH]]" + ";".join(cmds))


def dsp(expr):
    return f"dispatch {expr}"


def claim():
    pidfile = runtime / "hypr-workspace-follow.pid"
    if pidfile.exists():
        try:
            old = int(pidfile.read_text().strip())
        except ValueError:
            old = 0
        if old and old != os.getpid():
            try:
                os.kill(old, signal.SIGTERM)
            except ProcessLookupError:
                old = 0
            if old:
                for _ in range(20):
                    try:
                        os.kill(old, 0)
                    except ProcessLookupError:
                        break
                    time.sleep(0.05)
    pidfile.write_text(str(os.getpid()))

    def cleanup(*_args):
        if pidfile.exists() and pidfile.read_text().strip() == str(os.getpid()):
            pidfile.unlink()
        os._exit(0)

    signal.signal(signal.SIGTERM, cleanup)


def classify(mons):
    laptop = next((mon for mon in mons if mon["name"].startswith("eDP")), None)
    if laptop is None:
        return None, None
    externals = [mon for mon in mons if mon["name"] != laptop["name"]]
    if not externals:
        return laptop["name"], None
    chosen = next((mon for mon in externals if mon.get("focused")), externals[0])
    return laptop["name"], chosen["name"]


def snapshot(mons, spaces):
    global laptop_name, external_name, laptop_ws, focused_name, last_main
    laptop_name, external_name = classify(mons)
    for mon in mons:
        if mon.get("focused"):
            focused_name = mon["name"]
        if laptop_name and mon["name"] == laptop_name:
            laptop_ws = mon["activeWorkspace"]["name"]
        if external_name and mon["name"] == external_name:
            active = mon["activeWorkspace"]
            if active["id"] > 0:
                last_main = active["id"]
    place.clear()
    for ws in spaces:
        if ws["id"] > 0:
            place[ws["id"]] = ws["monitor"]


def follow(ws_id):
    global laptop_ws, last_main
    if not external_name or not laptop_name or ws_id <= 0:
        return
    last_main = ws_id
    side = f"{EXT}{ws_id}"
    known = place.get(ws_id)
    if known == external_name:
        need_move = False
    elif known is not None:
        need_move = True
    elif focused_name == external_name:
        place[ws_id] = external_name
        need_move = False
    else:
        need_move = True
    if not need_move and laptop_ws == side:
        return
    cmds = []
    switched = laptop_ws != side
    if switched:
        cmds.append(dsp(f'hl.dsp.focus({{ monitor = "{laptop_name}" }})'))
        cmds.append(dsp(f'hl.dsp.focus({{ workspace = "name:{side}" }})'))
        laptop_ws = side
    if need_move:
        cmds.append(dsp(f'hl.dsp.workspace.move({{ workspace = "{ws_id}", monitor = "{external_name}" }})'))
        place[ws_id] = external_name
    if switched:
        cmds.append(dsp(f'hl.dsp.focus({{ monitor = "{external_name}" }})'))
    batch(cmds)


def pull_to_external():
    cmds = []
    for ws in query("j/workspaces"):
        if ws["id"] > 0 and ws["monitor"] != external_name:
            cmds.append(dsp(f'hl.dsp.workspace.move({{ workspace = "{ws["id"]}", monitor = "{external_name}" }})'))
            place[ws["id"]] = external_name
    batch(cmds)


def undock():
    cmds = []
    for client in query("j/clients"):
        name = str(client.get("workspace", {}).get("name", ""))
        if not name.startswith(EXT):
            continue
        suffix = name.removeprefix(EXT)
        if suffix.isdigit():
            address = client["address"]
            cmds.append(dsp(f'hl.dsp.window.move({{ workspace = "{suffix}", follow = false, window = "address:{address}" }})'))
    if last_main:
        cmds.append(dsp(f'hl.dsp.focus({{ workspace = "{last_main}" }})'))
    batch(cmds)


def norm_addr(addr):
    addr = str(addr).lower()
    if addr.startswith("0x"):
        return addr
    return "0x" + addr


def note(addr, ws_id):
    key = norm_addr(addr)
    if ws_id > 0:
        where[key] = ws_id
    else:
        where.pop(key, None)


def remember():
    where.clear()
    for client in query("j/clients"):
        ws = client.get("workspace") or {}
        ws_id = ws.get("id") or 0
        name = str(ws.get("name") or "")
        if ws_id > 0 and not name.startswith(EXT):
            where[norm_addr(client["address"])] = ws_id


def occupied():
    groups = {}
    for client in query("j/clients"):
        if not client.get("mapped"):
            continue
        ws = client.get("workspace") or {}
        ws_id = ws.get("id") or 0
        name = str(ws.get("name") or "")
        if ws_id <= 0 or name.startswith(EXT):
            continue
        groups.setdefault(ws_id, []).append(client)
    return groups


def fill_gap(closed_id):
    groups = occupied()
    if groups.get(closed_id):
        return
    higher = sorted(ws_id for ws_id in groups if ws_id > closed_id)
    cmds = []
    if higher:
        for index, ws_id in enumerate(higher):
            dest = closed_id + index
            for client in groups[ws_id]:
                if dest == ws_id:
                    continue
                addr = client["address"]
                cmds.append(dsp(f'hl.dsp.window.move({{ workspace = "{dest}", follow = false, window = "address:{addr}" }})'))
                where[norm_addr(addr)] = dest
        cmds.append(dsp(f'hl.dsp.focus({{ workspace = "{closed_id}" }})'))
    else:
        lower = [ws_id for ws_id in groups if ws_id < closed_id]
        if lower:
            cmds.append(dsp(f'hl.dsp.focus({{ workspace = "{max(lower)}" }})'))
    batch(cmds)


def rescan():
    had_external = external_name
    snapshot(query("j/monitors"), query("j/workspaces"))
    if external_name:
        if last_main:
            follow(last_main)
        pull_to_external()
    elif had_external:
        undock()
    remember()


def handle(line):
    global focused_name, laptop_ws
    if line.startswith("workspacev2>>"):
        ws_id_s, name = line.split(">>", 1)[1].split(",", 1)
        ws_id = int(ws_id_s)
        if name.startswith(EXT):
            laptop_ws = name
            return
        if ws_id > 0:
            follow(ws_id)
        return
    if line.startswith("moveworkspacev2>>"):
        ws_id_s, _name, mon = line.split(">>", 1)[1].split(",", 2)
        ws_id = int(ws_id_s)
        if ws_id > 0:
            place[ws_id] = mon
        return
    if line.startswith("focusedmonv2>>"):
        focused_name = line.split(">>", 1)[1].split(",", 1)[0]
        return
    if line.startswith("monitoradded>>") or line.startswith("monitorremoved>>"):
        rescan()
        return
    if line.startswith("openwindow>>"):
        addr, ws_name, _rest = line.split(">>", 1)[1].split(",", 2)
        note(addr, int(ws_name) if ws_name.isdigit() else 0)
        return
    if line.startswith("movewindowv2>>"):
        addr, ws_id_s, ws_name = line.split(">>", 1)[1].split(",", 2)
        ws_id = int(ws_id_s) if ws_id_s.lstrip("-").isdigit() else 0
        if ws_name.startswith(EXT):
            ws_id = 0
        note(addr, ws_id)
        return
    if line.startswith("movewindow>>"):
        addr, ws_name = line.split(">>", 1)[1].split(",", 1)
        note(addr, int(ws_name) if ws_name.isdigit() else 0)
        return
    if line.startswith("closewindow>>"):
        ws_id = where.pop(norm_addr(line.split(">>", 1)[1]), None)
        if ws_id:
            fill_gap(ws_id)


def main():
    claim()
    rescan()
    sock = socket.socket(socket.AF_UNIX, socket.SOCK_STREAM)
    sock.connect(str(base / ".socket2.sock"))
    try:
        with sock.makefile("r", encoding="utf-8", newline="\n") as events:
            for line in events:
                handle(line.rstrip("\n"))
    finally:
        sock.close()


if __name__ == "__main__":
    main()
