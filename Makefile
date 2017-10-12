PROJECT_DIR := $(shell pwd)
INCLUDE_DIR :=

export FLAGS :=

OS ?=
ifeq ($(OS),)
export ARCH = arm
export CROSS_COMPILE = arm-linux-
export CC = $(CROSS_COMPILE)gcc -I$(INCLUDE_DIR) -I$(PROJECT_DIR) $(FLAGS)
export LD = $(CROSS_COMPILE)ld
export STRIP = $(CROSS_COMPILE)strip
else
export CC = gcc -I$(INCLUDE_DIR) -I$(PROJECT_DIR) $(FLAGS)
export LD = gcc -lm
export STRIP = echo
endif

ifneq ($(KERNELRELEASE),)
	obj-m := sht11_dev.o
else
	KERNELDIR ?= /opt/EmbedSky/TQ2416/linux-3.1_TQ2416
	PWD := $(shell pwd)
default:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
.PHONY: clean	
clean:
	rm -rf *.o *.ko *.symvers *.order *.mod.c
endif
