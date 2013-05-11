
KVERSION = $(shell uname -r)
KDIR := /lib/modules/$(KVERSION)/build
PWD := $(shell pwd)

obj-m = hid-lg-g710-plus.o

default: build

build:
	make -C $(KDIR) M=$(PWD) modules
clean:
	make -C $(KDIR) M=$(PWD) clean
install:
	make -C $(KDIR) M=$(PWD) modules_install