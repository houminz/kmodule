# 如果已定义KERNELRELEASE，则说明是从内核构造系统调用的，
# 因此可利用其内建语句
ifneq ($(KERNELRELEASE), )
    obj-m := hello.o
    # module-objs := file1.o file2.o 如果模块依赖多个源文件，请添加这一句并相应地修改目标文件列表
# 否则，是直接从命令行调用的，
# 这时要调用内核构造系统
else
    KERNELDIR ?= /lib/modules/$(shell uname -r)/build
    PWD := $(shell pwd)
modules:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules
modules_install:
	$(MAKE) -C $(KERNELDIR) M=$(PWD) modules_install
clean:
	rm -f *.ko *.o *.mod *.mod.o *.mod.c .*.cmd *.symvers  modul*
endif
