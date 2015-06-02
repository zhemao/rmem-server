import sys
import pandas
from scipy.stats import linregress
import numpy as np

def main():
    if len(sys.argv) < 2:
        return "Usage: calc_regression.py results.csv"

    df = pandas.read_csv(sys.argv[1])
    x = df.iloc[:,0].values
    y = np.mean(df.iloc[:,1:].values, 1)

    slope, intercept, r_value, _, _ = linregress(x, y)

    print("y = {a} x + {b}".format(a = slope, b = intercept))
    print("R^2 = {}".format(r_value ** 2))

if __name__ == "__main__":
    sys.exit(main())
