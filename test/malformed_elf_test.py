#!/usr/bin/env python3
"""Feed malformed ELF files to athrill3 and check that each is rejected cleanly.

Every variant is derived from one valid little-endian ELF32 or ELF64 file by
changing a single header field or cutting the file. athrill3 must refuse to
load it: exit with a normal non-zero status (no signal), print no
AddressSanitizer report, and never report a successful load. The valid file
itself must load. Works for any Athrill target (V850 ELF32, RISC-V ELF64).

usage: malformed_elf_test.py --athrill ATHRILL3 --memory MEMORY.TXT --elf VALID.ELF
"""
import argparse
import os
import struct
import subprocess
import sys
import tempfile

SHT_SYMTAB = 2
SHT_STRTAB = 3
PT_LOAD = 1


class Layout:
    """Field offsets and formats of the ELF header and its tables."""

    def __init__(self, data):
        self.is_64 = data[4] == 2
        if self.is_64:
            self.header = {"phoff": (32, "<Q"), "shoff": (40, "<Q"),
                           "phentsize": (54, "<H"), "phnum": (56, "<H"),
                           "shentsize": (58, "<H"), "shnum": (60, "<H"),
                           "shstrndx": (62, "<H")}
            self.program = {"type": (0, "<I"), "offset": (8, "<Q"),
                            "filesz": (32, "<Q"), "memsz": (40, "<Q")}
            self.section = {"name": (0, "<I"), "type": (4, "<I"),
                            "offset": (24, "<Q"), "size": (32, "<Q"),
                            "link": (40, "<I"), "entsize": (56, "<Q")}
            self.symbol_size = 24
            self.huge = 0xFFFFFFFFFFFFFFF0
        else:
            self.header = {"phoff": (28, "<I"), "shoff": (32, "<I"),
                           "phentsize": (42, "<H"), "phnum": (44, "<H"),
                           "shentsize": (46, "<H"), "shnum": (48, "<H"),
                           "shstrndx": (50, "<H")}
            self.program = {"type": (0, "<I"), "offset": (4, "<I"),
                            "filesz": (16, "<I"), "memsz": (20, "<I")}
            self.section = {"name": (0, "<I"), "type": (4, "<I"),
                            "offset": (16, "<I"), "size": (20, "<I"),
                            "link": (24, "<I"), "entsize": (36, "<I")}
            self.symbol_size = 16
            self.huge = 0xFFFFFFF0


def get(data, base, field):
    offset, fmt = field
    return struct.unpack_from(fmt, data, base + offset)[0]


def put(data, base, field, value):
    offset, fmt = field
    struct.pack_into(fmt, data, base + offset, value)


class Elf:
    def __init__(self, data):
        self.data = data
        self.layout = Layout(data)

    def header(self, name):
        return get(self.data, 0, self.layout.header[name])

    def program_base(self, index):
        return self.header("phoff") + index * self.header("phentsize")

    def section_base(self, index):
        return self.header("shoff") + index * self.header("shentsize")

    def section(self, index, name):
        return get(self.data, self.section_base(index), self.layout.section[name])

    def first_load(self):
        for index in range(self.header("phnum")):
            base = self.program_base(index)
            if get(self.data, base, self.layout.program["type"]) == PT_LOAD:
                return base
        raise SystemExit("valid ELF has no PT_LOAD")

    def symtab(self):
        for index in range(self.header("shnum")):
            if self.section(index, "type") == SHT_SYMTAB:
                return index
        raise SystemExit("valid ELF has no symbol table")


def variants(valid):
    """Yield (name, bytes) for every malformed variant."""
    elf = Elf(bytearray(valid))
    layout = elf.layout
    load = elf.first_load()
    symtab = elf.symtab()
    strtab = elf.section(symtab, "link")
    shstrndx = elf.header("shstrndx")

    def edit(change):
        data = bytearray(valid)
        change(data)
        return bytes(data)

    def header(name, value):
        return edit(lambda d: put(d, 0, layout.header[name], value))

    def program(name, value):
        return edit(lambda d: put(d, load, layout.program[name], value))

    def section(index, name, value):
        return edit(lambda d: put(d, elf.section_base(index), layout.section[name], value))

    def last_byte(index):
        end = elf.section(index, "offset") + elf.section(index, "size") - 1
        return edit(lambda d: d.__setitem__(end, ord("x")))

    first_symbol = elf.section(symtab, "offset")
    yield "shorter than the identification", valid[:10]
    yield "shorter than the header", valid[:30]
    yield "unknown class", edit(lambda d: d.__setitem__(4, 7))
    yield "big endian", edit(lambda d: d.__setitem__(5, 2))
    yield "program table offset huge", header("phoff", layout.huge)
    yield "program table count too large", header("phnum", 0xFFFF)
    yield "program entry size too small", header("phentsize", 1)
    yield "PT_LOAD offset outside the file", program("offset", len(valid) + 16)
    yield "PT_LOAD offset + size overflows", program("filesz", layout.huge)
    yield "PT_LOAD file size above memory size", edit(lambda d: (
        put(d, load, layout.program["filesz"], 0x100),
        put(d, load, layout.program["memsz"], 0x10)))
    yield "section table offset huge", header("shoff", layout.huge)
    yield "section table count too large", header("shnum", 0xFFFF)
    yield "section entry size too small", header("shentsize", 1)
    yield "section name table index out of range", header("shstrndx", elf.header("shnum"))
    yield "section contents outside the file", section(symtab, "offset", len(valid) + 16)
    yield "section name table not NUL-terminated", last_byte(shstrndx)
    yield "section name index huge", section(symtab, "name", 0xFFFFFF00)
    yield "symbol string table index out of range", section(symtab, "link", 0xFFFF)
    yield "symbol string table not NUL-terminated", last_byte(strtab)
    yield "symbol entry size wrong", section(symtab, "entsize", layout.symbol_size + 1)
    yield "symbol name index huge", edit(lambda d: struct.pack_into(
        "<I", d, first_symbol + layout.symbol_size, 0xFFFFFF00))
    yield "file cut before the section table", valid[:elf.header("shoff") + 4]


def run(athrill, memory, path):
    result = subprocess.run([athrill, "-c1", "-m", memory, "-t1000", path],
                            stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
                            timeout=30)
    return result.returncode, result.stdout.decode(errors="replace")


def loaded(output):
    return "loading succeeded" in output or "Elf loading was succeeded" in output


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--athrill", required=True)
    parser.add_argument("--memory", required=True)
    parser.add_argument("--elf", required=True)
    args = parser.parse_args()

    with open(args.elf, "rb") as file:
        valid = file.read()
    failures = 0
    status, output = run(args.athrill, args.memory, args.elf)
    if not loaded(output) or "AddressSanitizer" in output:
        print(f"FAIL valid ELF does not load (status {status})\n{output}")
        failures += 1
    else:
        print("PASS valid ELF loads")
    with tempfile.TemporaryDirectory() as directory:
        for name, data in variants(valid):
            path = os.path.join(directory, "variant.elf")
            with open(path, "wb") as file:
                file.write(data)
            status, output = run(args.athrill, args.memory, path)
            clean = status > 0 and "AddressSanitizer" not in output and not loaded(output)
            print(f"{'PASS' if clean else 'FAIL'} {name} (status {status})")
            if not clean:
                failures += 1
                print(output[-2000:])
    print("SELFTEST PASS" if failures == 0 else f"SELFTEST FAIL {failures}")
    return 0 if failures == 0 else 1


if __name__ == "__main__":
    sys.exit(main())
