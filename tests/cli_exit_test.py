#!/usr/bin/env python3
"""Exercise early CLI exits without initialized SDL, plugins or managers."""
import os
from pathlib import Path
import subprocess
import sys
import tempfile

binary = Path(sys.argv[1]).resolve()
cases = [(["--help"], 0, "Usage:"), (["-h"], 0, "Usage:"),
         (["--version"], 0, "Veloce v"), (["-v"], 0, "Veloce v"),
         (["--unknown-veloce-option"], 1, "Unknown option:"),
         (["missing-cartridge.nes", "--help"], 0, "Usage:"),
         (["--debug", "--version"], 0, "Veloce v")]
with tempfile.TemporaryDirectory(prefix="veloce-cli-") as directory:
    env = os.environ.copy()
    env.update(HEADLESS="1", FRAMES="invalid",
               VELOCE_TEST_OUT=str(Path(directory) / "unexpected.result"))
    for args, code, message in cases:
        result = subprocess.run([str(binary), *args], cwd=directory, env=env,
                                capture_output=True, text=True, timeout=5)
        assert result.returncode == code, (args, result.returncode, result.stdout, result.stderr)
        assert message in result.stdout + result.stderr, (args, result.stdout, result.stderr)
        assert not list(Path(directory).iterdir()), "CLI exit created runtime artifacts"
print(f"{len(cases)} CLI early exits and no runtime initialization PASS")
