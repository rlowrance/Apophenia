.SUFFIXES: .dvi .pdf
DOCNAME = gsl_stats
#DOCNAME = c_crash

all: sources/dummy.tex
	latex $(DOCNAME)

latex: sources/dummy.tex
	latex $(DOCNAME)

codefigs:
	cd sources;
	"for i in *c; do c++2latex -a $i; done;"
	cd ..;

index:
	makeindex .texputs/$(DOCNAME).idx;

view:
	xdvi -expert -geometry 1041x797+-17+-17 -s 6 -paper usr $(DOCNAME)

publish: $(DOCNAME).pdf
	scp $(DOCNAME).pdf fluff@fluff.info:www/klemens/

viewpdf: 
	gv -seascape  -geometry 1129x824+-95+-49 -scale 2 $(DOCNAME).pdf

sources/dummy.tex:
	cd sources && ./remake && cd ..;

$(DOCNAME).pdf:	*tex
	pdflatex $(DOCNAME)

pdf:	
	pdflatex $(DOCNAME)
#	dvips -f < $(DOCNAME).dvi > $(DOCNAME).ps
#	ps2pdf $(DOCNAME).ps  $(DOCNAME).pdf

pdfslides:	
	#latex $(DOCNAME)
	#dvips -t landscape -t letter -f < $(DOCNAME).dvi > $(DOCNAME).ps\
	dvips -t landscape -f < $(DOCNAME).dvi > $(DOCNAME).ps
	ps2pdf $(DOCNAME).ps  $(DOCNAME).pdf

bib:	
	latex $(DOCNAME); bibtex $(DOCNAME); latex $(DOCNAME); latex $(DOCNAME)

viewps: 
	gv -seascape  -geometry 1129x824+-95+-49 -scale 2 $(DOCNAME).ps
