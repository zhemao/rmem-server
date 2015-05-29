TEXFILES=$(wildcard *.tex)

poster.pdf: $(TEXFILES) figures
	pdflatex poster.tex

figures:
	make -C ../figures
