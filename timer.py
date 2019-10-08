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
        print(f"Iteration: {i + 1}", end="\n\n\n")
        times.append(get_time(exe, file, threads))
    times.sort()
    return statistics.mean(times[1:-1])


def print_results(results):
    for i in results:
        print(i)


def run(exe: str, file: str):
    final = 1
    results = []
    try:
        for threads in [1, 2, 4, 8, 16, 24, 32, 40, 50]:
            final = threads
            print(f"\n\n\nThreads: {threads}")
            results.append((threads, average(exe, file, threads)))
            threads *= 2
    except Exception as ex:
        print(f"Failed when threads = {final}")
        print(ex)
    finally:
        print_results(results)


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
