import sys
import subprocess
import statistics

from sys import stderr


USAGE = "USAGE: python3 ./timer.py [downloader] [file]"


def get_time(exe: str, file: str, threads: int):
    args = ["time", "-f", "%e", exe, file, str(threads), "timer"]
    result = subprocess.run(args, stderr=subprocess.PIPE)
    return float(result.stderr.decode())


def average(exe: str, file: str, threads: int):
    times = []
    for i in range(5):
        times.append(get_time(exe, file, threads))
    times.sort()
    return statistics.mean(times[1:-1])


def print_results(results):
    for i in results:
        print(i)


def run(exe: str, file: str):
    print(exe)
    i = 1
    results = []
    while i <= 64:
        results.append((i, average(exe, file, i)))
        i *= 2
    print_results(results)
    print("\n\n")


def main():
    exe = ""
    file = ""
    try:
        exe = sys.argv[1]
        file = sys.argv[2]
    except Exception as ex:
        print(USAGE)
        print("\n\n")
        print(ex, file=stderr)

    run(exe, file)


if __name__ == "__main__":
    main()
