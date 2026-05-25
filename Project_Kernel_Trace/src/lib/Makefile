# Global Makefile for src/lib in Project_Kernel_Trace

.PHONY: all clean core dds_lib generated_msg image_lib

all: core dds_lib generated_msg image_lib

core:
	$(MAKE) -C lib -f lib_makefile

dds_lib:
	$(MAKE) -C dds_lib -f dds_lib_makefile

generated_msg:
	$(MAKE) -C generated_msg

image_lib: generated_msg
	$(MAKE) -C image_lib

clean:
	$(MAKE) -C lib -f lib_makefile clean
	$(MAKE) -C dds_lib -f dds_lib_makefile clean
	$(MAKE) -C generated_msg clean
	$(MAKE) -C image_lib clean

.PHONY: all clean
