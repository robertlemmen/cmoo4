development.pdf: development.tex design_hl.pdf
	@echo "Typesetting $@..."
	@pdflatex -interaction=batchmode development.tex
	@pdflatex -interaction=batchmode development.tex

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
