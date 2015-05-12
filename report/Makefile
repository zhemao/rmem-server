.PHONY: clean clean-latex open spell

PROJ=main
TEXFILES=$(wildcard *.tex)
GRAPH_DATA=$(wildcard graphs/*.csv)
GRAPHS=$(GRAPH_DATA:.csv=.pdf)
DRAWING_SOURCES=$(wildcard graphs/*.svg)
DRAWINGS=$(DRAWING_SOURCES:.svg=.pdf)
FIGURES=$(GRAPHS) $(DRAWINGS)
BIBFILE=main.bib

INKSCAPE=inkscape
PYTHON=python
LATEX=pdflatex
BIBTEX=bibtex

main.pdf: $(TEXFILES) $(BIBFILE) $(FIGURES)
	$(LATEX) main.tex
	$(BIBTEX) main
	$(LATEX) main.tex
	$(LATEX) main.tex

figures: $(FIGURES)

%.pdf: %.csv
	$(PYTHON) plot_results.py $<

%.pdf: %.svg
	$(INKSCAPE) -D $< -A $@

open: $(PROJ).pdf
	kpdf $^

clean: clean-latex
	rm -f $(FIGURES)

clean-latex:
	rm -f $(PROJ).pdf *aux *.toc *.bbl *.blg *.log *.out *.bak *.cb *.cb2

spell:
	for i in $(TEXFILES) ; do \
	aspell -t -c -p $(shell pwd)/dict.txt $$i ; \
	done;
