.PHONY: all install clean uninstall

all install clean uninstall:
	@for x in $(wildcard Makefile_?); do \
	    make -f $$x $(MAKEFILAGS) $@; \
	done
