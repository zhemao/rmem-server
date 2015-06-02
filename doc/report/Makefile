.PHONY: clean clean-latex open spell

PROJ=main
TEXFILES=$(wildcard *.tex)
BIBFILE=main.bib

LATEX=pdflatex
BIBTEX=bibtex

main.pdf: $(TEXFILES) $(BIBFILE) figures
	$(LATEX) main.tex
	$(BIBTEX) main
	$(LATEX) main.tex
	$(LATEX) main.tex

figures:
	make -C ../figures

open: $(PROJ).pdf
	kpdf $^

clean: clean-latex
	make -C ../figures clean

clean-latex:
	rm -f $(PROJ).pdf *aux *.toc *.bbl *.blg *.log *.out *.bak *.cb *.cb2

spell:
	for i in $(TEXFILES) ; do \
	aspell -t -c -p ../dict.txt $$i ; \
	done;
