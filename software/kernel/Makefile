KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

ccflags-y += -Wall -I$(src)/include

# =========================
# Module object definitions
# =========================

udp_core-objs := \
	driver/udp_core_main.o \
	driver/udp_core_irq.o \
	driver/udp_core_netdev.o \
	driver/udp_core_regs.o \
	driver/udp_core_devlink.o \
	driver/udp_core_pkt.o

dev-irq-objs := driver/dev-irq.o

# Default: only udp_core
obj-m := udp_core.o

# If BUILD_DEV_IRQ=1, build dev-irq instead
ifeq ($(BUILD_DEV_IRQ),1)
obj-m := dev-irq.o
endif

# If BUILD_BOTH=1, build both
ifeq ($(BUILD_BOTH),1)
obj-m := udp_core.o dev-irq.o
endif

# =========================
# Targets
# =========================

.PHONY: all udp-core dev-irq both clean

all: udp-core

udp-core:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules

dev-irq:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules BUILD_DEV_IRQ=1

both:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) modules BUILD_BOTH=1

clean:
	$(MAKE) -C $(KERNEL_DIR) M=$(PWD) clean
