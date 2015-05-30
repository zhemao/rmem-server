GRAPHS=commit-results.pdf recovery-results.pdf
DRAWING_SOURCES=$(wildcard *.svg)
DRAWINGS=$(DRAWING_SOURCES:.svg=.pdf)
FIGURES=$(GRAPHS) $(DRAWINGS)

INKSCAPE=inkscape
PYTHON=python

all: $(FIGURES)

%.pdf: %-rm.csv %-rc.csv %-blcr.csv
	$(PYTHON) plot_results.py $@ $^

%.pdf: %.svg
	$(INKSCAPE) -D $< -A $@

clean:
	rm -f $(FIGURES)
