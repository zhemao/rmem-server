import sys
import pandas
import matplotlib.pyplot as plt
import numpy as np
from os.path import splitext

def main():
    if len(sys.argv) < 2:
        return "Usage: plot_results.py results.csv"

    infname = sys.argv[1]
    df = pandas.read_csv(infname)

    x = df.iloc[:,0].values
    y = np.mean(df.iloc[:,1:].values, 1)

    outname, _ = splitext(infname)
    outname += ".pdf"

    plt.xlabel("Num. Pages")
    plt.ylabel("Time (s)")
    plt.plot(x, y)
    plt.savefig(outname)

if __name__ == "__main__":
    sys.exit(main())
