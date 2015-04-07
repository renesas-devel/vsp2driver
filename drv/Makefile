CFILES := vsp2_drv.c vsp2_entity.c vsp2_video.c
CFILES += vsp2_rpf.c vsp2_rwpf.c vsp2_wpf.c
CFILES += vsp2_bru.c vsp2_uds.c
CFILES += vsp2_vspm.c

obj-m += vsp2.o
vsp2-objs := $(CFILES:.c=.o)

U_INCLUDE := -I$(PWD)
U_INCLUDE += -I$(KERNELSRC)/include
EXTRA_CFLAGS += $(U_INCLUDE)
ifndef VSP2_VSPMSYMVERS
VSP2_VSPMSYMVERS = Module.symvers
endif

all:
	make -C $(KERNELDIR) M=$(PWD) KBUILD_EXTRA_SYMBOLS=$(VSP2_VSPMDIR)/$(VSP2_VSPMSYMVERS) modules
clean:
	make -C $(KERNELDIR) M=$(PWD) clean
