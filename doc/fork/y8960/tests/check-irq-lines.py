"""List which file raises which bit of the emulator's pending-interrupt mask,
and fail if a bit that one of the fork's own files raises is raised by any
other file.

Usage:  py check-irq-lines.py [--src <blueMSX/Src>] [--upstream <ref>]

boardClearInt (Board.c) drops a bit for everyone, and lets the line go once
no bit is left. Two devices on one bit therefore lose each other's
interrupts. Upstream names the bits in one table in Board.h (BOARD_INT_*),
and Board.c fails the build when two of the names listed in its
BOARD_INT_ALL overlap. That check sees only the names in that list: a bit
given as a number, or a name left out of the list, passes it. Run this after
every merge of upstream.

A file is the fork's own when <ref> (default upstream/develop) does not have
it. Bits shared only among upstream's files are listed but do not fail: this
cannot tell a real clash from the backends of one chip sharing its bit.

Bits are counted per file that raises them, so sharing through one file does
not show: ym2151.c raises 0x40 for both the SFG cartridges and Yamaha NET,
and two cartridges of one kind share whatever bit their file raises. Each
Y8960 block raises its bit in a file of its own, and the card goes in once.

Exit status: 0 when no bit of a fork file is shared and every BOARD_INT_*
name is in BOARD_INT_ALL, 1 when either fails, 2 when a call could not be
resolved to a value. For 2, add a rule to PASS_THROUGH rather than skip the
call: a value read from elsewhere is still a bit someone holds.
"""
import argparse
import ast
import os
import re
import subprocess
import sys

# Calls whose argument is a parameter rather than a constant. Each maps to
# the names that are handed in, as (file that hands it in, name).
PASS_THROUGH = {
    ("SoundChips/Y8960Opl2Core.h", "mask"): [("SoundChips/Y8960Opl2ex.cpp", "BOARD_INT_Y8960_OPL2EX")],
    ("SoundChips/YM2608.cpp", "ym2608->irqMask"): [("Memory/romMapperMakoto.c", "BOARD_INT_MAKOTO")],
}

# Where the functions are defined; their argument is the parameter itself.
SKIP_FILES = {"Board/Board.c", "Board/Board.h"}
TABLE_FILE = "Board/Board.h"
CHECK_FILE = "Board/Board.c"

CALL = re.compile(r"\bboard(?:Set|Clear|Get)Int\s*\(")
DEFINE = re.compile(r"^[ \t]*#[ \t]*define[ \t]+(\w+)[ \t]+(.+?)[ \t]*$", re.M)
VDP_IE = re.compile(r"^vdp->intIE([0-2])$")
SECONDARY = re.compile(r"\bVdpSecondaryConfig\s+\w+\s*=\s*\{([^}]*)\}")
TABLE_NAME = re.compile(r"\bBOARD_INT_\w+")
INT_SUFFIX = re.compile(r"\b(0[xX][0-9a-fA-F]+|\d+)[uUlL]+\b")


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


def evaluate(expr, lookup):
    """The value of a constant expression of numbers, names, << and |, or None.
    lookup(name) gives a name's value or None."""
    try:
        tree = ast.parse(INT_SUFFIX.sub(r"\1", expr.strip()), mode="eval")
    except SyntaxError:
        return None

    def walk(node):
        if isinstance(node, ast.Expression):
            return walk(node.body)
        if isinstance(node, ast.Constant) and isinstance(node.value, int):
            return node.value
        if isinstance(node, ast.Name):
            return lookup(node.id)
        if isinstance(node, ast.BinOp) and isinstance(node.op, (ast.LShift, ast.BitOr)):
            a, b = walk(node.left), walk(node.right)
            if a is None or b is None:
                return None
            return a << b if isinstance(node.op, ast.LShift) else a | b
        return None

    return walk(tree)


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

    texts, raw = {}, {}
    for dirpath, _, names in os.walk(args.src):
        for name in names:
            if os.path.splitext(name)[1].lower() not in (".c", ".cpp", ".h", ".hpp"):
                continue
            path = os.path.join(dirpath, name)
            rel = os.path.relpath(path, args.src).replace(os.sep, "/")
            with open(path, "rb") as f:
                text = strip_comments(f.read().decode("latin-1"))
            texts[rel] = text
            raw[rel] = dict(DEFINE.findall(text))

    unresolved = []
    if TABLE_FILE not in raw:
        print("no %s under %s" % (TABLE_FILE, args.src))
        return 2
    table_names = sorted(n for n in raw[TABLE_FILE] if n.startswith("BOARD_INT_"))

    def table(name, seen=()):
        if name not in table_names or name in seen:
            return None
        return evaluate(raw[TABLE_FILE][name], lambda n: table(n, seen + (name,)))

    def define(rel, name, seen=()):
        """A name's value in rel: its own #define first, then the table."""
        if (rel, name) in seen:
            return None
        expr = raw.get(rel, {}).get(name)
        if expr is not None:
            v = evaluate(expr, lambda n: define(rel, n, seen + ((rel, name),)))
            if v is not None:
                return v
        return table(name)

    for name in table_names:
        if table(name) is None:
            unresolved.append((TABLE_FILE, 0, name))

    # Every table name has to be in BOARD_INT_ALL, or the build check skips it.
    m = re.search(r"#\s*define\s+BOARD_INT_ALL\s*\(\s*op\s*\)(.*?)\n\s*typedef", texts.get(CHECK_FILE, ""), re.S)
    listed = set(TABLE_NAME.findall(m.group(1))) if m else set()
    if not m:
        print("no BOARD_INT_ALL in %s" % CHECK_FILE)
    missing = [n for n in table_names if n != "BOARD_INT_ALL" and n not in listed]

    # The machine's own VDP takes the table's IE bits. A second VDP's owner
    # hands in intBase; the VDP raises it, << 1 and << 2.
    vdp_ie = {k: [] for k in range(3)}
    for k in range(3):
        v = table("BOARD_INT_VDP_IE%d" % k)
        if v is None:
            unresolved.append(("VideoChips/VDP.c", 0, "BOARD_INT_VDP_IE%d" % k))
        else:
            vdp_ie[k].append(("VideoChips/VDP.c", v))
    for rel, text in texts.items():
        for m2 in SECONDARY.finditer(text):
            fields = [f.strip() for f in m2.group(1).split(",")]
            base = evaluate(fields[1], lambda n: define(rel, n)) if len(fields) > 1 else None
            if base is None:
                unresolved.append((rel, text.count("\n", 0, m2.start()) + 1, "VdpSecondaryConfig intBase"))
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
                v = evaluate(term, lambda n: define(rel, n))
                if v is not None:
                    found = [(rel, v)]
                elif VDP_IE.match(term) and rel == "VideoChips/VDP.c":
                    found = vdp_ie[int(VDP_IE.match(term).group(1))]
                elif (rel, term) in PASS_THROUGH:
                    for src, name in PASS_THROUGH[(rel, term)]:
                        v = define(src, name)
                        if v is None or not re.search(r"\b%s\b" % name, texts.get(src, "")):
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

    for name in missing:
        print("NOT IN BOARD_INT_ALL  %s (%s)" % (name, CHECK_FILE))
    for rel, line, term in unresolved:
        print("UNRESOLVED  %s:%d  %s" % (rel, line, term))
    if unresolved:
        return 2
    if clash:
        print("%d bit(s) of the fork's files are shared" % clash)
    if clash or missing or not m:
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
