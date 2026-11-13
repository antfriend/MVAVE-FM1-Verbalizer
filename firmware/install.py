#!/usr/bin/env python3
"""Put the Verbalizer into a checkout of M-VAVE's open FM-1 firmware.

    python firmware/install.py [path to fm1_open]      (default: ../fm1_open)

It changes only the checkout's `app` folder:

  1. copies the engine (engine/*.c, *.h) and the glue (firmware/fm1_verbalizer.*)
     into app/verbalizer/, where the SDK's build compiles every .c file;
  2. applies firmware/fm1_open.patch, which hooks the audio interrupt and the
     MIDI router and makes the factory DX7 bank decoder stop early;
  3. cuts the packed factory DX7 bank down to its first 16 voices, because the
     program image has no room for the Verbalizer otherwise.

Running it again refreshes the engine copy and leaves the rest alone. Then
build with the SDK:

    powershell -ExecutionPolicy Bypass -File sdk\\Build.ps1 -FirmwareOnly -Toolchain C:\\JL\\pi32

The patch was made against fm1_open commit 4c7496d (V14).
"""
import glob
import os
import re
import shutil
import subprocess
import sys

EXPECTED_COMMIT = "4c7496d"
KEPT_VOICES = 16
MARKER = "Verbalizer build: cut down to the first"

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)


def git(sdk, *args, check=True):
    return subprocess.run(["git", "-C", sdk] + list(args), check=check,
                          capture_output=True, text=True)


def copy_sources(sdk):
    target = os.path.join(sdk, "app", "verbalizer")
    os.makedirs(target, exist_ok=True)
    files = (glob.glob(os.path.join(REPO, "engine", "*.[ch]")) +
             glob.glob(os.path.join(HERE, "fm1_verbalizer.[ch]")))
    for path in files:
        shutil.copy2(path, target)
    print("copied %d source files to %s" % (len(files), target))


def apply_patch(sdk):
    patch = os.path.join(HERE, "fm1_open.patch")
    if git(sdk, "apply", "--reverse", "--check", patch, check=False).returncode == 0:
        print("patch already applied")
        return
    result = git(sdk, "apply", "--check", patch, check=False)
    if result.returncode != 0:
        sys.exit("fm1_open.patch does not apply to this checkout:\n" + result.stderr)
    git(sdk, "apply", patch)
    print("applied fm1_open.patch")


def trim_factory_bank(sdk):
    path = os.path.join(sdk, "app", "fm_factory.inc")
    text = open(path, encoding="utf-8", newline="").read()
    if MARKER in text:
        print("factory DX7 bank already trimmed")
        return
    start, end = text.index("{"), text.rindex("}")
    values = [int(v) for v in re.findall(r"\d+", text[start + 1:end])]
    # walk the packed stream until it has produced the voices we keep
    produced = at = 0
    while produced < KEPT_VOICES * 128:
        token = values[at]
        at += 1
        if token < 128:
            produced += 1
        else:
            at += 2 if values[at] & 128 else 1
            produced += token - 128 + 3
    kept = values[:at]
    rows = ["    " + ",".join(str(v) for v in kept[i:i + 24]) + ","
            for i in range(0, len(kept), 24)]
    head = text[:start + 1].replace(
        " * Source and license",
        " * %s %d voices to make room.\n * Source and license" % (MARKER, KEPT_VOICES))
    if MARKER not in head:
        sys.exit("fm_factory.inc has an unexpected header; not trimming it")
    with open(path, "w", encoding="utf-8", newline="") as f:
        f.write(head + "\n" + "\n".join(rows) + "\n" + text[end:])
    print("trimmed factory DX7 bank: %d of %d packed bytes kept" % (len(kept), len(values)))


def main():
    sdk = os.path.abspath(sys.argv[1] if len(sys.argv) > 1 else os.path.join(REPO, "..", "fm1_open"))
    if not os.path.isfile(os.path.join(sdk, "sdk", "Build.ps1")):
        sys.exit("%s is not an fm1_open checkout" % sdk)
    base = git(sdk, "merge-base", "HEAD", EXPECTED_COMMIT, check=False).stdout.strip()
    if not base.startswith(EXPECTED_COMMIT):
        print("warning: this checkout does not contain commit %s; the patch may not fit" % EXPECTED_COMMIT)
    copy_sources(sdk)
    apply_patch(sdk)
    trim_factory_bank(sdk)
    print("done. Build with sdk\\Build.ps1 -FirmwareOnly (see firmware/README.md).")


if __name__ == "__main__":
    main()
