"""
compiler_runner.py — Thin subprocess wrapper around the CloudPol compiler binary.
"""

import subprocess
import os
import tempfile
from pathlib import Path


# ─────────────────────────────────────────────────────────────────────────────
# Locate compiler binary
# ─────────────────────────────────────────────────────────────────────────────

def _find_binary() -> str:
    """Find the cloudpol binary relative to this file's directory."""
    here = Path(__file__).parent
    candidates = [
        here.parent / "build" / "cloudpol",
        here.parent / "cloudpol",
    ]
    for c in candidates:
        if c.exists() and os.access(c, os.X_OK):
            return str(c)
    raise FileNotFoundError(
        "Could not find the 'cloudpol' compiler binary.\n"
        "Build it first with: make  (from the project root)"
    )


# ─────────────────────────────────────────────────────────────────────────────
# Public API
# ─────────────────────────────────────────────────────────────────────────────

class CompilerResult:
    """Holds the result of a compiler run."""

    def __init__(self, returncode: int, stdout: str, stderr: str,
                 pol_file: str, iam_file: str | None = None):
        self.returncode = returncode
        self.stdout = stdout
        self.stderr = stderr
        self.pol_file = pol_file
        self.iam_file = iam_file
        self.success = (returncode == 0)

    def __str__(self) -> str:
        lines = []
        if self.stdout:
            lines.append(self.stdout.strip())
        if self.stderr:
            lines.append(self.stderr.strip())
        return "\n".join(lines)


def run(pol_source: str, emit_iam: bool = True, emit_report: bool = False,
        emit_mlir: bool = False) -> CompilerResult:
    """
    Write pol_source to a temp .pol file and run the cloudpol compiler on it.

    Parameters
    ----------
    pol_source   : CloudPol DSL source text
    emit_iam     : Whether to generate IAM JSON (default True)
    emit_report  : Whether to generate Markdown report
    emit_mlir    : Whether to emit MLIR text

    Returns
    -------
    CompilerResult with returncode, stdout, stderr, and optional IAM JSON content.
    """
    binary = _find_binary()

    # Write source to a temporary file
    with tempfile.NamedTemporaryFile(
        mode="w", suffix=".pol", delete=False, prefix="cloudpol_"
    ) as f:
        f.write(pol_source)
        pol_path = f.name

    iam_path = None
    iam_content = None

    try:
        args = [binary]

        if emit_mlir:
            args.append("--emit-mlir")

        if emit_iam:
            iam_path = pol_path.replace(".pol", ".json")
            args += ["--emit-iam", "-o", iam_path]

        if emit_report:
            report_path = pol_path.replace(".pol", "_report.md")
            args += ["--emit-report", "-o", report_path]

        args.append(pol_path)

        result = subprocess.run(
            args,
            capture_output=True,
            text=True,
            timeout=30,
        )

        if emit_iam and iam_path and os.path.exists(iam_path):
            with open(iam_path) as f:
                iam_content = f.read()

        return CompilerResult(
            returncode=result.returncode,
            stdout=result.stdout,
            stderr=result.stderr,
            pol_file=pol_path,
            iam_file=iam_content,
        )

    finally:
        # Clean up temp files (keep .pol for debugging; remove .json)
        if iam_path and os.path.exists(iam_path):
            try:
                os.unlink(iam_path)
            except OSError:
                pass


def format_result(result: CompilerResult) -> str:
    """Return a user-friendly string summarising the compiler result."""
    sections = []

    if result.success:
        sections.append("✅  **Compilation successful!**")
    else:
        sections.append("❌  **Compilation failed.**")

    # Compiler pipeline stdout (strip ANSI, keep meaningful lines)
    if result.stdout:
        lines = [l for l in result.stdout.splitlines()
                 if l.strip() and not l.startswith("[CloudPol] Phase")]
        if lines:
            sections.append("\n".join(lines))

    # Errors
    if result.stderr:
        sections.append("**Compiler errors/warnings:**\n```\n" + result.stderr.strip() + "\n```")

    # IAM JSON output
    if result.iam_file:
        sections.append("**Generated AWS IAM Policy JSON:**\n```json\n" + result.iam_file.strip() + "\n```")

    return "\n\n".join(sections)
