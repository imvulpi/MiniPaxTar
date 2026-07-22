from pathlib import Path

SCRIPT_DIR = Path(__file__).resolve().parent
ROOT = SCRIPT_DIR.parent

HEADER_PATH = ROOT / "minipaxtar.h"
SRC_PATH = ROOT / "minipaxtar.c"
OUTPUT_PATH = ROOT / "dist" / "minipaxtar.h"

def generate_single_header():
    if not HEADER_PATH.exists() or not SRC_PATH.exists():
        raise FileNotFoundError("Source files missing!")
    
    header_code = HEADER_PATH.read_text(encoding="utf-8")
    src_code = SRC_PATH.read_text(encoding="utf-8")

    # Filter out local header inclusion to prevent duplication
    src_clean = "\n".join(
        line for line in src_code.splitlines() 
        if not line.strip().startswith('#include "minipaxtar.h"')
    )

    single_header = f"""/* 
 * minipaxtar.h - Single Header Distribution
 * Generated automatically. Do not edit directly.
 */

{header_code}
#ifdef MINIPAXTAR_IMPLEMENTATION
{src_clean}

#endif /* MINIPAXTAR_IMPLEMENTATION */
"""

    OUTPUT_PATH.parent.mkdir(exist_ok=True)
    OUTPUT_PATH.write_text(single_header, encoding="utf-8")

if __name__ == "__main__":
    generate_single_header()