import sys
import subprocess
import time

def run_to_completion(cmd_args):
    ret = -1
    totaltime = 0.0

    while ret != 0:
        start = time.time()
        ret = subprocess.call(cmd_args)
        end = time.time()
        totaltime += (end - start)

    print(totaltime)

if __name__ == "__main__":
    run_to_completion(sys.argv[1:])
