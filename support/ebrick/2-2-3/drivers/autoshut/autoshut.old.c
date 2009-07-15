// autoshut.c
// Linux Device Driver for the EPICS Brick auto shutdown
// 
// This device driver was derived from the source code provided by the
// text "Linux Device Drivers, 3rd Edition, by Corbet & Rubini."
// 
// 2006-10-3    David M. Kline, ANL, APS, BCDA.
//                  Initial development complete.
//                  Interrupts are *NOT* supported at this time.
// 

// Include files:
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/ioport.h>
#include <linux/types.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include "autoshut.h"


// General defintions / defaults
#define AS_DEVNAM   "autoshut"
#define AS_BUFSIZ   (80)
#define AS_DEVCNT   (1)
#define AS_MAJOR    (0)
#define AS_MINOR    (0)
#define AS_BASE     (0)
#define AS_IRQ      (0)


// Hardware definitions / defaults
#define AS_TIMEOUT (5000)
#define AS_REGCNT  (3)

#define AS_DATREG  (0)
#define AS_STSREG  (1)
#define AS_CTLREG  (2)

#define AS_STSBUS  (0x80)
#define AS_STSACK  (0x40)
#define AS_STSOUT  (0x20)
#define AS_STSSIN  (0x10)
#define AS_STSERR  (0x08)
#define AS_STSIRQ  (0x04)

#define AS_CTLBI   (0x20)
#define AS_CTLACK  (0x10)
#define AS_CTLSEL  (0x08)
#define AS_CTLINT  (0x04)
#define AS_CTLALE  (0x02)
#define AS_CTLSTR  (0x01)

#define AS_STSMSK  (0xFC)
#define AS_CTLMSK  (0x3F)


// Define data structures
typedef struct As As;
struct As
{
    int irq;
	int base;
	int major;
	int minor;
    char send[AS_BUFSIZ];
    char recv[AS_BUFSIZ];
	dev_t devno;
	struct cdev cdev;
	struct semaphore sem;
    struct resource* pres;
};


// Declare variants
static int intcnt;
static As* as_dev = NULL;
static char key[] = "autoshut-shutdown";
DECLARE_WAIT_QUEUE_HEAD(as_wait);


// Module parameters
static int asirq   = AS_IRQ;
static int asmajor = AS_MAJOR;
static int asbase  = AS_BASE;
module_param(asirq,int,S_IRUGO);
module_param(asbase,int,S_IRUGO);
module_param(asmajor,int,S_IRUGO);


// ----------------------------------------------------------------------------
// File operations related methods
// ----------------------------------------------------------------------------

// Open file
static int as_open(struct inode* inode,struct file* filp)
{
    unsigned int major,minor;

    major = imajor(inode);
    minor = iminor(inode);
    filp->private_data = as_dev;

    return( 0 );
}


// Close file
static int as_release(struct inode* inode,struct file* filp)
{
    return( 0 );
}


// Read
static ssize_t as_read(struct file* filp,char __user* buf,size_t count,loff_t* f_pos)
{
	int sts=0;
    As* pas = filp->private_data;

    down_interruptible(&pas->sem);
    up(&pas->sem);

	return( sts );
}


// Write
static ssize_t as_write(struct file* filp,const char __user* buf,size_t count,loff_t* f_pos)
{
	int sts=0;
    As* pas = filp->private_data;

    down_interruptible(&pas->sem);
    up(&pas->sem);

	return( sts );
}


// Control
static int as_ioctl(struct inode* inode,struct file* filp,unsigned int cmd,unsigned long arg)
{
    int sts = -1;
    As* pas = filp->private_data;

    down_interruptible(&pas->sem);
    switch( cmd )
    {
    case AS_IOCRBASE:
        sts = __put_user(pas->base,(int __user*)arg);
        break;
    case AS_IOCRMAJOR:
        sts = __put_user(pas->major,(int __user*)arg);
        break;
    case AS_IOCRNAME:
        sts = copy_to_user((int __user*)arg,AS_DEVNAM,sizeof(AS_DEVNAM));
        break;
    case AS_IOCRMINOR:
        sts = __put_user(pas->minor,(int __user*)arg);
        break;
    case AS_IOCRDEVCNT:
        sts = __put_user(AS_DEVCNT,(int __user*)arg);
        break;
    case AS_IOCRMAXDEVCNT:
        sts = __put_user(AS_DEVCNT,(int __user*)arg);
        break;
    case AS_IOCWAITONINT:
        interruptible_sleep_on(&as_wait);
        sts = copy_to_user((int __user*)arg,key,sizeof(key));
        break;
    default:
        sts = -1;
        break;
    }
    up(&pas->sem);

    return( sts );
}


static struct file_operations as_fops =
{
    .owner   = THIS_MODULE,
    .read    = as_read,
    .write   = as_write,
    .ioctl   = as_ioctl,
    .open    = as_open,
    .release = as_release,
};


// ----------------------------------------------------------------------------
// Module related methods
// ----------------------------------------------------------------------------


// Interrupt handler
static irqreturn_t handler(int a,void* b,struct pt_regs* c)
{
    intcnt++;
    printk(">>> AUTOSHUT interrupt count=%d\n",intcnt);
	
    wake_up_interruptible(&as_wait);

    return( IRQ_HANDLED );
}


// Setup cdev structure
static int as_setup_cdev(As* pas,int minor)
{
    dev_t devno = MKDEV(asmajor,minor);

    cdev_init(&pas->cdev,&as_fops);
    pas->cdev.owner = THIS_MODULE;
    pas->cdev.ops   = &as_fops;
    if( cdev_add(&pas->cdev,devno,1) )
        return( -1 );
    else
        return( 0 );
}


// Close and cleanup module
static void __exit as_cleanup_module(void)
{
    dev_t dev = MKDEV(asmajor,AS_MINOR);

    cdev_del(&as_dev->cdev);
    if( as_dev->pres ){release_region(as_dev->base,AS_REGCNT);}

    disable_irq(as_dev->irq);
    free_irq(as_dev->irq,NULL);

	kfree(as_dev);
	as_dev = NULL;
	unregister_chrdev_region(dev,AS_DEVCNT);
	printk("\n*** UNLOADING OMS DRIVER ***\n");
}


// Initialize module
static int __init as_init_module(void)
{
	int sts;
    dev_t dev = 0;

	printk("\n*** LOADING AUTOSHUT DRIVER ***\n");
	if( asmajor )
	{
		dev = MKDEV(asmajor,AS_MINOR);
		sts = register_chrdev_region(dev,AS_DEVCNT,AS_DEVNAM);
	}
	else
	{
		sts = alloc_chrdev_region(&dev,AS_MINOR,AS_DEVCNT,AS_DEVNAM);
		asmajor = MAJOR(dev);
	}

	if( sts )
	{
		printk("Failure to assign major device number for %s\n",AS_DEVNAM);
		return( sts );
	}

	as_dev = (As*)kmalloc(sizeof(As),GFP_KERNEL);
	if( as_dev )
		memset(as_dev,0,sizeof(As));
	else
    {
		printk("Failure to allocate kernel memory for %s\n",AS_DEVNAM);
		as_cleanup_module();
		return( -ENOMEM );
    }

    sts = request_irq(asirq,&handler,SA_INTERRUPT|SA_SHIRQ,"autoshut",as_dev);
	if( sts )
	{
		printk("Failure to request IRQ-%d for %s\n",asirq,AS_DEVNAM);
		as_cleanup_module();
		return( sts );
	}

    as_dev->base  = asbase;
	as_dev->major = asmajor;
	as_dev->minor = AS_MINOR;
	as_dev->irq   = asirq;

    as_dev->pres = request_region(as_dev->base,AS_REGCNT,AS_DEVNAM);
    if( as_dev->pres == NULL )
    {
   		printk("Failure to allocate IO ports 0x%2.2X..0x%2.2X for %s\n",as_dev->base,as_dev->base+AS_REGCNT,AS_DEVNAM);
    	as_cleanup_module();
	    return( -1 );
    }

    sts = as_setup_cdev(as_dev,0);
    if( sts )
    {
   		printk("Failure to setup cdev %s\n",AS_DEVNAM);
    	as_cleanup_module();
	    return( -1 );
    }

	init_MUTEX(&as_dev->sem);
    enable_irq(as_dev->irq);
   	outb_p(AS_CTLACK,as_dev->base+AS_CTLREG);
	printk("AUTOSHUT has base addr 0x%2.2X,major %d,minor %d,irq %d\n",as_dev->base,as_dev->major,as_dev->minor,as_dev->irq);

	return( 0 );
}


// Registration macors
module_init(as_init_module);
module_exit(as_cleanup_module);


// Module ID symbols
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Kline");
MODULE_DESCRIPTION("EPICS Brick auto shutdown Linux device driver");
