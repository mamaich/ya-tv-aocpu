#define SKIP_IO_TRACE
#include <linux/module.h>
#include <linux/uaccess.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/pci.h>
#include <linux/proc_fs.h>
#include <linux/uaccess.h>


#define MHU_MAX_SIZE            96
#define BUFF_SIZE            	16
#define CMD_RPCUINTREE_TEST     0x6


/* boot log:
[AOCPU]: mailbox init start
reg idx=0 cmd=6 handler=f701d548
*/

#define CMD_RPCUINTREE_TEST_ADDR     0xf701d548


enum c_chan_t {
	C_DSPA_FIFO = 0, /* to dspa */
	C_DSPB_FIFO = 1, /* to dspb */
	C_AOCPU_FIFO = 2, /* to aocpu */
	C_SECPU_FIFO = 3, /* to secpu core */
	C_DSPA_PL = 4, /* to dspa core */
	C_DSPB_PL = 5, /* to dspb core */
	C_AOCPU_PL = 6, /* to aocpu core */
	C_SECPU_PL = 7, /* to secpu core */
	C_AOCPU_OLD = 8, /* to aocpu core */
	C_MAXNUM,
};

int scpi_send_data(void *data, int size, int channel,
		   int cmd, void *revdata, int revsize);

/*
void func(unsigned long *msg) { //https://godbolt.org/ risc-v 32 bits
    unsigned long addr=*msg;
    for(int i=0; i<64; i++)
    {
        msg[i]=*(unsigned long*)(addr+i*4);
    }
}
func(unsigned long*):   // https://riscvasm.lucasteske.dev/
        addi    sp,sp,-48
        sw      ra,44(sp)
        sw      s0,40(sp)
        addi    s0,sp,48
        sw      a0,-36(s0)
        lw      a5,-36(s0)
        lw      a5,0(a5)
        sw      a5,-24(s0)
        sw      zero,-20(s0)
        j       .L2
.L3:
        lw      a5,-20(s0)
        slli    a5,a5,2
        mv      a4,a5
        lw      a5,-24(s0)
        add     a5,a4,a5
        mv      a3,a5
        lw      a5,-20(s0)
        slli    a5,a5,2
        lw      a4,-36(s0)
        add     a5,a4,a5
        lw      a4,0(a3)
        sw      a4,0(a5)
        lw      a5,-20(s0)
        addi    a5,a5,1
        sw      a5,-20(s0)
.L2:
        lw      a4,-20(s0)
        li      a5,63
        ble     a4,a5,.L3
        nop
        nop
        lw      ra,44(sp)
        lw      s0,40(sp)
        addi    sp,sp,48
        jr      ra
*/
static uint32_t patch[]={
	0xfd010113,
	0x02112623,
	0x02812423,
	0x03010413,
	0xfca42e23,
	0xfdc42783,
	0x0007a783,
	0xfef42623,
	0xfec42783,
	0x0007a703,
	0xfdc42783,
	0x00e7a023,
	0xfec42783,
	0x00478793,
	0x00078713,
	0xfdc42783,
	0x00478793,
	0x00072703,
	0x00e7a023,
	0xfec42783,
	0x00878793,
	0x00078713,
	0xfdc42783,
	0x00878793,
	0x00072703,
	0x00e7a023,
	0xfec42783,
	0x00c78793,
	0x00078713,
	0xfdc42783,
	0x00c78793,
	0x00072703,
	0x00e7a023,
	0x00000013,
	0x02c12083,
	0x02812403,
	0x03010113,
	0x00008067,
};

static void init_patch(void)
{
	uint32_t __iomem *virt = ioremap(CMD_RPCUINTREE_TEST_ADDR,sizeof(patch)); // xMboxUintReeTestCase addr
	int i;
	if(!virt)
	{
		printk(KERN_ALERT "ioremap() failed\n");
		return;
	}
	printk(KERN_ALERT "before copy\n");
	for(i=0; i<sizeof(patch)/4; i++)
	{
		iowrite32(patch[i], virt+i);
	}	
	printk(KERN_ALERT "after copy\n");
	iounmap(virt);
}

static void readbuf(uint32_t addr, uint8_t buff[BUFF_SIZE])
{
    uint32_t read_addr=addr;
    scpi_send_data(&read_addr,sizeof(read_addr),C_AOCPU_FIFO,CMD_RPCUINTREE_TEST,buff,BUFF_SIZE);
}
       
static int phys_access_proc_open(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t phys_access_proc_write(struct file *filp, const char __user *buffer,
			size_t count, loff_t *ppos)
{
	return 0;
}

static ssize_t phys_access_proc_read(struct file *filp, char __user *buffer,
			size_t count, loff_t *ppos)
{
	long phys_offset = *ppos;
	uint8_t virt[BUFF_SIZE];

	if(count>BUFF_SIZE)
		count=BUFF_SIZE;

	readbuf(phys_offset, virt);

	if(copy_to_user(buffer, virt, count)) {}
	*ppos += count;
	return count;
}

static int phys_access_mmap(struct file *file, struct vm_area_struct *vma)
{
	return -EAGAIN;
}

static loff_t phys_access_lseek(struct file *file, loff_t offset, int orig)
{
	loff_t ret;
	inode_lock(file_inode(file));
	switch (orig) {
	case SEEK_CUR:
		offset += file->f_pos;
		file->f_pos = offset;
		ret = offset;
		break;
	case SEEK_SET:
		file->f_pos = offset;
		ret = file->f_pos;
		break;
	default:
		ret = -EINVAL;
	}
	inode_unlock(file_inode(file));
	return ret;
}

static const struct file_operations phys_access_fops = {
	.open = phys_access_proc_open,
	.read = phys_access_proc_read,
	.write = phys_access_proc_write,
	.mmap = phys_access_mmap,
	.llseek = phys_access_lseek,
};

static int __init phys_access_init(void)
{
	proc_create("riscv_mem", 0755, NULL, &phys_access_fops);
	pr_info("Patching AOCPU mem\n");
	init_patch();
	return 0;
}

static void __exit phys_access_cleanup(void)
{
	remove_proc_entry("riscv_mem", NULL);
}

module_init(phys_access_init);
module_exit(phys_access_cleanup);
MODULE_LICENSE("GPL");
