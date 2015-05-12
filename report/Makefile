.PHONY: clean open spell

PROJ=main
TEXFILES=$(wildcard *.tex)
GRAPH_DATA=$(wildcard graphs/*.csv)
GRAPHS=$(GRAPH_DATA:.csv=.pdf)
DRAWING_SOURCES=$(wildcard graphs/*.svg)
DRAWINGS=$(DRAWING_SOURCES:.svg=.pdf)
FIGURES=$(GRAPHS) $(DRAWINGS)

main.pdf: $(TEXFILES) $(FIGURES)
	pdflatex main.tex
	bibtex main
	pdflatex main.tex

figures: $(FIGURES)

%.pdf: %.csv
	python plot_results.py $<

%.pdf: %.svg
	/Applications/Inkscape.app/Contents/Resources/bin/inkscape -D $< -A $@

open: $(PROJ).pdf
	kpdf $^

clean:
	rm -f $(PROJ).pdf *.aux *.toc *.bbl *.blg *.log *.out *.bak $(FIGURES)

spell:
	for i in $(TEXFILES) ; do \
	aspell -t -c -p $(shell pwd)/dict.txt $$i ; \
	done;
