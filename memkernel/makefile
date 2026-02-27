# Minimal Makefile for building memkernel kernels with v++
# Usage:
#   make            -> build all kernels (default TARGET=hw_emu)
#   make TARGET=hw  -> build for FPGA hardware
#   make clean
#
# Edit PLATFORM if your platform xpfm path is different.

VPP ?= v++
PLATFORM ?= /xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/xilinx_u280_gen3x16_xdma_1_202211_1.xpfm
INCLUDES := -I/home/sail/lyf/pma_demo/stream/include

# Target can be hw_emu or hw
TARGET ?= hw_emu

# List of kernels (file names without .cpp and matching extern "C" function names)
KERNELS :=  alloc_free_kernel  app_mem_middle_ware

XO_FILES := $(patsubst %,%.xo,$(KERNELS))


.PHONY: all clean help
all: $(XO_FILES)

# build single kernel .xo from corresponding .cpp
# uses -k <kernel_name> (kernel name must match extern "C" function)
# %.xo: %.cpp
#     @echo "---- Building kernel $* -> $@ (target=$(TARGET)) ----"
#     $(VPP) -c -t $(TARGET) $(INCLUDES) --platform $(PLATFORM) -k $* -o $@ $<
#     @echo "---- done: $@ ----"

%.xo: %.cpp
	@echo "---- Building kernel $* -> $@ (target=$(TARGET)) ----"
	$(VPP) -c -g -t $(TARGET) $(INCLUDES) --platform $(PLATFORM) -k $* -o $@ $<
	@echo "---- done: $@ ----"
# allocate_kernel.xo: allocate_kernel.cpp memory_manage_alloc.cpp
# 	@echo "---- Building allocate_kernel -> $@ ----"
# 	$(VPP) -c -g -t $(TARGET) $(INCLUDES) --platform $(PLATFORM) -k allocate_kernel -o $@ allocate_kernel.cpp memory_manage_alloc.cpp

# free_kernel.xo: free_kernel.cpp memory_manage_free.cpp
# 	@echo "---- Building free_kernel -> $@ ----"
# 	$(VPP) -c -g -t $(TARGET) $(INCLUDES) --platform $(PLATFORM) -k free_kernel -o $@ free_kernel.cpp memory_manage_free.cpp

host: host.cpp
	@echo "---- Building host executable ----"
	g++ -g -o host host.cpp -I$(XILINX_XRT)/include -L$(XILINX_XRT)/lib -I$(XILINX_VITIS_HLS)/include -lxrt_coreutil -lxrt_core -lpthread -lstdc++ -lm


help:
	@echo "Makefile targets:"
	@echo "  make           build all kernels (default TARGET=hw_emu)"
	@echo "  make TARGET=hw build for hw"
	@echo "  make clean     remove generated .xo and _x/ directories"
	@echo
	@echo "Adjust PLATFORM and INCLUDES variables at top if needed."

clean:
	@echo "Cleaning .xo and build artifacts..."
	@rm -f *.xo
	@rm -rf _x
	@rm v++_*
	@rm *_summary
	@echo "done."