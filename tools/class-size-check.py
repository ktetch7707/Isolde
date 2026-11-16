#!/usr/bin/env python3
"""
Class-size checker for Isolde C++ source files.

Detects oversized classes using a conservative regex-based parser
(libclang is not available). Limitations documented in docs/code-quality.md.

Limits enforced:
  - Maximum 300 lines per class body
  - Maximum 20 methods per class
  - Maximum 10 data members per class
  - Maximum 5 public methods per class

Usage:
    python tools/class-size-check.py [src_dir...]

Exits non-zero if any class exceeds a limit.
"""

import re
import sys
from pathlib import Path

MAX_CLASS_LINES = 300
MAX_METHODS = 20
MAX_DATA_MEMBERS = 10
MAX_PUBLIC_METHODS = 5


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
                result.append(c)
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


def find_class_bodies(code: str):
    """Yield (name, is_struct, start_idx, end_idx) for each class/struct body."""
    # Match: class/struct Name [ : access base... ] { ... }
    pattern = re.compile(
        r'\b(class|struct)\s+([A-Za-z_][A-Za-z0-9_]*)\s*'
        r'(?::\s*(?:public|protected|private)\s*)?'
        r'([A-Za-z_][A-Za-z0-9_]*(?:\s*,\s*[A-Za-z_][A-Za-z0-9_]*)*\s*)?'
        r'\s*\{',
        re.MULTILINE
    )

    for m in pattern.finditer(code):
        kind = m.group(1)
        name = m.group(2)
        brace_start = m.end() - 1  # position of opening brace

        # Find matching closing brace
        depth = 1
        j = brace_start + 1
        n = len(code)
        in_str = False
        str_char = ''
        in_lc = False
        in_bc = False

        while j < n:
            c = code[j]
            nxt = code[j + 1] if j + 1 < n else ''

            if in_lc:
                if c == '\n':
                    in_lc = False
                j += 1
                continue
            if in_bc:
                if c == '*' and nxt == '/':
                    in_bc = False
                    j += 2
                    continue
                j += 1
                continue
            if in_str:
                if c == '\\':
                    j += 2
                    continue
                if c == str_char:
                    in_str = False
                j += 1
                continue

            if c == '/' and nxt == '/':
                in_lc = True
                j += 2
                continue
            if c == '/' and nxt == '*':
                in_bc = True
                j += 2
                continue
            if c == '"' or c == "'":
                in_str = True
                str_char = c
                j += 1
                continue

            if c == '{':
                depth += 1
            elif c == '}':
                depth -= 1
                if depth == 0:
                    yield (name, kind == 'struct', brace_start + 1, j)
                    break
            j += 1


def count_class_metrics(body_code: str):
    """Count lines, methods, data members, public methods in a class body."""
    lines = [ln for ln in body_code.splitlines() if ln.strip()]
    line_count = len(lines)

    # Method pattern: return_type name(params) ... ; or { ... }
    # Match: type name ( args ) [ const ] [ override ] [ = 0 ] [ ; or { ]
    method_pattern = re.compile(
        r'(?:^|\s)'
        r'(?:static\s+|virtual\s+|inline\s+|explicit\s+|friend\s+)*'
        r'(?:[A-Za-z_][A-Za-z0-9:]*(?:\s*[&*]\s*)?\s+)'
        r'([A-Za-z_][A-Za-z0-9_]*)\s*\([^;{]*\)'
        r'(\s*(?:const|noexcept|override|final|virtual|static|explicit|inline)*\s*)*'
        r'(\s*=\s*0\s*)?'
        r'\s*[;{]',
        re.MULTILINE
    )

    methods = []
    for m in method_pattern.finditer(body_code):
        full_match = m.group(0).strip()
        method_name = m.group(1)
        # Skip constructors: explicit ClassName(args) where return type == class name
        if full_match.startswith("explicit ") and " " + method_name + "(" in full_match:
            continue
        # Skip return statements: return TypeName(args)
        if full_match.startswith("return "):
            continue
        methods.append(method_name)

    # Data member: type name [ = value ] ;  (not followed by ( )
    member_pattern = re.compile(
        r'(?:^|\s)'
        r'(?:static\s+|constexpr\s+|const\s+|mutable\s+)*'
        r'([A-Za-z_][A-Za-z0-9:]*(?:\s*[&*]\s*)?\s+)'
        r'([A-Za-z_][A-Za-z0-9_]*)\s*(?:\[[^\]]*\]\s*)*'
        r'(?:\s*=\s*[^;,]*)?\s*;',
        re.MULTILINE
    )

    members = member_pattern.findall(body_code)

    # Public methods: count methods after 'public:' until next access specifier
    public_method_count = 0
    current_access = 'public'  # struct defaults to public, class defaults to private
    # For struct, start as public; for class, start as private
    # We don't know class vs struct here from body alone, so default to private
    # (most conservative for class, less for struct - we'll check both)
    access_sections = re.split(r'(public\s*:|protected\s*:|private\s*:)', body_code)

    # First segment is before any access label
    segments = []
    current_access = 'private'  # default for class
    for seg in access_sections:
        if re.match(r'^(public|protected|private)\s*:$', seg.strip()):
            current_access = seg.strip().rstrip(':')
        elif seg.strip():
            segments.append((current_access, seg))

    for access, seg in segments:
        if access == 'public':
            seg_methods = 0
            for m in method_pattern.finditer(seg):
                fm = m.group(0).strip()
                if fm.startswith("explicit ") and " " + m.group(1) + "(" in fm:
                    continue
                if fm.startswith("return "):
                    continue
                seg_methods += 1
            public_method_count += seg_methods

    return {
        'lines': line_count,
        'methods': len(methods),
        'members': len(members),
        'public_methods': public_method_count,
    }


def check_file(path: Path):
    """Check a single file for class-size violations. Returns list of violation strings."""
    try:
        code = path.read_text(encoding='utf-8', errors='replace')
    except Exception as e:
        return [f"{path}: cannot read: {e}"]

    code = strip_comments_and_strings(code)
    violations = []

    for name, is_struct, start, end in find_class_bodies(code):
        body = code[start:end]
        metrics = count_class_metrics(body)

        if metrics['lines'] > MAX_CLASS_LINES:
            violations.append(
                f"{path}: class '{name}' has {metrics['lines']} lines (max {MAX_CLASS_LINES})"
            )
        if metrics['methods'] > MAX_METHODS:
            violations.append(
                f"{path}: class '{name}' has {metrics['methods']} methods (max {MAX_METHODS})"
            )
        if metrics['members'] > MAX_DATA_MEMBERS:
            violations.append(
                f"{path}: class '{name}' has {metrics['members']} data members (max {MAX_DATA_MEMBERS})"
            )
        if metrics['public_methods'] > MAX_PUBLIC_METHODS:
            violations.append(
                f"{path}: class '{name}' has {metrics['public_methods']} public methods (max {MAX_PUBLIC_METHODS})"
            )

    return violations


def main():
    paths = sys.argv[1:] if len(sys.argv) > 1 else ['src']
    all_violations = []

    for p in paths:
        base = Path(p)
        if not base.exists():
            print(f"Warning: path not found: {p}", file=sys.stderr)
            continue
        files = list(base.rglob('*.cpp')) + list(base.rglob('*.h')) + \
                list(base.rglob('*.hpp')) + list(base.rglob('*.cc')) + \
                list(base.rglob('*.cxx'))
        for f in sorted(files):
            all_violations.extend(check_file(f))

    if all_violations:
        print(f"CLASS-SIZE CHECK FAILED: {len(all_violations)} violation(s):")
        for v in all_violations:
            print(f"  {v}")
        sys.exit(1)

    print("Class-size check passed: no violations found.")
    sys.exit(0)


if __name__ == '__main__':
    main()
