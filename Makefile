obj-m := lcdcontrol.o

ccflags-y := -I$(src)/include

lcdcontrol-y := src/lcdcontrol.o src/hd44780.o

KERNEL_DIR ?= /lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

all:
	@make -C $(KERNEL_DIR) M=$(PWD) modules

clean:
	@make -C $(KERNEL_DIR) M=$(PWD) clean
	@rm -f compile_commands.json
