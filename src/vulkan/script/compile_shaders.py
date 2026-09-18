#!/usr/bin/env python3
"""Compile a shader file or directory to SPIR-V, preserving relative paths."""

import argparse
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


SHADER_SUFFIXES = {
    ".vert", ".frag", ".comp", ".geom", ".tesc", ".tese", ".mesh", ".task",
    ".rgen", ".rint", ".rahit", ".rchit", ".rmiss", ".rcall",
}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("-s", required=True, type=Path, help="Shader source file or directory")
    parser.add_argument("-o", required=True, type=Path, help="Output directory")
    parser.add_argument("--target-env", default="vulkan1.3", help="glslc target environment (default: vulkan1.3)")
    parser.add_argument("--glslc", default="glslc", help="glslc executable path or name")
    args = parser.parse_args()

    try:
        source = args.s.resolve(strict=True)
        if source.is_dir():
            source_root = source
            shaders = sorted(p for p in source.rglob("*") if p.is_file() and p.suffix in SHADER_SUFFIXES)
        elif source.is_file():
            source_root = source.parent
            shaders = [source]
        else:
            raise ValueError(f"Not a shader file or directory: {source}")

        if not shaders:
            print(f"No shader sources in {source}")
            return 0

        compiler = shutil.which(args.glslc)
        if compiler is None:
            raise ValueError(f"glslc not found: {args.glslc}")

        output_root = args.o.resolve()
        for shader in shaders:
            relative = shader.relative_to(source_root)
            output = output_root / relative.parent / (relative.name + ".spv")
            output.parent.mkdir(parents=True, exist_ok=True)
            # Publish only successful compiler output; preserve an existing file on failure.
            with tempfile.TemporaryDirectory(prefix=".shader-", dir=output.parent) as temp:
                compiled = Path(temp) / output.name
                subprocess.run(
                    [compiler, f"--target-env={args.target_env}", str(shader), "-o", str(compiled)],
                    check=True,
                )
                if not output.exists() or output.read_bytes() != compiled.read_bytes():
                    compiled.replace(output)
            print(f"{relative} -> {output}")
        return 0
    except (OSError, ValueError, subprocess.CalledProcessError) as error:
        print(f"Shader compilation failed: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    sys.exit(main())
