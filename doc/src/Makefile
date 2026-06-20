# You can set these variables from the command line.
SPHINXOPTS    = -j16
SPHINXBUILD   = sphinx-build
SPHINXPROJ    = INETFramework
SOURCEDIR     = .
BUILDDIR      = _build
SERVERPORT     = 8001

.PHONY: all pdf pdf-lualatex html clean deploy help server copy-media

all: html pdf

clean:
	@rm -rf $(BUILDDIR)

help:
	@$(SPHINXBUILD) -M help "$(SOURCEDIR)" "$(BUILDDIR)" $(SPHINXOPTS) $(O)

include tools/sphinx_deployment.mk

server: Makefile copy-media
	@ln -sfn ../../showcases showcases
	@ln -sfn ../../tutorials tutorials
	@trap 'rm -f showcases tutorials' EXIT; \
	sphinx-autobuild \
		--host 0.0.0.0 \
		--port $(SERVERPORT) \
		"$(SOURCEDIR)" \
		"$(BUILDDIR)/html" \
		$(SPHINXOPTS) \
		$(O) \
		--ignore .git

pdf: Makefile
	@ln -sfn ../../showcases showcases
	@ln -sfn ../../tutorials tutorials
	@trap 'rm -f showcases tutorials' EXIT; \
	$(SPHINXBUILD) \
		-M latexpdf \
		"$(SOURCEDIR)" \
		"$(BUILDDIR)" \
		$(SPHINXOPTS) \
		$(O)

pdf-lualatex: Makefile
	@ln -sfn ../../showcases showcases
	@ln -sfn ../../tutorials tutorials
	@trap 'rm -f showcases tutorials' EXIT; \
	$(SPHINXBUILD) \
		-M latex \
		"$(SOURCEDIR)" \
		"$(BUILDDIR)" \
		-D latex_engine=lualatex \
		$(SPHINXOPTS) \
		$(O) && \
	$(MAKE) -C "$(BUILDDIR)/latex" clean && \
	$(MAKE) \
		-C "$(BUILDDIR)/latex" \
		PDFLATEX="latexmk -lualatex -interaction=nonstopmode" \
		LATEXOPTS="" \
		all-pdf

html: Makefile copy-media
	@ln -sfn ../../showcases showcases
	@ln -sfn ../../tutorials tutorials
	@trap 'rm -f showcases tutorials' EXIT; \
	$(SPHINXBUILD) \
		-M html \
		"$(SOURCEDIR)" \
		"$(BUILDDIR)" \
		$(SPHINXOPTS) \
		$(O)

copy-media: ../../media
	@mkdir -p "$(BUILDDIR)/html"
	@# Copy video and audio files to the output directory.
	@# This should eventually be handled by SPHINXBUILD itself.
	@rsync -a \
		--copy-links \
		--prune-empty-dirs \
		--exclude '_docs' \
		--include '*/' \
		--include '*.wav' \
		--include '*.mp3' \
		--include '*.mp4' \
		--include '*.m4v' \
		--exclude '*' \
		../../tutorials "$(BUILDDIR)/html"
	@rsync -a \
		--copy-links \
		--prune-empty-dirs \
		--exclude '_docs' \
		--include '*/' \
		--include '*.wav' \
		--include '*.mp3' \
		--include '*.mp4' \
		--include '*.m4v' \
		--exclude '*' \
		../../showcases "$(BUILDDIR)/html"

../../media:
	@echo "*** 'media' folder is not present. Check out using:"
	@echo "*** git clone git@github.com:inet-framework/media.git"
	@false