import sys
import pandas
import matplotlib.pyplot as plt
import numpy as np
from os.path import splitext

def plot_file(fname, label):
    df = pandas.read_csv(fname)

    x = df.iloc[:,0].values
    y = np.mean(df.iloc[:,1:].values, 1)

    plt.plot(x, y, label=label)

def main():
    if len(sys.argv) < 5:
        return "Usage: plot_results.py output.pdf rm.csv rc.csv blcr.csv"

    outfname = sys.argv[1]
    infnames = sys.argv[2:]

    labels = ["Infiniband", "RAMCloud", "BLCR"]

    for fname, label in zip(infnames, labels):
        plot_file(fname, label)

    if "commit" in outfname:
        plt.title("Commit Micro-benchmark")
    else:
        plt.title("Recovery Micro-benchmark")

    plt.xlabel("Num. Pages")
    plt.ylabel("Time (s)")
    plt.legend()
    plt.savefig(outfname)

if __name__ == "__main__":
    sys.exit(main())
