.PHONY: clean open spell

PROJ=main
TEXFILES=$(wildcard *.tex)
FIGURE_DATA=$(wildcard *.csv)
FIGURES=$(FIGURE_DATA:.csv=.png)

PYTHON=python

main.pdf: $(TEXFILES) $(FIGURES)
	pdflatex main.tex
	bibtex main
	pdflatex main.tex

%.png: %.csv
	$(PYTHON) plot_results.py $<

open: $(PROJ).pdf
	kpdf $^

clean:
	rm -f $(PROJ).pdf *.aux *.toc *.bbl *.blg *.log *.out *.bak $(FIGURES)

spell:
	for i in $(TEXFILES) ; do \
	aspell -t -c -p $(shell pwd)/dict.txt $$i ; \
	done;
