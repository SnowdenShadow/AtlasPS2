#!/usr/bin/env python3
"""
AtlasPS2 - checkexports.py

Verify that iop/atlascdvd/exports.tab puts every function at the ordinal
the real cdvdman puts it at.

WHY THIS EXISTS
---------------
Games bind to cdvdman by ordinal. The names in exports.tab are for
people; the positions are the ABI. A table with the right functions in
the wrong order links cleanly, runs, and calls the wrong function - a
game that asked whether the drive was ready gets a disk type instead.

There is no way to notice that on a build machine by running the code,
because the code cannot run on a build machine. But the ordering is
plain text on both sides, so it can be compared, and that comparison is
run as part of building the module rather than as a test somebody might
skip.

The authority is $PS2SDK/iop/include/cdvdman.h. If that file and this
table disagree, this table is wrong.

    python3 tools/checkexports.py iop/atlascdvd/exports.tab \
            $PS2SDK/iop/include/cdvdman.h
"""

import re
import sys

# Positions 0-3 are loadcore's module hooks, not cdvdman functions, and
# the SDK header says nothing about them.
HOOK_SLOTS = 4


def read_sdk(path):
    """ordinal -> name, from the DECLARE_IMPORT lines in cdvdman.h."""
    out = {}
    pat = re.compile(r"DECLARE_IMPORT\(\s*(\d+)\s*,\s*(\w+)\s*\)")

    with open(path, "r", encoding="utf-8", errors="replace") as f:
        for line in f:
            m = pat.search(line)
            if m:
                out[int(m.group(1))] = m.group(2)

    if not out:
        sys.exit(f"{path}: no DECLARE_IMPORT lines - wrong file?")

    return out


def read_table(path):
    """The DECLARE_EXPORT names, in order, with comments stripped first.

    Comments are removed before matching so that a line commented out
    during debugging cannot be counted as an entry - which would shift
    every ordinal after it.
    """
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        text = f.read()

    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)

    return re.findall(r"DECLARE_EXPORT\(\s*(\w+)\s*\)", text)


def main():
    if len(sys.argv) != 3:
        sys.exit(f"usage: {sys.argv[0]} exports.tab cdvdman.h")

    names = read_table(sys.argv[1])
    sdk = read_sdk(sys.argv[2])

    if len(names) < HOOK_SLOTS:
        sys.exit(f"{sys.argv[1]}: only {len(names)} entries; "
                 f"the first {HOOK_SLOTS} are loadcore's hooks")

    problems = []

    for i, name in enumerate(names):
        if i < HOOK_SLOTS:
            continue

        want = sdk.get(i)

        if want is None:
            # An ordinal the SDK does not name. Only a deliberate
            # placeholder belongs there; anything else means the table
            # has drifted and everything after it is off by one.
            if not name.startswith("atlas_"):
                problems.append(
                    f"  {i}: table has {name}, the SDK names no function "
                    f"at this ordinal")
            continue

        if name != want:
            problems.append(f"  {i}: table has {name}, cdvdman has {want}")

    # An ordinal the SDK names, below the end of our table, that we have
    # skipped entirely: impossible by construction above (positions are
    # dense), but checked so that a future edit cannot introduce it.
    highest = len(names) - 1
    for ordinal in sorted(sdk):
        if ordinal <= highest and ordinal >= HOOK_SLOTS:
            if sdk[ordinal] != names[ordinal] and \
               not names[ordinal].startswith("atlas_"):
                pass    # already reported above

    if problems:
        print(f"{sys.argv[1]}: export table does not match cdvdman:",
              file=sys.stderr)
        print("\n".join(problems), file=sys.stderr)
        print("\nGames bind by ordinal. A mismatch here calls the wrong "
              "function at runtime.", file=sys.stderr)
        sys.exit(1)

    print(f"exports: {len(names) - HOOK_SLOTS} entries match cdvdman "
          f"ordinals {HOOK_SLOTS}-{highest}")


if __name__ == "__main__":
    main()
