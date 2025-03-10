CONFIG_MODULE_SIG=n

ccflags-y += -I$(src)/../../ -I$(src)/../../../../src/buddy/include/ -I$(src)/../../../../src/common/

obj-m += kern.o

KDIR := /lib/modules/$(shell uname -r)/build

PWD := $(shell pwd)

default:
	make -C $(KDIR) M=$(PWD) modules
	
clean:
	make -C $(KDIR) M=$(PWD) clean
