#!/bin/make

hidtools := hidrawmon

obj-m := hid-betop-t6.o

KERN_DIR ?= /usr/lib/modules/$(shell uname -r)/build
PWD := $(shell pwd)

build:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) modules
	
$(hidtools): %: %.c
	$(CC) -o $@ $<

clean:
	$(MAKE) -C $(KERN_DIR) M=$(PWD) clean
	rm $(hidtools) || true