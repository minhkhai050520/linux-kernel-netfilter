obj-m += myfw_hash.o

KDIR ?= /workspace/linux-custom
CROSS_COMPILE ?= aarch64-linux-gnu-

all:
	make ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) modules

clean:
	make ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) -C $(KDIR) M=$(PWD) clean