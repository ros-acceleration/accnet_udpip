###################################################################
# 
# Xilinx Vivado FPGA Makefile
# 
# Copyright (c) 2016 Alex Forencich
# 
###################################################################
# 
# Parameters:
# FPGA_TOP - Top module name
# FPGA_FAMILY - FPGA family (e.g. VirtexUltrascale)
# FPGA_DEVICE - FPGA device (e.g. xcvu095-ffva2104-2-e)
# SYN_FILES - space-separated list of source files
# INC_FILES - space-separated list of include files
# XDC_FILES - space-separated list of timing constraint files
# XCI_FILES - space-separated list of IP XCI files
# 
# Example:
# 
# FPGA_TOP = fpga
# FPGA_FAMILY = VirtexUltrascale
# FPGA_DEVICE = xcvu095-ffva2104-2-e
# SYN_FILES = rtl/fpga.v
# XDC_FILES = fpga.xdc
# XCI_FILES = ip/pcspma.xci
# include ../common/vivado.mk
# 
###################################################################

# phony targets
.PHONY: clean fpga

# prevent make from deleting intermediate files and reports
.PRECIOUS: %.xpr %.bit %.mcs %.prm
.SECONDARY:

CONFIG ?= config.mk
-include ../$(CONFIG)

FPGA_TOP ?= udp_ip_core_wrapper
PROJECT ?= udp_ip_core

OVERLAY_DIR = ./output_overlay/$(PROJECT)

SYN_FILES_REL = $(patsubst %, ../%, $(SYN_FILES))
INC_FILES_REL = $(patsubst %, ../%, $(INC_FILES))
XCI_FILES_REL = $(patsubst %, ../%, $(XCI_FILES))
IP_TCL_FILES_REL = $(patsubst %, ../%, $(IP_TCL_FILES))

ifdef XDC_FILES
  XDC_FILES_REL = $(patsubst %, ../%, $(XDC_FILES))
else
  XDC_FILES_REL = $(PROJECT).xdc
endif

PFM_TCL_REL = $(patsubst %, ../%, $(PFM_TCL))

# XCLBIN
PLATFORM_PATH = fpga/xsct/$(PROJECT)/$(PROJECT)/export/$(PROJECT)/$(PROJECT).xpfm
PLATFORM_PATH_REL = $(patsubst %, ../%, $(PLATFORM_PATH))

###################################################################
# Main Targets
#
# all: build everything
# clean: remove output files and project files
###################################################################

all: xclbin

vivado: $(PROJECT).xpr
	vivado $(PROJECT).xpr

xsa: $(PROJECT).xsa

# xpfm requires sourcing vitis settings sh
xpfm: $(PROJECT).xpfm

# xclbin requires sourcing vitis settings sh
xclbin: $(PROJECT).xclbin

bit: $(PROJECT).bit

tmpclean:
	-rm -rf *.log *.jou *.cache *.gen *.hbs *.hw *.ip_user_files *.runs *.xpr *.html *.xml *.sim *.srcs *.str .Xil
	-rm -rf create_project.tcl generate_xsa.tcl run_synth.tcl run_impl.tcl generate_bit.tcl
	-rm -rf temp boot image xsct linux.bif xclbin .ipcache
	-rm -f $(PROJECT).xclbin $(OVERLAY_DIR)/$(PROJECT).bit.bin

clean: tmpclean
	-rm -rf *.bit *.ltx program.tcl generate_mcs.tcl *.mcs *.prm flash.tcl *.xsa *.xpfm *.xclbin *.info *.link_summary
	-rm -rf *_utilization.rpt *_utilization_hierarchical.rpt

distclean: clean
	-rm -rf rev

###################################################################
# Target implementations
###################################################################

# Vivado project file
create_project.tcl: Makefile $(XCI_FILES_REL) $(IP_TCL_FILES_REL)
	echo "create_project -force -part $(FPGA_PART) $(PROJECT)" > $@
	echo "add_files -fileset sources_1 $(SYN_FILES_REL)" >> $@
	# Source files are imported (copied into the project) so that v++ process does not complain about missing sources
	echo "import_files -norecurse $(SYN_FILES_REL)" >> $@
	echo "add_files -fileset constrs_1 $(XDC_FILES_REL)" >> $@
	for x in $(XCI_FILES_REL); do echo "import_ip $$x" >> $@; done
	for x in $(IP_TCL_FILES_REL); do echo "source $$x" >> $@; done
	echo "make_wrapper -files [get_files $(PROJECT).srcs/sources_1/bd/$(PROJECT)/$(PROJECT).bd] -top" >> $@
	echo "add_files -norecurse $(PROJECT).gen/sources_1/bd/$(PROJECT)/hdl/$(PROJECT)_wrapper.v" >> $@
	echo "update_compile_order -fileset sources_1" >> $@
	echo "set_property top $(FPGA_TOP) [current_fileset]" >> $@
	echo "update_compile_order -fileset sources_1" >> $@

$(PROJECT).xpr: create_project.tcl
	vivado -nojournal -nolog -mode batch $(foreach x,$?,-source $x)

# generate xsa
$(PROJECT).xsa: $(PROJECT).xpr $(SYN_FILES_REL) $(INC_FILES_REL) $(XDC_FILES_REL)

	# Open project
	echo "open_project $(PROJECT).xpr" > generate_xsa.tcl

	# Generate output products 
	echo "delete_ip_run [get_files -of_objects [get_fileset sources_1] $(PROJECT).srcs/sources_1/bd/$(PROJECT)/$(PROJECT).bd]" >> generate_xsa.tcl
	echo "set_property synth_checkpoint_mode None [get_files  $(PROJECT).srcs/sources_1/bd/$(PROJECT)/$(PROJECT).bd]" >> generate_xsa.tcl
	echo "generate_target all [get_files  $(PROJECT).srcs/sources_1/bd/$(PROJECT)/$(PROJECT).bd]" >> generate_xsa.tcl
	
	# Export xsa
	echo "export_ip_user_files -of_objects [get_files $(PROJECT).srcs/sources_1/bd/$(PROJECT)/$(PROJECT).bd] -no_script -sync -force -quiet" >> generate_xsa.tcl
	echo "export_simulation -of_objects [get_files $(PROJECT).srcs/sources_1/bd/$(PROJECT)/$(PROJECT).bd] -directory $(PROJECT).ip_user_files/sim_scripts -ip_user_files_dir $(PROJECT).ip_user_files -ipstatic_source_dir $(PROJECT).ip_user_files/ipstatic -lib_map_path [list {modelsim=$(PROJECT).cache/compile_simlib/modelsim} {questa=$(PROJECT).cache/compile_simlib/questa} {xcelium=$(PROJECT).cache/compile_simlib/xcelium} {vcs=$(PROJECT).cache/compile_simlib/vcs} {riviera=$(PROJECT).cache/compile_simlib/riviera}] -use_ip_compiled_libs -force -quiet" >> generate_xsa.tcl
	echo "set_property platform.board_id {board} [current_project]" >> generate_xsa.tcl
	echo "set_property platform.name {name} [current_project]" >> generate_xsa.tcl
	echo "set_property pfm_name {xilinx:board:name:0.0} [get_files -all {$(PROJECT).srcs/sources_1/bd/$(PROJECT)/$(PROJECT).bd}]" >> generate_xsa.tcl
	echo "set_property platform.extensible {true} [current_project]" >> generate_xsa.tcl
	echo "set_property platform.design_intent.embedded {true} [current_project]" >> generate_xsa.tcl
	echo "set_property platform.design_intent.datacenter {false} [current_project]" >> generate_xsa.tcl
	echo "set_property platform.design_intent.server_managed {false} [current_project]" >> generate_xsa.tcl
	echo "set_property platform.design_intent.external_host {false} [current_project]" >> generate_xsa.tcl
	echo "set_property platform.default_output_type {sd_card} [current_project]" >> generate_xsa.tcl
	echo "set_property platform.uses_pr {false} [current_project]" >> generate_xsa.tcl
	echo "write_hw_platform -hw -force -file $(PROJECT).xsa" >> generate_xsa.tcl

	# Run all previous commands
	vivado -nojournal -nolog -mode batch -source generate_xsa.tcl

# generate xpfm
$(PROJECT).xpfm: $(PROJECT).xsa
	xsct -sdx $(PFM_TCL_REL) -xsa $(PROJECT).xsa
	ln -f -s xsct/$(PROJECT)/$(PROJECT)/export/$(PROJECT)/$(PROJECT).xpfm .

# generate xclbin
$(PROJECT).xclbin: $(PROJECT).xpfm
	v++ -l --save-temps -t hw --platform $(PLATFORM_PATH_REL) --temp_dir ./temp/ -o $(PROJECT).xclbin
	cp $(PROJECT).xclbin $(OVERLAY_DIR)/$(PROJECT).bit.bin
	