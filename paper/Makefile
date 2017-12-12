NAME = hdht

all:
	pdflatex $(NAME)
	bibtex $(NAME)
	pdflatex $(NAME)
	pdflatex $(NAME)

clean:
	rm -rf $(NAME).blg $(NAME).log $(NAME).dvi $(NAME).bbl $(NAME).aux $(NAME).log $(NAME).pdf
