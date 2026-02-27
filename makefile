VPP ?= v++

PLATFORM ?= /xilinx/platforms/xilinx_u280_gen3x16_xdma_1_202211_1/xilinx_u280_gen3x16_xdma_1_202211_1.xpfm
INCLUDES := -I /home/sail/lyf/pma_demo/stream/include
TARGET ?= hw_emu
MEMKERNEL_DIR := ./memkernel
PMA_KERNEL_DIR := ./pma_kernel
MEM_XO_FILES := $(addprefix $(MEMKERNEL_DIR)/, alloc_free_kernel.xo   app_mem_middle_ware.xo)
PMA_XO_FILES := $(addprefix $(PMA_KERNEL_DIR)/, update_message_generator.xo update_message_router.xo pma_insert_vertex.xo pma_insert_edge.xo pma_read_kernel.xo check_kernel.xo signal_kernel.xo)

XCLBIN_FILE := $(TARGET)_test_all.xclbin
all: subdirs $(XCLBIN_FILE)

subdirs:
	$(MAKE) -C memkernel all TARGET=$(TARGET)
	$(MAKE) -C pma_kernel all TARGET=$(TARGET)

ALL_XO_FILES = $(MEM_XO_FILES) $(PMA_XO_FILES)
$(XCLBIN_FILE): $(ALL_XO_FILES)
	@echo "---- Linking kernels to create $(XCLBIN_FILE) ----"
	$(VPP) -l -g -t $(TARGET) $(INCLUDES) --platform $(PLATFORM) --config system.cfg --config profile.cfg -o $@ $^
	@echo "---- Link done: $@ ----"
host: host.cpp
	g++ -std=c++17 $(INCLUDES) -I${XILINX_XRT}/include -L${XILINX_XRT}/lib -I${XILINX_VITIS_HLS}/include -o $@ $^ -lxrt_core -lxrt_coreutil -lOpenCL -pthread -lrt -ldl -g
real_host: real_host.cpp
	g++ -std=c++17 $(INCLUDES) -I${XILINX_XRT}/include -L${XILINX_XRT}/lib -I${XILINX_VITIS_HLS}/include -o $@ $^ -lxrt_core -lxrt_coreutil -lOpenCL -pthread -lrt -ldl -g
test_host: test_host.cpp
	g++ -std=c++17 $(INCLUDES) -I${XILINX_XRT}/include -L${XILINX_XRT}/lib -I${XILINX_VITIS_HLS}/include -o $@ $^ -lxrt_core -lxrt_coreutil -lOpenCL -pthread -lrt -ldl -g

clean:
	$(MAKE) -C memkernel clean
	$(MAKE) -C pma_kernel clean
	rm -f $(XCLBIN_FILE) host