#!/usr/bin/env python3
"""Reference renderer: runs the ORIGINAL VocalTract.spin assembly, instruction by instruction.

This is the ground truth the C port in engine/vocal_tract.c is checked against.
It assembles the DAT section of Chip Gracey's VocalTract.spin into a small
Propeller cog model (512 registers, self-modifying code and all), re-implements
only the tiny Spin method `go`, and plays a .vts script through it.

VocalTract.spin is not stored in this repository. Get it from
https://github.com/antfriend/Verbalizer and pass its path:

    python tools/pasm_ref.py --spin path/to/VocalTract.spin tests/golden/vowel.vts
    python tools/pasm_ref.py --spin path/to/VocalTract.spin --update tests/golden

With --update, every *.vts in the directory is rendered and hashes.txt is
rewritten. The C test suite compares its own renders against that file.

Script format (.vts), one command per line, '#' starts a comment:

    set ga=30 gp=100 f1=40   stage tract parameters (0..255); they persist
    go 100                   queue a frame gliding to the staged values over 100 steps
    run 4000                 render 4000 samples (20 kHz)
    pace 100                 set_pace
    atten 0                  set_attenuation

`go` blocks while all eight frame buffers are in use, rendering samples as it
waits, exactly like the Spin method.
"""
import argparse
import glob
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import gen_tables  # noqa: E402

M32 = 0xFFFFFFFF
PARAMS = ["aa", "ga", "gp", "vp", "vr", "f1", "f2", "f3", "f4", "na", "nf", "fa", "ff"]

# mnemonic -> (operation, writes result by default)
OPS = {
    "mov": ("mov", 1), "add": ("add", 1), "sub": ("sub", 1), "and": ("and", 1),
    "test": ("and", 0), "or": ("or", 1), "neg": ("neg", 1), "negc": ("negc", 1),
    "negnz": ("negnz", 1), "sumc": ("sumc", 1), "sumnc": ("sumnc", 1),
    "shl": ("shl", 1), "shr": ("shr", 1), "sar": ("sar", 1), "rcl": ("rcl", 1),
    "movs": ("movs", 1), "movd": ("movd", 1), "djnz": ("djnz", 1),
    "jmpret": ("jmpret", 1), "rdbyte": ("rdbyte", 1), "rdword": ("rdword", 1),
    "rdlong": ("rdlong", 1), "wrlong": ("wrlong", 0), "waitcnt": ("waitcnt", 1),
}
OP_NAMES = sorted(set(v[0] for v in OPS.values()))
OP_CODE = {name: i for i, name in enumerate(OP_NAMES)}
# condition field, indexed by (C << 1 | Z), as on the real chip
CONDS = {"if_c": 0b1100, "if_nc": 0b0011, "if_z": 0b1010, "if_nz": 0b0101}
SPECIAL = {"par": 0x1F0, "cnt": 0x1F1, "dira": 0x1F6, "dirb": 0x1F7, "ctra": 0x1F8,
           "ctrb": 0x1F9, "frqa": 0x1FA, "frqb": 0x1FB, "vscl": 0x1FF}
CONSTS = {"frame_bytes": 16, "frame_buffer_bytes": 128}


def read_spin(path):
    raw = open(path, "rb").read()
    if raw[:2] in (b"\xff\xfe", b"\xfe\xff"):
        return raw.decode("utf-16")
    return raw.decode("utf-8", errors="replace")


def encode(op, z, c, r, i, cond, d, s):
    return (OP_CODE[op] << 26) | (z << 25) | (c << 24) | (r << 23) | (i << 22) | \
           (cond << 18) | ((d & 0x1FF) << 9) | (s & 0x1FF)


class Assembler:
    """Two-pass assembler for the subset of PASM that VocalTract.spin uses."""

    def __init__(self, text):
        lines = text.splitlines()
        start = next(i for i, l in enumerate(lines) if re.match(r"^entry\s+org", l))
        self.items = []  # (address, scope, mnemonic, rest)
        self.symbols = dict(SPECIAL)
        self.symbols.update(CONSTS)
        addr, scope = 0, "entry"
        for line in lines[start:]:
            if line.startswith("{{"):
                break
            line = line.split("'")[0].rstrip()
            if not line.strip():
                continue
            tokens = line.split()
            first = tokens[0].lower()
            if first not in OPS and first not in CONDS and first not in (
                    "org", "long", "res", "jmp", "call", "ret"):
                label = tokens[0]
                if label.startswith(":"):
                    self.symbols[self.local(scope, label)] = addr
                else:
                    scope = label
                    self.symbols[label] = addr
                line = line.split(None, 1)[1] if len(tokens) > 1 else ""
                if not line.strip():
                    continue
            parts = line.split(None, 1)
            mnem = parts[0].lower()
            rest = parts[1] if len(parts) > 1 else ""
            if mnem == "org":
                continue
            if mnem == "res":
                addr += self.expr(rest, scope)
                continue
            self.items.append((addr, scope, mnem, rest))
            addr += 1
        self.size = addr
        if self.size > 0x1F0:
            raise ValueError("program does not fit in a cog")

    @staticmethod
    def local(scope, label):
        return "L_%s_%s" % (scope, label[1:])

    def expr(self, text, scope):
        text = re.sub(r"\$([0-9A-Fa-f_]+)", lambda m: "0x" + m.group(1).replace("_", ""), text)
        text = re.sub(r":(\w+)", lambda m: self.local(scope, ":" + m.group(1)), text)
        return int(eval(text, {"__builtins__": {}}, self.symbols)) & M32

    def assemble(self):
        reg = [0] * 512
        for addr, scope, mnem, rest in self.items:
            if mnem == "long":
                reg[addr] = self.expr(rest, scope)
                continue
            cond = 0b1111
            if mnem in CONDS:
                cond = CONDS[mnem]
                mnem, rest = (rest.split(None, 1) + [""])[:2]
                mnem = mnem.lower()
            z = c = 0
            nr = False
            while True:
                m = re.search(r"[\s,]+(wc|wz|nr)\s*$", rest)
                if not m:
                    break
                z |= m.group(1) == "wz"
                c |= m.group(1) == "wc"
                nr |= m.group(1) == "nr"
                rest = rest[:m.start()]
            if mnem == "ret":
                reg[addr] = encode("jmpret", 0, 0, 0, 1, cond, 0, 0)
                continue
            if mnem == "jmp":
                dst, src, r = "0", rest, 0
                op = "jmpret"
            elif mnem == "call":
                target = rest.strip().lstrip("#").strip()
                dst, src, r = target + "_ret", rest, 1
                op = "jmpret"
            else:
                op, r = OPS[mnem]
                dst, src = rest.split(",", 1)
            if nr:
                r = 0
            src = src.strip()
            imm = src.startswith("#")
            reg[addr] = encode(op, int(z), int(c), r, int(imm), cond,
                               self.expr(dst, scope), self.expr(src.lstrip("#"), scope))
        return reg


def sar(v, n):
    n &= 31
    return ((v - (1 << 32) if v & 0x80000000 else v) >> n) & M32


def parity(v):
    return bin(v).count("1") & 1


class Cog:
    PAR = 0x1000  # hub address of the Spin object's `attenuation` long

    def __init__(self, spin_path):
        asm = Assembler(read_spin(spin_path))
        self.reg = asm.assemble()
        self.hub = bytearray(0x10000)
        for i, v in enumerate(gen_tables.antilog_table()):
            struct.pack_into("<H", self.hub, 0xD000 + 2 * i, v)
        for i, v in enumerate(gen_tables.sine_table()):
            struct.pack_into("<H", self.hub, 0xE000 + 2 * i, v)
        self.reg[SPECIAL["par"]] = self.PAR
        self.pc = 0
        self.c = self.z = 0
        self.samples = []
        self.index = 0   # Spin `index`, in frames
        self.pace = 100
        self.frames = self.PAR + 8 * 4
        # run the initialisation code up to the first waitcnt
        self._run_to_waitcnt()

    # hub access
    def rd(self, addr, size):
        addr &= 0xFFFF & ~(size - 1)
        return int.from_bytes(self.hub[addr:addr + size], "little")

    def wr_long(self, addr, value):
        addr &= 0xFFFC
        struct.pack_into("<I", self.hub, addr, value & M32)

    def _step(self):
        """Execute one instruction. Returns True if it was a waitcnt."""
        ins = self.reg[self.pc]
        pc = self.pc
        self.pc = (pc + 1) & 0x1FF
        cond = (ins >> 18) & 15
        if not (cond >> ((self.c << 1) | self.z)) & 1:
            return False
        op = OP_NAMES[ins >> 26]
        wz, wc, wr, imm = (ins >> 25) & 1, (ins >> 24) & 1, (ins >> 23) & 1, (ins >> 22) & 1
        di, si = (ins >> 9) & 0x1FF, ins & 0x1FF
        d = self.reg[di]
        s = si if imm else self.reg[si]
        c = self.c
        res = d
        if op == "mov":
            res, c = s, s >> 31
        elif op == "add":
            res, c = (d + s) & M32, (d + s) >> 32
        elif op == "sub":
            res, c = (d - s) & M32, int(d < s)
        elif op == "and":
            res = d & s
            c = parity(res)
        elif op == "or":
            res = d | s
            c = parity(res)
        elif op == "neg":
            res, c = (-s) & M32, s >> 31
        elif op == "negc":
            res, c = ((-s) & M32 if self.c else s), s >> 31
        elif op == "negnz":
            res, c = (s if self.z else (-s) & M32), s >> 31
        elif op == "sumc":
            res = (d - s) & M32 if self.c else (d + s) & M32
        elif op == "sumnc":
            res = (d + s) & M32 if self.c else (d - s) & M32
        elif op == "shl":
            res, c = (d << (s & 31)) & M32, d >> 31
        elif op == "shr":
            res, c = d >> (s & 31), d & 1
        elif op == "sar":
            res, c = sar(d, s), d & 1
        elif op == "rcl":
            n = s & 31
            res = ((d << n) | (((1 << n) - 1) if self.c else 0)) & M32
            c = d >> 31
        elif op == "movs":
            res = (d & ~0x1FF) | (s & 0x1FF)
        elif op == "movd":
            res = (d & ~(0x1FF << 9)) | ((s & 0x1FF) << 9)
        elif op == "djnz":
            res = (d - 1) & M32
            if res:
                self.pc = s & 0x1FF
        elif op == "jmpret":
            res = (d & ~0x1FF) | ((pc + 1) & 0x1FF)
            self.pc = s & 0x1FF
        elif op == "rdbyte":
            res = self.rd(s, 1)
        elif op == "rdword":
            res = self.rd(s, 2)
        elif op == "rdlong":
            res = self.rd(s, 4)
        elif op == "wrlong":
            self.wr_long(s, d)
            if (s & 0xFFFC) == self.PAR + 4:
                self.samples.append(d)
        elif op == "waitcnt":
            res = (d + s) & M32
        if wr:
            self.reg[di] = res
        if wz:
            self.z = int(res == 0)
        if wc:
            self.c = c & 1
        return op == "waitcnt"

    def _run_to_waitcnt(self):
        """Stop with pc AT the next waitcnt (not yet executed)."""
        limit = 200000
        while OP_NAMES[self.reg[self.pc] >> 26] != "waitcnt":
            self._step()
            limit -= 1
            if not limit:
                raise RuntimeError("cog never reached waitcnt")

    def run(self, n):
        for _ in range(n):
            self._step()            # the waitcnt itself
            self._run_to_waitcnt()  # one full pass of the sample loop

    # Spin-side methods
    def set_attenuation(self, level):
        self.wr_long(self.PAR, level)

    def go(self, params, time):
        slot = self.frames + self.index * 16
        while self.rd(slot, 4):
            self.run(1)
        self.hub[slot + 3:slot + 16] = bytes(params)
        step = 0x01000000 // max(time * 100 // self.pace, 2)
        self.wr_long(slot, self.rd(slot, 4) | step)
        self.index = (self.index + 1) & 7


def play_script(cog, path):
    params = [0] * 13
    for raw in open(path):
        line = raw.split("#")[0].split()
        if not line:
            continue
        cmd, args = line[0], line[1:]
        if cmd == "set":
            for a in args:
                k, v = a.split("=")
                params[PARAMS.index(k)] = int(v) & 0xFF
        elif cmd == "go":
            cog.go(params, int(args[0]))
        elif cmd == "run":
            cog.run(int(args[0]))
        elif cmd == "pace":
            cog.pace = int(args[0])
        elif cmd == "atten":
            cog.set_attenuation(int(args[0]))
        else:
            raise ValueError("%s: unknown command %r" % (path, cmd))
    return cog.samples


def fnv1a64(samples):
    h = 0xCBF29CE484222325
    for s in samples:
        for b in struct.pack("<I", s):
            h = ((h ^ b) * 0x100000001B3) & 0xFFFFFFFFFFFFFFFF
    return h


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--spin", required=True, help="path to the original VocalTract.spin")
    ap.add_argument("--update", action="store_true",
                    help="treat the argument as a directory and rewrite its hashes.txt")
    ap.add_argument("--raw", help="also write the samples as raw little-endian int32")
    ap.add_argument("target", help="a .vts script, or a directory with --update")
    args = ap.parse_args()

    if args.update:
        rows = []
        for path in sorted(glob.glob(os.path.join(args.target, "*.vts"))):
            samples = play_script(Cog(args.spin), path)
            name = os.path.splitext(os.path.basename(path))[0]
            rows.append("%s %d %016x" % (name, len(samples), fnv1a64(samples)))
            print(rows[-1])
        with open(os.path.join(args.target, "hashes.txt"), "w", newline="\n") as f:
            f.write("# name samples fnv1a64 -- written by tools/pasm_ref.py --update\n")
            f.write("\n".join(rows) + "\n")
        return

    samples = play_script(Cog(args.spin), args.target)
    print("%d %016x" % (len(samples), fnv1a64(samples)))
    if args.raw:
        with open(args.raw, "wb") as f:
            f.write(struct.pack("<%dI" % len(samples), *samples))


if __name__ == "__main__":
    main()
