#!/usr/bin/env python3
"""
Fix LaTeX and unrendered formatting issues across documentation markdown files.
"""

import os
import re

BASE_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DOCS_DIR = os.path.join(BASE_DIR, "documentation", "docs")

def fix_snippets_clock_speeds():
    path = os.path.join(DOCS_DIR, "snippets", "clock-speeds.md")
    if not os.path.isfile(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    # Replace $...$ units in table
    # Match both standard \text{ ms} and \ \mu\text{s}
    content = re.sub(r'\$(\d+(?:\.\d+)?)\\text\{\s*ms\}\$', r'\1 ms', content)
    content = re.sub(r'\$(\d+(?:\.\d+)?)\\text\{\s*ns\}\$', r'\1 ns', content)
    content = re.sub(r'\$(\+?\d+(?:\.\d+)?)\\\s*\\mu\\text\{\s*s\}\$', r'\1 µs', content)
    content = content.replace(r'(+19.9 \mu\text{s} ROM margin)', '(+19.9 µs ROM margin)')
    content = content.replace(r'(+28 ns margin above 55 ns ROM limit)', '(+28 ns margin above 55 ns ROM limit)')
    content = re.sub(r'\$(\+?\d+(?:\.\d+)?\\text\{\s*ns\})\$', r'\1', content)
    content = content.replace(r'\text{ ns}', ' ns')

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Fixed {path}")

def fix_config_menu():
    path = os.path.join(DOCS_DIR, "user-guide", "config-menu.md")
    if not os.path.isfile(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    content = content.replace(r'$\le 50\text{ kHz}$', '≤ 50 kHz')
    content = content.replace(r'$\ge 62.5\text{ kHz}$', '≥ 62.5 kHz')
    content = content.replace(r'$55\text{ ns}$', '55 ns')
    content = content.replace(r'$18.18\text{ MHz}$', '18.18 MHz')
    content = content.replace(r'$83.3\text{ ns}$', '83.3 ns')
    content = content.replace(r'$+28\text{ ns}$', '+28 ns')
    content = content.replace(r'$\times$', '×')

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Fixed {path}")

def fix_terminal_commands():
    path = os.path.join(DOCS_DIR, "user-guide", "terminal-commands.md")
    if not os.path.isfile(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    content = content.replace(r'Programs $\le$ 4 kB:', 'Programs ≤ 4 kB:')
    content = content.replace(r'$\ge$ 10 seconds', '≥ 10 seconds')

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Fixed {path}")

def fix_architecture():
    path = os.path.join(DOCS_DIR, "firmware", "architecture.md")
    if not os.path.isfile(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    content = content.replace(r'Programs $\le$ 4 kB', 'Programs ≤ 4 kB')
    content = content.replace(r'frequency is $\le$ 50 kHz', 'frequency is ≤ 50 kHz')
    content = content.replace(r'takes ~10 $\mu$s per byte', 'takes ~10 µs per byte')
    content = content.replace(r'($1.66\ \mu\text{s}$ at 12 MHz)', '(1.66 µs at 12 MHz)')
    content = content.replace(r'($T_{AA} = 55\text{ ns}$)', '(*T*~AA~ = 55 ns)')
    content = content.replace(r'($T_{AA} = 55$ ns)', '(*T*~AA~ = 55 ns)')
    content = content.replace(r'at $41.6\text{ ns}$ or 48 MHz at $20.8\text{ ns}$', 'at 41.6 ns or 48 MHz at 20.8 ns')
    content = content.replace(r'($+28.3\text{ ns}$)', '(+28.3 ns)')
    content = content.replace(r'frequencies $\le 50\text{ kHz}$', 'frequencies ≤ 50 kHz')
    content = content.replace(r'frequencies $\ge 62.5\text{ kHz}$', 'frequencies ≥ 62.5 kHz')

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Fixed {path}")

def fix_roadmap():
    path = os.path.join(DOCS_DIR, "roadmap.md")
    if not os.path.isfile(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    content = content.replace(r'($1.66\ \mu\text{s}$ at 12 MHz)', '(1.66 µs at 12 MHz)')
    content = content.replace(r'of $55\text{ ns}$ ($18.18\text{ MHz}$ physical limit)', 'of 55 ns (18.18 MHz physical limit)')
    content = content.replace(r'24 MHz ($41.6\text{ ns}$) and 48 MHz ($20.8\text{ ns}$)', '24 MHz (41.6 ns) and 48 MHz (20.8 ns)')
    content = content.replace(r'($+28.3\text{ ns}$ at 12 MHz)', '(+28.3 ns at 12 MHz)')
    content = content.replace(r'with $1$ to $100\text{k}$ tick multipliers', 'with 1 to 100k tick multipliers')
    content = content.replace(r'**`PD8`** $\rightarrow$ `TCK`', '**`PD8`** → `TCK`')
    content = content.replace(r'**`PD9`** $\rightarrow$ `TMS`', '**`PD9`** → `TMS`')
    content = content.replace(r'**`PD10`** $\rightarrow$ `TDI`', '**`PD10`** → `TDI`')
    content = content.replace(r'**`PD11_JTAG`** (or `PF6`) $\rightarrow$ `TDO`', '**`PD11_JTAG`** (or `PF6`) → `TDO`')

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Fixed {path}")

def fix_programs_readme():
    for rpath in [os.path.join(DOCS_DIR, "programs", "README.md"), os.path.join(BASE_DIR, "programs", "README.md")]:
        if not os.path.isfile(rpath):
            continue
        with open(rpath, "r", encoding="utf-8") as f:
            content = f.read()

        content = content.replace(r'($0, 1, 1, 2, 3, 5, 8, 13, 21, 34, ...$)', '(0, 1, 1, 2, 3, 5, 8, 13, 21, 34, ...)')
        content = content.replace(r'($\phi = 1.6180339887...$)', '(φ = 1.6180339887...)')
        content = content.replace(r'emitting $1!, 2!, 3!, 4!, ...$', 'emitting 1!, 2!, 3!, 4!, ...')
        content = content.replace(r'Streaming $\pi$ Spigot', 'Streaming π Spigot')
        content = content.replace(r'streaming $\pi$ spigot', 'streaming π spigot')
        content = content.replace(r'($64$ bytes)', '(64 bytes)')

        with open(rpath, "w", encoding="utf-8") as f:
            f.write(content)
        print(f"Fixed {rpath}")

def fix_examples():
    path = os.path.join(DOCS_DIR, "brainfuck", "examples.md")
    if not os.path.isfile(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    content = content.replace(r'($0, 1, 1, 2, 3, 5, 8, 13, 21, 34, ...$', '(0, 1, 1, 2, 3, 5, 8, 13, 21, 34, ...')
    content = content.replace(r'($\phi = \frac{1 + \sqrt{5}}{2} \approx 1.6180339887...$,', '(φ = (1 + √5)/2 ≈ 1.6180339887...,')
    content = content.replace(r'($1!, 2!, 3!, 4!, \dots$,', '(1!, 2!, 3!, 4!, ...,')
    content = content.replace(r'Streaming $\pi$ Spigot', 'Streaming π Spigot')
    content = content.replace(r'streaming $\pi$ spigot', 'streaming π spigot')
    content = content.replace(r'slots ($64$ bytes)', 'slots (64 bytes)')
    content = content.replace(r'($8,000$ slots)', '(8,000 slots)')
    content = content.replace(r'($233$)', '(233)')
    content = content.replace(r'[Streaming $\pi$ Spigot Deep-Dive]', '[Streaming π Spigot Deep-Dive]')

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Fixed {path}")

def fix_pi_spigot():
    path = os.path.join(DOCS_DIR, "brainfuck", "pi-spigot.md")
    if not os.path.isfile(path):
        return
    with open(path, "r", encoding="utf-8") as f:
        content = f.read()

    content = content.replace(r'Streaming $\pi$ Spigot', 'Streaming π Spigot')
    content = content.replace(r'streaming $\pi$ spigot', 'streaming π spigot')
    content = content.replace(r'digits of $\pi$', 'digits of π')
    content = content.replace(r'expansion for $\pi$:', 'expansion for π:')
    content = content.replace(r'lower-left element $s$ remains $0$', 'lower-left element s remains 0')
    content = content.replace(r'state matrix $\begin{pmatrix} q & r \\ 0 & t \end{pmatrix}$', 'state matrix (q, r, 0, t)')
    content = content.replace(r'iteration $k \ge 1$ with state matrix $(q, r, t)$:', 'iteration k ≥ 1 with state matrix (q, r, t):')
    content = content.replace(r'confirmed digit $n$:', 'confirmed digit n:')
    content = content.replace(r'digit $n$ is confirmed', 'digit n is confirmed')
    content = content.replace(r'ASCII digit $n$', 'ASCII digit n')
    content = content.replace(r'term $(k, 4k+2, 2k+1)$ where $l = 2k+1$:', 'term (k, 4k+2, 2k+1) where l = 2k+1:')
    content = content.replace(r'($8,000$ total slots available)', '(8,000 total slots available)')
    content = content.replace(r'($64$ bytes)', '(64 bytes)')
    content = content.replace(r'(4 $\to$ $N$)', '(4 → N)')

    # Fix display equations in list items to use block math format with proper indentation
    content = content.replace(
        "1. **Extract Digit Candidate**:\n   $$n = \\lfloor (3q + r) / t \\rfloor$$",
        "1. **Extract Digit Candidate**:\n\n    $$\n    n = \\lfloor (3q + r) / t \\rfloor\n    $$"
    )
    content = content.replace(
        "2. **Refinement Check**:\n   Test whether the interval bounds produce the same integer digit:\n   $$4q + 2r - 2t < n \\cdot t$$",
        "2. **Refinement Check**:\n    Test whether the interval bounds produce the same integer digit:\n\n    $$\n    4q + 2r - 2t < n \\cdot t\n    $$"
    )
    content = content.replace(
        "   - Scale matrix for base-10 streaming:\n     $$q \\leftarrow 10q, \\quad r \\leftarrow 10(r - n \\cdot t)$$",
        "   - Scale matrix for base-10 streaming:\n\n        $$\n        q \\leftarrow 10q, \\quad r \\leftarrow 10(r - n \\cdot t)\n        $$"
    )
    content = content.replace(
        "   - Ingest next term (k, 4k+2, 2k+1) where l = 2k+1:\n     $$r \\leftarrow q(4k+2) + r \\cdot l, \\quad t \\leftarrow t \\cdot l, \\quad q \\leftarrow q \\cdot k, \\quad l \\leftarrow l + 2, \\quad k \\leftarrow k + 1$$",
        "   - Ingest next term (k, 4k+2, 2k+1) where l = 2k+1:\n\n        $$\n        r \\leftarrow q(4k+2) + r \\cdot l, \\quad t \\leftarrow t \\cdot l, \\quad q \\leftarrow q \\cdot k, \\quad l \\leftarrow l + 2, \\quad k \\leftarrow k + 1\n        $$"
    )

    with open(path, "w", encoding="utf-8") as f:
        f.write(content)
    print(f"Fixed {path}")

if __name__ == "__main__":
    fix_snippets_clock_speeds()
    fix_config_menu()
    fix_terminal_commands()
    fix_architecture()
    fix_roadmap()
    fix_programs_readme()
    fix_examples()
    fix_pi_spigot()
    print("\nAll files processed successfully.")
