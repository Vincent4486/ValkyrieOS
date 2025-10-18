# Makefile for building the documentation PDF

TEX = The_Code_of_Valkyrie_OS.tex
OUTDIR = generated
PDF = $(OUTDIR)/The_Code_of_Valkyrie_OS.pdf
PDFLATEX ?= pdflatex
LATEXMK ?= latexmk
BIBTEX ?= bibtex
BASE = $(basename $(TEX))

.PHONY: all pdf latexmk clean view

all: pdf


pdf:
	@echo "Building $(PDF) with $(PDFLATEX) (outputs -> $(OUTDIR)/)"
	@mkdir -p $(OUTDIR)
	# first pdflatex pass (write aux files into $(OUTDIR))
	$(PDFLATEX) -interaction=nonstopmode -output-directory=$(OUTDIR) $(TEX) || true
	# run bibtex inside the output directory if a .aux was produced
	@if [ -f $(OUTDIR)/$(BASE).aux ]; then \
		( cd $(OUTDIR) && $(BIBTEX) $(BASE) ) || true; \
	fi
	# subsequent pdflatex passes to resolve references
	$(PDFLATEX) -interaction=nonstopmode -output-directory=$(OUTDIR) $(TEX) || true
	$(PDFLATEX) -interaction=nonstopmode -output-directory=$(OUTDIR) $(TEX) || true

latexmk:
	@echo "Building with latexmk (outputs -> $(OUTDIR)/)"
	@mkdir -p $(OUTDIR)
	# latexmk supports -outdir to set output directory for pdflatex
	$(LATEXMK) -pdf -outdir=$(OUTDIR) -interaction=nonstopmode $(TEX)

clean:
	@echo "Cleaning temporary LaTeX files (docs)"
	# Try to let latexmk clean any known files in the output directory
	-$(LATEXMK) -C -outdir=$(OUTDIR) || true
	# Remove any stray aux/bbl/log/out files in the docs/ dir (from older runs)
	-rm -f $(BASE).aux $(BASE).bbl $(BASE).blg $(BASE).log $(BASE).out
	# Remove the generated output directory entirely
	-rm -rf $(OUTDIR)
	# keep the source .tex file; delete generated artifacts above


view: pdf
	@echo "Opening $(PDF)"
	open "$(PDF)"
