#!/usr/bin/env python3
"""Put the Verbalizer into a checkout of M-VAVE's open FM-1 firmware.

    python firmware/install.py [path to fm1_open]      (default: ../fm1_open)
    python firmware/install.py --only [path]           (default: ../fm1_open_only)

There are two builds, and each needs a checkout of its own.

Without --only the Verbalizer joins the stock instrument. It changes the
checkout's `app` folder and one file of the browser simulator:

  1. copies the engine (engine/*.c, *.h) and the glue (firmware/fm1_verbalizer.*)
     into app/verbalizer/, where the SDK's build compiles every .c file;
  2. applies firmware/fm1_open.patch, which adds the Verbalizer's page and its
     flash sector, hooks the audio interrupt and the
     MIDI router and makes the factory DX7 bank decoder stop early;
  3. cuts the packed factory DX7 bank down to its first 4 voices, because the
     program image has no room for the Verbalizer otherwise.

With --only the Verbalizer is the whole instrument:

  1. copies the engine and the glue as above;
  2. deletes the stock project, sample, DX7, MIDI, transfer and screen sources
     and puts the small files of firmware/only/ in their place;
  3. applies firmware/fm1_open_only.patch, which makes the audio interrupt and
     the browser simulator render the Verbalizer alone and names its two
     flash sectors.

Running it again refreshes the copied files and leaves the rest alone. Then
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
KEPT_VOICES = 4  # must match FACTORY_KEPT in fm1_open.patch
MARKER = "Verbalizer build: cut down to the first"
# Stock sources the Verbalizer-only build has no use for. Four of them are
# replaced by the files of the same name in firmware/only/app.
ONLY_REMOVED = ["project.c", "presets.c", "samples.c", "transfer.c", "fm_library.c",
                "fm_factory.c", "fm_factory.inc", "midi_router.c", "screen.c"]
ONLY_MARKER = "The screen of the Verbalizer-only firmware"

HERE = os.path.dirname(os.path.abspath(__file__))
REPO = os.path.dirname(HERE)


def git(sdk, *args, check=True):
    return subprocess.run(["git", "-C", sdk] + list(args), check=check,
                          capture_output=True, text=True)


def copy_sources(sdk):
    target = os.path.join(sdk, "app", "verbalizer")
    os.makedirs(target, exist_ok=True)
    files = (glob.glob(os.path.join(REPO, "engine", "*.[ch]")) +
             glob.glob(os.path.join(HERE, "fm1_verbalizer*.[ch]")))
    for path in files:
        shutil.copy2(path, target)
    print("copied %d source files to %s" % (len(files), target))


def apply_patch(sdk, name):
    patch = os.path.join(HERE, name)
    if git(sdk, "apply", "--reverse", "--check", patch, check=False).returncode == 0:
        print("patch already applied")
        return
    result = git(sdk, "apply", "--check", patch, check=False)
    if result.returncode != 0:
        sys.exit("%s does not apply to this checkout:\n%s" % (name, result.stderr))
    git(sdk, "apply", patch)
    print("applied " + name)


def is_only(sdk):
    """True if this checkout already holds the Verbalizer-only build."""
    screen = os.path.join(sdk, "app", "screen.c")
    return os.path.isfile(screen) and ONLY_MARKER in open(screen, encoding="utf-8").read()


def replace_stock_sources(sdk):
    app = os.path.join(sdk, "app")
    if not is_only(sdk):
        if os.path.isdir(os.path.join(app, "verbalizer")) and \
                "verbalizer" in open(os.path.join(app, "screen.c"), encoding="utf-8").read():
            sys.exit("%s holds the combined build. The Verbalizer-only build needs a checkout "
                     "of its own, at commit %s." % (sdk, EXPECTED_COMMIT))
        for name in ONLY_REMOVED:
            path = os.path.join(app, name)
            if os.path.exists(path):
                os.remove(path)
        print("removed %d stock source files" % len(ONLY_REMOVED))
    copied = 0
    for folder, _, names in os.walk(os.path.join(HERE, "only")):
        target = os.path.join(sdk, os.path.relpath(folder, os.path.join(HERE, "only")))
        for name in names:
            shutil.copy2(os.path.join(folder, name), target)
            copied += 1
    print("copied %d replacement files from firmware/only" % copied)


def trim_factory_bank(sdk):
    path = os.path.join(sdk, "app", "fm_factory.inc")
    text = open(path, encoding="utf-8", newline="").read()
    if "%s %d voices" % (MARKER, KEPT_VOICES) in text:
        print("factory DX7 bank already trimmed")
        return
    if MARKER in text:
        # trimmed to a different size by an earlier version: start again
        text = subprocess.run(["git", "-C", sdk, "show", EXPECTED_COMMIT + ":app/fm_factory.inc"],
                              check=True, capture_output=True).stdout.decode("utf-8")
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
    args = sys.argv[1:]
    only = "--only" in args
    args = [a for a in args if a != "--only"]
    default = os.path.join(REPO, "..", "fm1_open_only" if only else "fm1_open")
    sdk = os.path.abspath(args[0] if args else default)
    if not os.path.isfile(os.path.join(sdk, "sdk", "Build.ps1")):
        sys.exit("%s is not an fm1_open checkout" % sdk)
    base = git(sdk, "merge-base", "HEAD", EXPECTED_COMMIT, check=False).stdout.strip()
    if not base.startswith(EXPECTED_COMMIT):
        print("warning: this checkout does not contain commit %s; the patch may not fit" % EXPECTED_COMMIT)
    if only:
        copy_sources(sdk)
        replace_stock_sources(sdk)
        apply_patch(sdk, "fm1_open_only.patch")
    else:
        if is_only(sdk):
            sys.exit("%s holds the Verbalizer-only build; run this with --only, or give "
                     "the combined build a checkout of its own." % sdk)
        copy_sources(sdk)
        apply_patch(sdk, "fm1_open.patch")
        trim_factory_bank(sdk)
    print("done. Build with sdk\\Build.ps1 -FirmwareOnly (see firmware/README.md).")


if __name__ == "__main__":
    main()
