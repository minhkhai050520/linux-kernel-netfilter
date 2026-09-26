obj-m += myfw.o

myfw-objs := main.o myfw_rcu_api.o myfw_nf_hook.o \
			myfw_hashtable.o myfw_spinlock.o

KDIR ?= $(abspath $(CURDIR)/../../linux-custom)
ARCH ?= arm64
CROSS_COMPILE ?= aarch64-linux-gnu-

.PHONY: all clean

all:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(CURDIR) modules

clean:
	$(MAKE) ARCH=$(ARCH) CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(CURDIR) clean