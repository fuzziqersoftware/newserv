import collections
import os
import subprocess
import sys
from dataclasses import dataclass


version_tokens = ("3OJ2", "3OJ3", "3OJ4", "3OJ5", "3OE0", "3OE1", "3OE2", "3OP0")


@dataclass
class WriteRegion:
    address: int
    data: list[int]


def disassemble_opcode(opcode: int, start_address: int) -> str:
    try:
        result = subprocess.check_output(
            [
                "m68kdasm",
                f"--start-address={hex(start_address)}",
                "--ppc32",
                "--parse-data",
            ],
            input=f"{opcode:08X}".encode("ascii"),
        )
        return result.decode("ascii").strip().split(None, 2)[2]
    except Exception:
        return ""


def write_patches_for_code(
    out_dir: str,
    name: str,
    version_to_lines: dict[str, dict[int, int]],
    long_name: str | None,
    desc: str | None,
) -> None:
    for v, lines in version_to_lines.items():
        write_regions: list[WriteRegion] = []
        for addr, value in sorted(lines.items()):
            if write_regions and (
                write_regions[-1].address + len(write_regions[-1].data) * 4 == addr
            ):
                write_regions[-1].data.append(value)
            else:
                write_regions.append(WriteRegion(address=addr, data=[value]))

        if write_regions:
            filename = os.path.join(
                out_dir,
                f'{name.replace(" ", "")}.{v}.patch.s',
            )
            with open(filename, "wt") as f:
                if long_name is not None:
                    f.write(f'.meta name="{long_name}"\n')
                if desc is not None:
                    f.write(f'.meta description="{desc}"\n')
                f.write("\n")
                f.write("entry_ptr:\n")
                f.write("reloc0:\n")
                f.write("  .offsetof start\n")
                f.write("start:\n")
                f.write("  .include  WriteCodeBlocksGC\n")
                for region in write_regions:
                    f.write(
                        f"  # region @ {region.address:08X} ({len(region.data) * 4} bytes)\n"
                    )
                    f.write(f"  .data     0x{region.address:08X}  # address\n")
                    f.write(f"  .data     0x{(len(region.data) * 4):08X}  # size\n")
                    for z, value in enumerate(region.data):
                        addr = region.address + (z * 4)
                        disassembly = disassemble_opcode(value, addr)
                        f.write(
                            f"  .data     0x{value:08X}  # {addr:08X} => {disassembly}\n"
                        )
                f.write("  # end sentinel\n")
                f.write("  .data     0x00000000  # address\n")
                f.write("  .data     0x00000000  # size\n")
            print(f"... {filename}")
        else:
            print(f"*** {filename} (no data to write)")


def main():
    if len(sys.argv) != 3:
        raise RuntimeError(
            "Usage: python3 generate-patches.py <source-filename> <out-dir>"
        )
    src_file = sys.argv[1]
    out_dir = sys.argv[2]

    with open(src_file, "rt") as f:
        lines = f.read().splitlines()

    reading_code = False
    reading_patch = False
    code_name = ""
    version_name = ""
    name_to_version_to_lines = collections.defaultdict(
        lambda: collections.defaultdict(dict)
    )  # {name:{version: {addr: value}}}
    name_to_long_name = {}
    name_to_description = {}
    for line in lines:
        if not line:
            reading_code = False
            reading_patch = False
        elif reading_code:
            for z, v in enumerate(version_tokens):
                addr_str = line[18 * z : 18 * z + 8]
                value_str = line[18 * z + 9 : 18 * z + 17]
                if addr_str != "        " and value_str != "        ":
                    addr = int(addr_str, 16)
                    if addr in name_to_version_to_lines[code_name][v]:
                        raise ValueError(f"duplicate write to address {addr:08X}")
                    name_to_version_to_lines[code_name][v][addr] = int(value_str, 16)
        elif line.startswith("*** name="):
            name_to_long_name[code_name] = line[9:]
        elif line.startswith("*** desc="):
            name_to_description[code_name] = line[9:]
        elif line.startswith("======== PsoV3-"):
            reading_patch = True
            version_name = line[15:].split(".")[0]
        elif reading_patch:
            addr_str, data_str = line.split()
            addr = int(addr_str, 16)
            data = bytes.fromhex(data_str)
            for z in range(0, len(data), 4):
                name_to_version_to_lines[code_name][version_name][addr + z] = (
                    (data[z] << 24)
                    | (data[z + 1] << 16)
                    | (data[z + 2] << 8)
                    | (data[z + 3] << 0)
                )
        elif line.startswith("3OJ2------------"):
            reading_code = True
        else:
            code_name = line

    for name, version_to_lines in name_to_version_to_lines.items():
        write_patches_for_code(
            out_dir,
            name,
            version_to_lines,
            name_to_long_name.get(name),
            name_to_description.get(name),
        )


if __name__ == "__main__":
    main()
