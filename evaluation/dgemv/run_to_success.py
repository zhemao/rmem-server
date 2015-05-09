import sys
import subprocess
import time
import shlex
import os

def run_to_completion(setup_cmd, run_cmd):
    devnull = open(os.devnull, "w")
    start = time.time()
    ret = subprocess.call(shlex.split(setup_cmd), stdout=devnull)
    end = time.time()
    totaltime = (end - start)

    while ret != 0:
        start = time.time()
        ret = subprocess.call(shlex.split(run_cmd), stdout=devnull)
        end = time.time()
        totaltime += (end - start)

    print(totaltime)
    devnull.close()

if __name__ == "__main__":
    run_to_completion(*sys.argv[1:])
