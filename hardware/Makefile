# Check if Vivado command is available
VIVADO := $(shell command -v vivado 2>/dev/null)

ifeq ($(VIVADO),)
$(error Vivado not found in PATH. Please source Vivado settings.sh)
endif

# Targets
TARGETS:=

# Subdirectories
SUBDIRS = fpga
SUBDIRS_CLEAN = $(patsubst %,%.clean,$(SUBDIRS))

# Rules
.PHONY: all 1g 10g
all: $(SUBDIRS) $(TARGETS)

.PHONY: $(SUBDIRS)
$(SUBDIRS):
	cd $@ && $(MAKE)

# Build 1G RGMII FPGA bitstream
1g:
	cd fpga && $(MAKE) VARIANT=1g

# Build 10G XGMII FPGA bitstream
10g:
	cd fpga && $(MAKE) VARIANT=10g

.PHONY: $(SUBDIRS_CLEAN)
$(SUBDIRS_CLEAN):
	cd $(@:.clean=) && $(MAKE) clean

.PHONY: clean
clean: $(SUBDIRS_CLEAN)
	-rm -rf $(TARGETS)

.PHONY: program
program:
	cd $(SUBDIRS) && $(MAKE) program
