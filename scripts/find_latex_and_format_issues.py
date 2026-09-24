#!/usr/bin/env python3
"""
Scan documentation/docs for LaTeX expressions, unrendered math formulas,
and formatting issues.
"""

import os
import re
import glob

DOCS_DIR = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", "documentation", "docs"))

# Patterns to detect
PATTERNS = [
    ("Display Math ($$...$$)", re.compile(r'\$\$(.+?)\$\$', re.DOTALL)),
    ("Inline Math ($...$)", re.compile(r'(?<!\$)\$(?!\$)(.+?)(?<!\$)\$(?!\$)')),
    ("LaTeX Command (\\cmd)", re.compile(r'\\[a-zA-Z]+(?:\{.*?\})*')),
    ("LaTeX Environment (\\begin)", re.compile(r'\\begin\{.+?\}.*?\\end\{.+?\}', re.DOTALL)),
    ("LaTeX delimiters (\\[, \\()", re.compile(r'\\\[|\\\]|\\\(|\\\)')),
]

def scan_docs():
    md_files = glob.glob(os.path.join(DOCS_DIR, "**", "*.md"), recursive=True)
    findings = []

    for fpath in sorted(md_files):
        rel_path = os.path.relpath(fpath, DOCS_DIR)
        with open(fpath, "r", encoding="utf-8", errors="replace") as f:
            content = f.read()
            lines = content.splitlines()

        # Check display math ($$ ... $$)
        for m in re.finditer(r'\$\$(.+?)\$\$', content, re.DOTALL):
            matched_text = m.group(0)
            start_pos = m.start()
            line_no = content[:start_pos].count('\n') + 1
            findings.append({
                "file": rel_path,
                "line": line_no,
                "type": "Display Math ($$...$$)",
                "snippet": matched_text.strip().replace('\n', ' ')[:100]
            })

        # Check line by line for inline math or LaTeX commands outside code blocks
        in_code_block = False
        in_math_block = False
        for idx, line in enumerate(lines, 1):
            stripped = line.strip()
            if stripped.startswith("```"):
                in_code_block = not in_code_block
                continue
            if in_code_block:
                continue

            # Track $$ display math blocks
            if stripped.startswith("$$") and stripped.endswith("$$") and len(stripped) > 2:
                # Single line display math
                continue
            elif stripped == "$$":
                in_math_block = not in_math_block
                continue

            if in_math_block:
                continue

            # Remove inline code spans (e.g. `foo`) so backslash in code isn't flagged as LaTeX
            line_no_code = re.sub(r'`[^`]+`', '', line)

            # Check for inline math $...$
            # Ignore currency like $4 or $10
            inline_maths = re.findall(r'(?<![\$\w])\$([^\$]+?)\$(?![\$\w])', line_no_code)
            for im in inline_maths:
                # If it's just numbers, probably currency, e.g. $4
                if re.match(r'^\d+(\.\d+)?$', im.strip()):
                    continue
                findings.append({
                    "file": rel_path,
                    "line": idx,
                    "type": "Inline Math ($...$)",
                    "snippet": f"${im}$"[:100]
                })

            # Check for raw LaTeX commands not in math, like \text, \times, \frac, etc.
            latex_cmds = re.findall(r'(\\[a-zA-Z]+(?:\{[^\}]*\})*)', line_no_code)
            for cmd in latex_cmds:
                if cmd in ["\\n", "\\r", "\\t", "\\0", "\\b", "\\x1b"]:
                    continue
                # If not inside a finding already recorded for this line
                if not any(f["file"] == rel_path and f["line"] == idx for f in findings):
                    findings.append({
                        "file": rel_path,
                        "line": idx,
                        "type": "Raw LaTeX command",
                        "snippet": line.strip()[:100]
                    })

    print(f"Total findings: {len(findings)}\n")
    by_file = {}
    for f in findings:
        by_file.setdefault(f["file"], []).append(f)

    for fname, items in by_file.items():
        print(f"=== {fname} ({len(items)} items) ===")
        for it in items:
            print(f"  Line {it['line']:4d} [{it['type']}]: {it['snippet']}")
        print()

if __name__ == "__main__":
    scan_docs()
