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
#define BUFF_SIZE            	0x1000
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
    unsigned char *srcaddr=(unsigned char*)msg[0];
    unsigned char *dstaddr=(unsigned char*)msg[1];
    unsigned long len=msg[2];
    do
    {
        *dstaddr++=*srcaddr++;
    } while(--len);
}
func(unsigned long*):   // https://riscvasm.lucasteske.dev/
        addi    sp,sp,-48
        sw      ra,44(sp)
        sw      s0,40(sp)
        addi    s0,sp,48
        sw      a0,-36(s0)
        lw      a5,-36(s0)
        lw      a5,0(a5)
        sw      a5,-20(s0)
        lw      a5,-36(s0)
        addi    a5,a5,4
        lw      a5,0(a5)
        sw      a5,-24(s0)
        lw      a5,-36(s0)
        lw      a5,8(a5)
        sw      a5,-28(s0)
.L2:
        lw      a5,-20(s0)
        addi    a4,a5,1
        sw      a4,-20(s0)
        lbu     a4,0(a5)
        lw      a5,-24(s0)
        addi    a3,a5,1
        sw      a3,-24(s0)
        sb      a4,0(a5)
        lw      a5,-28(s0)
        addi    a5,a5,-1
        sw      a5,-28(s0)
        lw      a5,-28(s0)
        snez    a5,a5
        andi    a5,a5,0xff
        bne     a5,zero,.L2
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
	0xfdc42783,
	0x00478793,
	0x0007a783,
	0xfef42423,
	0xfdc42783,
	0x0087a783,
	0xfef42223,
	0xfec42783,
	0x00178713,
	0xfee42623,
	0x0007c703,
	0xfe842783,
	0x00178693,
	0xfed42423,
	0x00e78023,
	0xfe442783,
	0xfff78793,
	0xfef42223,
	0xfe442783,
	0x00f037b3,
	0x0ff7f793,
	0xfc0794e3,
	0x00000013,
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
    uint32_t msg[3];
    msg[0]=addr;
    msg[1]=0;//virt_to_phys(buff);
    msg[2]=BUFF_SIZE;
    memset(buff,0xff,BUFF_SIZE);
    scpi_send_data(msg,sizeof(msg),C_AOCPU_FIFO,CMD_RPCUINTREE_TEST,msg,sizeof(msg));
    memcpy(buff,__va(0), BUFF_SIZE);
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
	static uint8_t virt[BUFF_SIZE];

	if(count>BUFF_SIZE)
		count=BUFF_SIZE;

	readbuf(phys_offset, virt);

	if(copy_to_user(buffer, virt, count)) {}
	*ppos += count;
	return count;
}

static int phys_access_mmap(struct file *file, struct vm_area_struct *vma)
{
	size_t size = vma->vm_end - vma->vm_start;

	if (remap_pfn_range(vma,
			vma->vm_start,
			vma->vm_pgoff,
			size,
			vma->vm_page_prot)) {
		return -EAGAIN;
	}
	return 0;	
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
