import argparse
import shutil
import subprocess
import sys
from pathlib import Path

# REQUIRES: doxygem, doxybook2, python, mkdocs

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent
DOCS_DIR = ROOT / "docs"
DOCS_API_DIR = DOCS_DIR / "api"
XML_DIR = DOCS_DIR / "xml"


def clear_docs():
    for path in (DOCS_API_DIR, XML_DIR):
        shutil.rmtree(path, ignore_errors=True)
    print("Documentation artifacts cleared.")


def main():
    parser = argparse.ArgumentParser(description="Manage project documentation workflow.")
    parser.add_argument(
        "action",
        nargs="?",
        default="build",
        choices=["build", "clear"],
        help="Choose whether to build or clear the documentation."
    )
    args = parser.parse_args()

    if args.action == "clear":
        clear_docs()
        return

    try:
        DOCS_API_DIR.mkdir(parents=True, exist_ok=True)

        print("\n---- RUNNING DOXYGEN ----\n")
        subprocess.run(["doxygen", str(ROOT / "DoxyFile")], check=True)

        print("\n---- RUNNING DOXYBOOK2 ----\n")
        subprocess.run(
            ["doxybook2", "-i", str(XML_DIR), "-o", str(DOCS_API_DIR), "-c", "doxybook2.json"],
            check=True
        )

        print("\n---- RUNNING MKDOCS ----\n")
        subprocess.run(["mkdocs", "serve", "--config-file", str(ROOT / "mkdocs.yml")], check=True)

        print("\nDOCS GENERATED\n")

    except Exception as e:
        print(f"Generating docs failed: {e}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()