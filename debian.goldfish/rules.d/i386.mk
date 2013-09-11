human_arch	= 32 bit x86
build_arch	= i386
header_arch	= x86_64
defconfig	= defconfig
flavours        = goldfish
build_image	= bzImage
kernel_file	= arch/$(build_arch)/boot/bzImage
install_file	= vmlinuz
loader		= grub
no_dumpfile	= true

do_doc_package		= false
do_source_package	= false
do_common_headers_indep = false
do_libc_dev_package	= false
do_tools		= true
do_tools_perf		= true
disable_d_i             = true
