development.pdf: development.tex 
	@echo "Typesetting $@..."
	@pdflatex -interaction=batchmode development.tex
	@pdflatex -interaction=batchmode development.tex

clean:
	@echo "Cleaning..."
	@rm -f development.toc development.aux development.log \
		development.ind development.idx development.out \
		development.pdf
