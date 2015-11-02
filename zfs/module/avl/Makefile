src = /home/rtg/ukb/xenial/amd64/master-next/ubuntu-xenial/debian/build/build-generic/zfs/module/avl
obj = /home/rtg/ukb/xenial/amd64/master-next/ubuntu-xenial/debian/build/build-generic/zfs/module/avl

MODULE := zavl

EXTRA_CFLAGS = $(ZFS_MODULE_CFLAGS)  -Wno-unused-but-set-variable -Wno-bool-compare -DHAVE_SPL -D_KERNEL -DTEXT_DOMAIN=\"zfs-linux-kernel\" -DNDEBUG 

obj-$(CONFIG_ZFS) := $(MODULE).o

$(MODULE)-objs += avl.o
