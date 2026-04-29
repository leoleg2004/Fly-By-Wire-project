# Makefile coordinatore per src/app

# Only include subdirectories that have a Makefile (actual apps)
SUBDIRS = $(shell for d in */; do if [ -f $$d/Makefile ]; then echo $$d; fi; done)

all:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir; \
	done

clean:
	@for dir in $(SUBDIRS); do \
		$(MAKE) -C $$dir clean; \
	done

.PHONY: all clean $(SUBDIRS)
