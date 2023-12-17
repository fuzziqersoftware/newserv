#!/bin/env python3

import argparse
import os
import subprocess
import sys


def get_ip_address(ifname):
    data = subprocess.check_output(["ifconfig", ifname])
    for line in data.splitlines():
        line = line.strip()
        if line.startswith(b"inet "):
            return line.split()[1].decode("ascii")
    raise RuntimeError("cannot get address for interface " + ifname)


def main(argv):
    parser = argparse.ArgumentParser()
    parser.add_argument("--pid", "-p", type=int, default=0)
    parser.add_argument("orig_destination", type=str)
    parser.add_argument("new_destination", type=str, default=None, nargs="?")
    args = parser.parse_args()

    if os.geteuid() != 0:
        raise RuntimeError("You must use sudo to run this script")
    new_destination = args.new_destination or get_ip_address("en0")
    proc_arg = "Flycast.app" if args.pid == 0 else str(args.pid)

    print(f'Finding occurrences of "{args.orig_destination}"')
    addresses_str = subprocess.check_output(
        ["memwatch", proc_arg, "find", f'"{args.orig_destination}"']
    )
    for line in addresses_str.splitlines():
        # line is like '(0) 00007FFF038500A0 (rw-)' (we care only about the address)
        tokens = line.split()
        if len(tokens) != 3:
            continue
        print(
            f'Replacing "{args.orig_destination}" with "{new_destination}" at {tokens[1]} in Flycast'
        )
        subprocess.check_call(
            ["memwatch", proc_arg, "write", tokens[1], f'"{new_destination}" 00']
        )

    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
