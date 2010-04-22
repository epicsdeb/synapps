# ensure that files are not open in FrameMaker before running this

# source files
SOURCE = Manual.book TitlePage.fm Introduction.fm Concepts.fm Compiling.fm \
	 Using.fm Syntax.fm Procvar.fm Examples.fm Installation.fm Glossary.fm

# default target (MIF files)
mif: $(SOURCE:%=%.mif)

# pattern rule to run fmbatch to generate MIF from Frame source
%.mif: %
	@$(RM) $*.fmbatch.tmp
	echo Open $< >>$*.fmbatch.tmp
	echo SaveAs m $< $@ >>$*.fmbatch.tmp
	echo Quit >>$*.fmbatch.tmp
	fmbatch $*.fmbatch.tmp
	@$(RM) $*.fmbatch.tmp

# the MIF is precious; don't delete it
.precious: %.mif

