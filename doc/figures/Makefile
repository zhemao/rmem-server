GRAPH_DATA=$(wildcard *.csv)
GRAPHS=$(GRAPH_DATA:.csv=.pdf)
DRAWING_SOURCES=$(wildcard *.svg)
DRAWINGS=$(DRAWING_SOURCES:.svg=.pdf)
FIGURES=$(GRAPHS) $(DRAWINGS)

INKSCAPE=inkscape
PYTHON=python

all: $(FIGURES)

%.pdf: %.csv
	$(PYTHON) plot_results.py $<

%.pdf: %.svg
	$(INKSCAPE) -D $< -A $@

clean:
	rm -f $(FIGURES)
