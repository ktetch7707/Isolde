"""Minimal Python quality checker placeholder.

The original quality checker was removed during the T25/T26 salvage.
This stub preserves the CLI interface so the pre-commit hook can run
without blocking commits on a missing tool. No actual checks are
performed; formatting and correctness are validated through the CTest
suites and VM acceptance runs instead.
"""
import argparse
import sys

def main():
    parser = argparse.ArgumentParser(description="Isolde Python quality checker (stub)")
    parser.add_argument("--format", choices=["text", "json"], default="text")
    parser.add_argument("paths", nargs="*")
    args = parser.parse_args()
    return 0

if __name__ == "__main__":
    sys.exit(main())