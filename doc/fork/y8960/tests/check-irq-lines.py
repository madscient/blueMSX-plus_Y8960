"""List which file raises which bit of the emulator's pending-interrupt mask,
and fail if a bit that one of the fork's own files raises is raised by any
other file.

Usage:  py check-irq-lines.py [--src <blueMSX/Src>] [--upstream <ref>]

boardClearInt (Board.c) drops a bit for everyone, and lets the line go once
no bit is left. Two devices on one bit therefore lose each other's
interrupts, and nothing says so at build or run time. Upstream gives out new
bits from the bottom up and keeps no table of them, so run this after every
merge of upstream.

A file is the fork's own when <ref> (default upstream/develop) does not have
it. Bits shared only among upstream's files are listed but do not fail: this
cannot tell a real clash from the backends of one chip sharing its bit.

Exit status: 0 when no bit of a fork file is shared, 1 when one is, 2 when a
call could not be resolved to a value. For 2, add a rule to PASS_THROUGH
rather than skip the call: a value read from elsewhere is still a bit someone
holds.
"""
import argparse
import os
import re
import subprocess
import sys

# Calls whose argument is a parameter rather than a constant. Each maps to
# the #define(s) that are handed in, as (file, name).
PASS_THROUGH = {
    ("SoundChips/Y8960Opl2Core.h", "mask"): [("SoundChips/Y8960Opl2ex.cpp", "Y8960_OPL2EX_IRQ")],
    ("SoundChips/YM2608.cpp", "ym2608->irqMask"): [("Memory/romMapperMakoto.c", "MAKOTO_IRQ_MASK")],
}

# Where the functions are defined; their argument is the parameter itself.
SKIP_FILES = {"Board/Board.c", "Board/Board.h"}

CALL = re.compile(r"\bboard(?:Set|Clear|Get)Int\s*\(")
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+(.+?)[ \t]*$", re.M)
LITERAL = re.compile(r"^\(?\s*(0[xX][0-9a-fA-F]+|\d+)[uUlL]*\s*\)?$")
VDP_IE = re.compile(r"^vdp->intIE([0-2])$")
SECONDARY = re.compile(r"\bVdpSecondaryConfig\s+\w+\s*=\s*\{([^}]*)\}")


def strip_comments(text):
    out = []
    i, n = 0, len(text)
    while i < n:
        c = text[i]
        if c in "\"'":
            j = i + 1
            while j < n and text[j] != c:
                j += 2 if text[j] == "\\" else 1
            out.append(text[i:j + 1])
            i = j + 1
        elif text.startswith("/*", i):
            j = text.find("*/", i + 2)
            j = n if j < 0 else j + 2
            out.append("\n" * text.count("\n", i, j))
            i = j
        elif text.startswith("//", i):
            j = text.find("\n", i)
            i = n if j < 0 else j
        else:
            out.append(c)
            i += 1
    return "".join(out)


def literal(expr):
    m = LITERAL.match(expr.strip())
    return int(m.group(1), 0) if m else None


def call_args(text):
    for m in CALL.finditer(text):
        depth, j = 1, m.end()
        while j < len(text) and depth:
            depth += {"(": 1, ")": -1}.get(text[j], 0)
            j += 1
        yield text.count("\n", 0, m.start()) + 1, text[m.end():j - 1]


def main():
    here = os.path.dirname(os.path.abspath(__file__))
    root = subprocess.run(["git", "-C", here, "rev-parse", "--show-toplevel"],
                          capture_output=True, text=True, check=True).stdout.strip()
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=os.path.join(root, "blueMSX", "Src"))
    ap.add_argument("--upstream", default="upstream/develop")
    args = ap.parse_args()

    r = subprocess.run(["git", "-C", root, "ls-tree", "-r", "--name-only", args.upstream, "--", "blueMSX/Src"],
                       capture_output=True, text=True)
    if r.returncode != 0 or not r.stdout.strip():
        print("cannot list %s: %s" % (args.upstream, r.stderr.strip()))
        return 2
    upstream = {p[len("blueMSX/Src/"):] for p in r.stdout.splitlines()}

    texts, defines = {}, {}
    for dirpath, _, names in os.walk(args.src):
        for name in names:
            if os.path.splitext(name)[1].lower() not in (".c", ".cpp", ".h", ".hpp"):
                continue
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, args.src).replace(os.sep, "/")
            with open(path, "rb") as f:
                text = strip_comments(f.read().decode("latin-1"))
            texts[rel] = text
            defines[rel] = {k: literal(v) for k, v in DEFINE.findall(text)}

    def define(rel, name):
        return defines.get(rel, {}).get(name)

    # The second VDP's owner hands in intBase; the VDP raises it, << 1 and << 2.
    vdp_ie = {k: [] for k in range(3)}
    for k in range(3):
        v = define("VideoChips/VDP.c", "INT_IE%d" % k)
        if v is not None:
            vdp_ie[k].append(("VideoChips/VDP.c", v))
    unresolved = []
    for rel, text in texts.items():
        for m in SECONDARY.finditer(text):
            fields = [f.strip() for f in m.group(1).split(",")]
            base = literal(fields[1]) if len(fields) > 1 else None
            if base is None and len(fields) > 1:
                base = define(rel, fields[1])
            if base is None:
                unresolved.append((rel, text.count("\n", 0, m.start()) + 1, "VdpSecondaryConfig intBase"))
                continue
            for k in range(3):
                vdp_ie[k].append((rel, base << k))

    owners = {}
    for rel, text in sorted(texts.items()):
        if rel in SKIP_FILES:
            continue
        for line, arg in call_args(text):
            for term in arg.split("|"):
                term = term.strip().strip("()").strip()
                found = []
                v = literal(term)
                if v is None:
                    v = define(rel, term)
                if v is not None:
                    found = [(rel, v)]
                elif VDP_IE.match(term) and rel == "VideoChips/VDP.c":
                    found = vdp_ie[int(VDP_IE.match(term).group(1))]
                elif (rel, term) in PASS_THROUGH:
                    for src, name in PASS_THROUGH[(rel, term)]:
                        v = define(src, name)
                        if v is None:
                            unresolved.append((rel, line, "%s (no %s in %s)" % (term, name, src)))
                        else:
                            found.append((src, v))
                else:
                    unresolved.append((rel, line, term))
                for owner, bit in found:
                    owners.setdefault(bit, set()).add(owner)

    clash = 0
    print("%-12s %s" % ("bit", "raised by (* = not in %s)" % args.upstream))
    for bit in sorted(owners):
        files = sorted(owners[bit])
        fork = [f for f in files if f not in upstream]
        note = ""
        if len(files) > 1:
            if fork:
                note = "  CLASH"
                clash += 1
            else:
                note = "  (shared, upstream only)"
        print("0x%08X   %s%s" % (bit, ", ".join(f + ("*" if f in fork else "") for f in files), note))

    for rel, line, term in unresolved:
        print("UNRESOLVED  %s:%d  %s" % (rel, line, term))
    if unresolved:
        return 2
    if clash:
        print("%d bit(s) of the fork's files are shared" % clash)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
