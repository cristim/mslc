#!/usr/bin/env python3
"""Generates src/stdlib_names.inc from the Metal Standard Library headers.

mslc has to tell "using metal::name;" of a name the library has from one it does
not, as Apple's compiler does. The headers are the source of truth, so the names are
read from them: every identifier outside comments, strings, preprocessor includes
and numeric literals, filed under the namespace or enum it appears in
(metal, metal::precise, metal::access, ...). A superset on purpose: a function's
locals and template parameters are filed under their namespace too, so a typo that
happens to spell one is not caught.

Usage:
    python3 scripts/gen_stdlib_names.py <Metal toolchain include/metal dir> > src/stdlib_names.inc

The directory is <toolchain>/usr/metal/<n>/lib/clang/<version>/include/metal.
"""

import os
import re
import sys
import textwrap

TOKEN = re.compile(r"[A-Za-z_][A-Za-z_0-9]*|[{};]")


def strip(text):
    text = re.sub(r"/\*.*?\*/", " ", text, flags=re.S)
    text = re.sub(r"//[^\n]*", " ", text)
    text = re.sub(r'"(\\.|[^"\\])*"|\'(\\.|[^\'\\])*\'', " ", text)
    text = re.sub(r"^\s*#\s*(include|pragma)[^\n]*", " ", text, flags=re.M)
    return re.sub(r"\b[0-9][A-Za-z0-9_.']*", " ", text)


def scan(text, sections):
    scopes = []  # (kind, name) per open brace; kind is "ns", "enum" or "block"
    statement = []

    def section(depth):
        names = [scope[1] for scope in scopes[:depth] if scope[0] != "block"]
        return "::".join(["metal"] + names)

    for token in TOKEN.findall(strip(text)):
        if token == "{":
            if len(statement) >= 2 and statement[-2] == "namespace" and not statement[-1].startswith("_"):
                name = statement[-1]
                scopes.append(("block", None, False) if name == "metal" else ("ns", name, False))
            elif "enum" in statement:
                after = statement[statement.index("enum") + 1:]
                rest = [t for t in after if t not in ("class", "struct")]
                scoped = len(rest) != len(after)
                scopes.append(("enum", rest[0], not scoped) if rest else ("block", None, False))
            else:
                scopes.append(("block", None, False))
            statement = []
        elif token in "};":
            if token == "}" and scopes:
                scopes.pop()
            statement = []
        else:
            statement.append(token)
            sections.setdefault(section(len(scopes)), set()).add(token)
            # An unscoped enum's enumerators are also names of the enclosing namespace.
            if scopes and scopes[-1][0] == "enum" and scopes[-1][2]:
                sections.setdefault(section(len(scopes) - 1), set()).add(token)


def main():
    root = sys.argv[1]
    sections = {}
    for directory, _, files in sorted(os.walk(root)):
        for name in sorted(files):
            if not name.endswith(".modulemap"):
                with open(os.path.join(directory, name), errors="ignore") as header:
                    scan(header.read(), sections)
    version = re.search(r"clang/([0-9.]+)/", os.path.abspath(root) + "/")
    print("// The identifiers of the Metal Standard Library's headers (metalfe-%s), one line per" % (version.group(1) if version else "unknown"))
    print("// namespace or enum, led by its name, for \"using metal::name;\" to be checked against: a")
    print("// name the library does not have is rejected, as Apple's compiler does. A superset (see")
    print("// scripts/gen_stdlib_names.py). Regenerate with")
    print("//   python3 scripts/gen_stdlib_names.py <toolchain>/usr/metal/<n>/lib/clang/<version>/include/metal > src/stdlib_names.inc")
    print('R"NAMES(')
    for key in sorted(sections):
        for line in textwrap.wrap(" ".join(sorted(sections[key])), 2000, break_long_words=False):
            print(key + " " + line)
    print(')NAMES"')


main()
