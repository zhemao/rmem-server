PROJ=main
.PHONY: clean open spell

FILES=$(wildcard *.tex)

all: clean
	pdflatex main.tex
	bibtex main
	pdflatex main.tex
	pdflatex main.tex

open: $(PROJ).pdf
	kpdf $^

$(PROJ).pdf: $(PROJ).tex $(PROJ).bbl
	pdflatex $(PROJ)
	pdflatex $(PROJ)

$(PROJ).bbl: biblio.bib $(PROJ).tex
	pdflatex $(PROJ)
	bibtex $(PROJ)

clean:
	rm -f $(PROJ).pdf *.aux *.toc *.bbl *.blg *.log *.out *.bak


spell:
	for i in $(FILES) ; do \
	aspell -t -c -p $(shell pwd)/dict.txt $$i ; \
	done;
