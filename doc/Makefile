GIT_VERSION := $(shell git log -1 --pretty=format:'g%h -- %ci')
GIT_DIRTY := $(shell git describe --all --long --dirty | grep -q dirty && echo ' (locally modified)' || true)

development.pdf: development.tex design_hl.pdf
	@echo "Typesetting $@..."
	@pdflatex -interaction=batchmode "\def\gitversion{$(GIT_VERSION)$(GIT_DIRTY)}\input{development}"
	@pdflatex -interaction=batchmode "\def\gitversion{$(GIT_VERSION)$(GIT_DIRTY)}\input{development}"

%.pdf : %.svg
	echo "Converting $@..."
	@inkscape --export-latex \
		--export-area-drawing \
		--export-pdf=$@ $<

clean:
	@echo "Cleaning..."
	@rm -f development.toc development.aux development.log \
		development.ind development.idx development.out \
		development.pdf
	@rm -f design_hl.pdf design_hl.pdf_tex
