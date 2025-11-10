import os
import shutil
import subprocess

def gen_ispc():
    src_file = "../extern/bc7enc_rdo/bc7e.ispc"

    targets = {
        "avx":   ("bc7e_", "bc7e_avx_", "BC7E_", "BC7E_AVX_"),
        "avx2":  ("bc7e_", "bc7e_avx2_", "BC7E_", "BC7E_AVX2_"),
        "sse2":  ("bc7e_", "bc7e_sse2_", "BC7E_", "BC7E_SSE2_"),
        "sse4":  ("bc7e_", "bc7e_sse4_", "BC7E_", "BC7E_SSE4_")
    }

    if not os.path.exists(src_file):
        print(f"{src_file} unable to get the file.")
        return

    with open(src_file, "r", encoding="utf-8") as f:
        src_text = f.read()

    for key, (lower_old, lower_new, upper_old, upper_new) in targets.items():
        dst_file = f"../extern/bc7enc_rdo/bc7e_{key}.ispc"
        modified = src_text.replace(upper_old, upper_new).replace(lower_old, lower_new)

        with open(dst_file, "w", encoding="utf-8") as f:
            f.write(modified)

        print(f"{dst_file} generate done")

def run_ispc():
    targets = [
        "avx",
        "avx2",
        "sse2",
        "sse4",
    ]
    
    common_opts = [
        "-O2",
        "--opt=fast-math",
        "--opt=disable-assertions"
    ]

    for target in targets:
        src = os.path.join(f"../extern/bc7enc_rdo/bc7e_{target}.ispc")
        obj = os.path.join(f"../build/bc7e_{target}.obj")
        header = os.path.join(f"bc7e_ispc_{target}.h")

        cmd = [
            "ispc",
            "-O2",
            src,
            "-o", obj,
            "-h", header,
            "--target", target
        ] + common_opts

        result = subprocess.run(cmd, capture_output=True, text=True)

        if result.returncode == 0:
            print(f"{target.upper()} build done")
        else:
            print(f"{target.upper()} failed to build")

def main():
    gen_ispc()
    run_ispc()

if __name__ == "__main__":
    main()
