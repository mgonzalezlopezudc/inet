#!/usr/bin/env python3

import ctypes
import os
from pathlib import Path
import subprocess
import sys
import time


def find_qtenv_window():
    output = subprocess.check_output(
        ["xwininfo", "-root", "-tree"], text=True, errors="replace"
    )
    for line in output.splitlines():
        if "OMNeT++/Qtenv" in line and '("Qtenv" "Qtenv")' in line:
            token = line.strip().split()[0]
            if token.startswith("0x"):
                return int(token, 16)
    return None


class X11Keys:
    def __init__(self):
        self.x11 = ctypes.CDLL("libX11.so.6")
        self.xtst = ctypes.CDLL("libXtst.so.6")
        self.x11.XOpenDisplay.restype = ctypes.c_void_p
        self.display = self.x11.XOpenDisplay(None)
        if not self.display:
            raise RuntimeError("Cannot open X display")

    def focus(self, window):
        self.x11.XRaiseWindow(self.display, ctypes.c_ulong(window))
        self.x11.XSetInputFocus(self.display, ctypes.c_ulong(window), 2, 0)
        self.x11.XFlush(self.display)
        time.sleep(0.2)

    def key(self, keysym):
        keycode = self.x11.XKeysymToKeycode(self.display, keysym)
        self.xtst.XTestFakeKeyEvent(self.display, keycode, 1, 0)
        self.xtst.XTestFakeKeyEvent(self.display, keycode, 0, 0)
        self.x11.XFlush(self.display)

    def ctrl_q(self):
        control = self.x11.XKeysymToKeycode(self.display, 0xFFE3)
        q_key = self.x11.XKeysymToKeycode(self.display, ord("q"))
        self.xtst.XTestFakeKeyEvent(self.display, control, 1, 0)
        self.xtst.XTestFakeKeyEvent(self.display, q_key, 1, 0)
        self.xtst.XTestFakeKeyEvent(self.display, q_key, 0, 0)
        self.xtst.XTestFakeKeyEvent(self.display, control, 0, 0)
        self.x11.XFlush(self.display)

    def express_run(self):
        self.key(0xFFC4)  # F7

    def accept_dialog(self):
        self.key(0xFF0D)  # Return


def wait_until(predicate, timeout, interval=0.25):
    deadline = time.monotonic() + timeout
    while time.monotonic() < deadline:
        value = predicate()
        if value:
            return value
        time.sleep(interval)
    return None


def main():
    if len(sys.argv) != 5:
        raise SystemExit(
            "usage: run_qtenv_review.py MODE CONFIG RESULT_DIR LOG\n"
            "example: run_qtenv_review.py release MixedUora "
            "/tmp/qtenv-results /tmp/qtenv.log"
        )

    mode, config, result_dir, log_name = sys.argv[1:]
    if mode not in ("release", "debug"):
        raise SystemExit("MODE must be 'release' or 'debug'")

    result_path = Path(result_dir)
    result_path.mkdir(parents=True, exist_ok=True)
    for old_file in result_path.glob("*"):
        if old_file.is_file():
            old_file.unlink()

    env = os.environ.copy()
    env["CCACHE_DISABLE"] = "1"
    command = [
        "inet",
        f"--{mode}",
        "-u",
        "Qtenv",
        "-c",
        config,
        "-r",
        "0",
        f"--result-dir={result_dir}",
    ]

    with open(log_name, "w") as log:
        process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT, env=env)
        try:
            window = wait_until(find_qtenv_window, 60)
            if not window:
                raise RuntimeError("Qtenv window did not appear")

            keys = X11Keys()
            scalar_file = result_path / f"{config}-#0.sca"

            time.sleep(2)
            keys.focus(window)
            keys.express_run()

            # Qtenv reports normal simulation termination in a modal dialog.
            # These short review cases finish within this interval in both modes.
            time.sleep(10)
            keys.accept_dialog()

            if not wait_until(scalar_file.exists, 20):
                raise RuntimeError("Qtenv simulation did not finish")

            keys.focus(window)
            keys.ctrl_q()
            return_code = process.wait(timeout=15)
            if return_code != 0:
                raise RuntimeError(f"Qtenv exited with status {return_code}")
        except Exception:
            process.terminate()
            try:
                process.wait(timeout=5)
            except subprocess.TimeoutExpired:
                process.kill()
            raise


if __name__ == "__main__":
    main()
