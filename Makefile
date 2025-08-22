#KDIR := /home/mamaich/linux-amlogic-archive-amlogic-4.9.113-10.01.22/
KDIR := /home/mamaich/linux-khadas-vims-5.4.y/
obj-m :=risc_access.o
all:
	make -Wno-unused-result -C $(KDIR) M=$(PWD) modules
	arm-linux-gnueabihf-gcc -O2 -static -o ./read_riscv_mem ./read_riscv_mem.c
clean:
	make -C $(KDIR) M=$(PWD) clean
	rm ./read_riscv_mem
