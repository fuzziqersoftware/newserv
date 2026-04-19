#!/usr/bin/env python3

import os
import shutil
import subprocess
import sys
from typing import Callable


def filter_directory(dir: str, predicate: Callable[[str], bool]):
    for filename in os.listdir(dir):
        if not predicate(filename):
            path = os.path.join(dir, filename)
            if os.path.isfile(path):
                os.remove(path)
            else:
                shutil.rmtree(path)


def main():
    print("Deleting existing release directory")
    if os.path.exists("release"):
        shutil.rmtree("release")
    if os.path.exists("release.zip"):
        os.remove("release.zip")
    os.mkdir("release")

    print("Adding executables")
    shutil.copy("newserv", "release/newserv-macos")
    shutil.copy("newserv.exe", "release/newserv-windows.exe")
    shutil.copy("README.md", "release/README.md")

    print("Adding system directory")
    shutil.copytree("system", "release/system")

    print("Removing instance-based and temporary files")
    filter_directory(
        "release/system",
        lambda filename: (not filename.endswith(".json"))
        or filename == "config.example.json",
    )
    filter_directory(
        "release/system/ep3", lambda filename: not filename.startswith("cardtex")
    )
    filter_directory(
        "release/system/client-functions",
        lambda filename: filename not in ("Debug-Private", "FastLoading", "notes.txt"),
    )
    filter_directory("release/system/dol", lambda filename: False)
    filter_directory("release/system/ep3/banners", lambda filename: False)
    filter_directory("release/system/ep3/battle-records", lambda filename: False)
    filter_directory("release/system/licenses", lambda filename: False)
    filter_directory("release/system/players", lambda filename: False)
    filter_directory(
        "release/system/quests",
        lambda filename: filename not in ("private", "includes"),
    )
    filter_directory("release/system/teams", lambda filename: filename == "base.json")
    subprocess.check_call(["find", "release", "-name", ".DS_Store", "-delete"])
    subprocess.check_call(["find", "release", "-name", "*.WIP-s", "-delete"])

    print("Setting up configuration")
    os.rename("release/system/config.example.json", "release/system/config.json")


if __name__ == "__main__":
    sys.exit(main())
