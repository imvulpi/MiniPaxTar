# import subprocess
# import sys
# from pathlib import Path

# SCRIPT_DIR = Path(__file__).resolve().parent
# ROOT = SCRIPT_DIR.parent

# from generate_single_header import generate_single_header

# def main():
#     try:
#         print("Generating single-header dist/minipaxtar.h...")
#         generate_single_header()
        
#         dist_file = ROOT / "dist" / "minipaxtar.h"
#         subprocess.run(["git", "add", str(dist_file)], check=True)

#         print("Successfully updated and staged dist/minipaxtar.h")
#     except Exception as e:
#         print(f"Pre-commit hook failed: {e}", file=sys.stderr)
#         sys.exit(1)

# if __name__ == "__main__":
#     main()