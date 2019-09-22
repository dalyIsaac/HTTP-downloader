#!/usr/bin/python3

import os
import shutil
import subprocess
import sys
from urllib.request import Request, urlopen

HEADERS = {"User-Agent": "getter"}


def create_dir(dirname: str):
    if os.path.exists(dirname):
        shutil.rmtree(dirname)
    os.mkdir(dirname)


def get_files(filename: str, legit_dirname: str):
    create_dir(legit_dirname)
    with open(filename, "r") as textfile:
        for name in textfile.readlines():
            target_filename = f"{legit_dirname}/{name.replace('/', '_')}".strip()
            url = "http://" + name.strip()
            req = Request(url, headers=HEADERS)
            with urlopen(req) as response, open(target_filename, "wb") as out_file:
                shutil.copyfileobj(response, out_file)


def run_downloader(filename: str, legit_dirname: str, custom_dirname: str, threads):
    create_dir(custom_dirname)
    subprocess.run(["./downloader", filename, threads, custom_dirname])
    with open("diffs.txt", "w") as diffs_file:
        subprocess.call(
            ["/usr/bin/diff", custom_dirname, legit_dirname], stdout=diffs_file
        )


def main():
    threads = sys.argv[1]
    files = sys.argv[2:]

    try:
        for name in files:
            legit_dirname = "legit_" + name
            custom_dirname = "custom_" + name
            filename = name + ".txt"
            get_files(filename, legit_dirname)
            run_downloader(filename, legit_dirname, custom_dirname, threads)
    except Exception as ex:
        print(ex)
        print("Usage: python3 e2e.py THREADS [small] [large]")


if __name__ == "__main__":
    main()
