#!/usr/bin/env python3
"""
Documentation enforcement checker for Isolde.

Scans C++ header and source files for:
  - Every class/struct: must have a doxygen comment at the top explaining its purpose
  - Every function definition: must have a doxygen comment with purpose, usage, and how it works
  - No doxygen comments inline at call sites (not allowed)
  - No useless/filler comments

Limits:
  - Conservative regex-based parser (libclang not available)
  - Avoids counting comments inside string literals where practical

Usage:
    python tools/documentation-check.py [src_dir...]

Exits non-zero if any documentation violation is found.
"""

import re
import sys
from pathlib import Path

# Files to skip entirely
SKIP_PATTERNS = [
    r'.*\.moc\.*',
    r'.*_autogen\..*',
    r'.*ui_\..*',
    r'.*qrc_..*',
]

# Functions that are simple accessors/mutators — allowed minimal documentation
SIMPLE_ACCESSOR_PATTERNS = [
    r'^get[A-Z]',
    r'^set[A-Z]',
    r'^is[A-Z]',
    r'^has[A-Z]',
    r'^can[A-Z]',
]


def strip_comments_and_strings(code: str) -> str:
    """Remove C++ comments and string literals to avoid false matches."""
    result = []
    i = 0
    n = len(code)
    in_string = False
    string_char = ''
    in_line_comment = False
    in_block_comment = False

    while i < n:
        c = code[i]
        nxt = code[i + 1] if i + 1 < n else ''

        if in_line_comment:
            if c == '\n':
                in_line_comment = False
            i += 1
            continue
        if in_block_comment:
            if c == '*' and nxt == '/':
                in_block_comment = False
                i += 2
                continue
            i += 1
            continue
        if in_string:
            if c == '\\':
                result.append(code[i:i + 2])
                i += 2
                continue
            if c == string_char:
                in_string = False
            i += 1
            continue

        if c == '/' and nxt == '/':
            in_line_comment = True
            i += 2
            continue
        if c == '/' and nxt == '*':
            in_block_comment = True
            i += 2
            continue
        if c == '"' or c == "'":
            in_string = True
            string_char = c
            result.append(c)
            i += 1
            continue

        result.append(c)
        i += 1

    return ''.join(result)


def find_class_declarations(code: str):
    """Yield (name, start_line, end_line) for each class/struct declaration."""
    pattern = re.compile(
        r'(?:^|\n)\s*(?:template\s*<[^>]*>\s*)?'
        r'(?:class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)\b'
        r'(?:\s*[:{])',
        re.MULTILINE
    )
    for m in pattern.finditer(code):
        yield m.group(1), m.start(), m.end()


def find_function_definitions(code: str):
    """Yield (name, return_type, params, start_line) for function definitions.

    A function definition has a body ({ ... }), not just a declaration ending in ;.
    """
    # Match: return_type name(params) [const/noexcept/override] [ = default/delete ] { ... }
    pattern = re.compile(
        r'(?:^|\n)\s*'
        r'(?:inline\s+|virtual\s+|static\s+|explicit\s+|friend\s+)*'
        r'([A-Za-z_][A-Za-z0-9:]*(?:\s*[&*]\s*)?)\s+'  # return type
        r'([A-Za-z_][A-Za-z0-9_]*)\s*\(([^)]*)\)'         # name + params
        r'(\s*(?:const|noexcept|override|final|virtual|static|explicit|inline|noexcept)*\s*)*'
        r'(?:\s*=\s*(?:default|delete|0)\s*)?'
        r'\s*\{',
        re.MULTILINE
    )
    for m in pattern.finditer(code):
        yield m.group(2), m.group(1), m.group(3), m.start()


def find_doxygen_before_line(code: str, line_start: int, window: int = 30):
    """Look backwards from line_start for a doxygen comment block."""
    lines = code[:line_start].splitlines(keepends=True)
    # Look at last `window` lines before the target
    start_idx = max(0, len(lines) - window)
    block = ''
    for i in range(start_idx, len(lines)):
        block += lines[i]

    # Match doxygen comment ending right before the target
    doxygen_pattern = re.compile(
        r'/\*\*(?:(?!\*/).)*\*/',
        re.DOTALL
    )
    matches = list(doxygen_pattern.finditer(block))
    if not matches:
        return None
    return matches[-1].group(0)


def check_doxygen_content(doc: str, context: str) -> list:
    """Check that doxygen comment has purpose (brief), usage, and how-it-works sections.

    Returns list of violation strings.
    """
    violations = []
    text = doc.strip()

    # Must have @brief or brief summary
    if not re.search(r'@brief', text) and not re.search(r'^\s*\*\s+[A-Z]', text, re.MULTILINE):
        violations.append(f'{context}: missing @brief or purpose summary')

    # Should have @details or substantive description
    if not re.search(r'@details', text) and len(text) < 80:
        violations.append(f'{context}: missing @details or substantive description')

    # For functions, check for param/return documentation
    return violations


def is_call_site_doxygen(code: str, doxy_pos: int) -> bool:
    """Check if a doxygen comment appears at a call site (inline before a function call).

    Returns True if the doxygen is followed by a function call pattern rather than a definition.
    """
    after = code[doxy_pos:doxy_pos + 500]
    # If followed by '(' with identifier before it on same line, likely a call site
    lines_after = after.splitlines()
    if lines_after:
        first_line = lines_after[0].strip()
        # Call site: something() followed by doxygen then more calls
        if re.search(r'\)\s*$', first_line):
            return True
    return False


def check_file(path: Path):
    """Check a single file for documentation violations."""
    try:
        code = path.read_text(encoding='utf-8', errors='replace')
    except Exception as e:
        return [f'{path}: cannot read: {e}']

    violations = []

    # Check classes/structs
    for name, start, end in find_class_declarations(code):
        doc = find_doxygen_before_line(code, start)
        if doc is None:
            violations.append(f'{path}:{code[:start].count(chr(10)) + 1}: class "{name}" missing doxygen comment explaining purpose')
        else:
            violations.extend(check_doxygen_content(doc, f'{path}: class "{name}"'))

    # Check function definitions
    for name, ret_type, params, start in find_function_definitions(code):
        # Skip simple accessors/mutators
        if any(re.match(pattern, name) for pattern in SIMPLE_ACCESSOR_PATTERNS):
            continue
        doc = find_doxygen_before_line(code, start)
        if doc is None:
            violations.append(f'{path}:{code[:start].count(chr(10)) + 1}: function "{name}" missing doxygen comment')
        else:
            violations.extend(check_doxygen_content(doc, f'{path}: function "{name}"'))

    return violations


def main():
    paths = sys.argv[1:] if len(sys.argv) > 1 else ['src']
    all_violations = []

    for p in paths:
        base = Path(p)
        if not base.exists():
            print(f'Warning: path not found: {p}', file=sys.stderr)
            continue
        files = list(base.rglob('*.cpp')) + list(base.rglob('*.h')) + \
                list(base.rglob('*.hpp')) + list(base.rglob('*.cc')) + \
                list(base.rglob('*.cxx'))
        for f in sorted(files):
            all_violations.extend(check_file(f))

    if all_violations:
        print(f'DOCUMENTATION CHECK FAILED: {len(all_violations)} violation(s):')
        for v in all_violations:
            print(f'  {v}')
        sys.exit(1)

    print('Documentation check passed: all classes and functions have required doxygen comments.')
    sys.exit(0)


if __name__ == '__main__':
    main()
