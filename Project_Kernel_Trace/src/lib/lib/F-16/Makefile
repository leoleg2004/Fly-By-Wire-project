DIRS = fcs_long fcs_lat fcs_alt nav_gps nav_ins radar_air radar_gnd engine_fadec weapons displays

all: subdirs

subdirs:
	@for dir in $(DIRS); do \
		if [ -d "$$dir" ]; then \
			$(MAKE) -C $$dir; \
		fi \
	done

clean:
	@for dir in $(DIRS); do \
		if [ -d "$$dir" ]; then \
			$(MAKE) -C $$dir clean; \
		fi \
	done

.PHONY: all clean subdirs
