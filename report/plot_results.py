import sys
import pandas
import matplotlib.pyplot as plt
import numpy as np
from os.path import splitext

def main():
    if len(sys.argv) < 2:
        print("Usage: plot_results.py title results.csv")
    title = sys.argv[1]
    infname = sys.argv[2]
    df = pandas.read_csv(infname)

    x = df.iloc[:,0].values
    y = np.mean(df.iloc[:,1:].values, 1)

    outname, _ = splitext(infname)
    outname += ".png"

    plt.xscale('log')
    plt.yscale('log')
    plt.title(title)
    plt.xlabel("Num. Pages")
    plt.ylabel("Time (s)")
    plt.plot(x, y)
    plt.savefig(outname)

if __name__ == "__main__":
    main()
