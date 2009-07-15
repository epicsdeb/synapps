// omm.c
// Linux Device Driver for the Onyx-MM-XT module
// 
// This device driver was derived from the source code provided by the
// text "Linux Device Drivers, 3rd Edition, by Corbet & Rubini."
// 
// 2007-01-19   David M. Kline, ANL, APS, BCDA.
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

#include "omm.h"


// General defintions / defaults
#define OMM_DEVNAM        "omm"
#define OMM_BUFSIZ        (80)
#define OMM_DEVCNT        (4)
#define OMM_MAJOR         (0)
#define OMM_MINOR         (0)
#define OMM_BASE          (0)
#define OMM_IRQ           (0)


// Hardware definitions / defaults
#define OMM_REGCNT        (16)
#define OMM_CFG           (0xE)


// Define data structures
typedef struct Omm Omm;
struct Omm
{
    int irq;
	int base;
	int major;
	int minor;
    int intcnt;
    char send[OMM_BUFSIZ];
    char recv[OMM_BUFSIZ];
	dev_t devno;
	struct cdev cdev;
	struct semaphore sem;
    struct resource* pres;
    wait_queue_head_t omm_wait;
};


// Declare variants
static Omm* omm_devs[OMM_DEVCNT] = {NULL,NULL,NULL,NULL};


// Module parameters
static int ommirq   = OMM_IRQ;
static int ommmajor = OMM_MAJOR;
static int ommbasecnt = 0;
static int ommbase[OMM_DEVCNT] = {OMM_BASE,OMM_BASE,OMM_BASE,OMM_BASE};
module_param(ommirq,int,S_IRUGO);
module_param(ommmajor,int,S_IRUGO);
module_param_array(ommbase,int,&ommbasecnt,0666);


// ----------------------------------------------------------------------------
// File operations related methods
// ----------------------------------------------------------------------------

// Open file
static int omm_open(struct inode* inode,struct file* filp)
{
    unsigned int major,minor;

    major = imajor(inode);
    minor = iminor(inode);
    filp->private_data = omm_devs[minor];

    return( 0 );
}


// Close file
static int omm_release(struct inode* inode,struct file* filp)
{
    return( 0 );
}


// Read
static ssize_t omm_read(struct file* filp,char __user* buf,size_t count,loff_t* f_pos)
{
	int sts=0;
    Omm* pomm = filp->private_data;

    down_interruptible(&pomm->sem);
    up(&pomm->sem);

	return( sts );
}


// Write
static ssize_t omm_write(struct file* filp,const char __user* buf,size_t count,loff_t* f_pos)
{
	int sts=0;
    Omm* pomm = filp->private_data;

    down_interruptible(&pomm->sem);
    up(&pomm->sem);

	return( sts );
}


// Control
static int omm_ioctl(struct inode* inode,struct file* filp,unsigned int cmd,unsigned long arg)
{
    int sts = -1;
    Omm* pomm = filp->private_data;

    down_interruptible(&pomm->sem);
    switch( cmd )
    {
    case OMM_IOCRBASE:
        sts = __put_user(pomm->base,(int __user*)arg);
        break;
    case OMM_IOCRMAJOR:
        sts = __put_user(pomm->major,(int __user*)arg);
        break;
    case OMM_IOCRNAME:
        sts = copy_to_user((int __user*)arg,OMM_DEVNAM,sizeof(OMM_DEVNAM));
        break;
    case OMM_IOCRMINOR:
        sts = __put_user(pomm->minor,(int __user*)arg);
        break;
    case OMM_IOCRDEVCNT:
        sts = __put_user(ommbasecnt,(int __user*)arg);
        break;
    case OMM_IOCRMAXDEVCNT:
        sts = __put_user(OMM_DEVCNT,(int __user*)arg);
        break;
    case OMM_IOCWAITONINT:
        interruptible_sleep_on(&pomm->omm_wait);
        sts = __put_user(pomm->intcnt,(int __user*)arg);
        break;
    default:
        sts = -1;
        break;
    }
    up(&pomm->sem);

    return( sts );
}


static struct file_operations omm_fops =
{
    .owner   = THIS_MODULE,
    .read    = omm_read,
    .write   = omm_write,
    .ioctl   = omm_ioctl,
    .open    = omm_open,
    .release = omm_release,
};


// ----------------------------------------------------------------------------
// Module related methods
// ----------------------------------------------------------------------------


// Interrupt handler
static irqreturn_t handler(int a,void* b,struct pt_regs* c)
{
    Omm* pomm = (Omm*)b;

    ++pomm->intcnt;
    wake_up_interruptible(&pomm->omm_wait);

    return( IRQ_HANDLED );
}


// Setup cdev structure
static int omm_setup_cdev(Omm* pomm,int minor)
{
    dev_t devno = MKDEV(ommmajor,minor);

    cdev_init(&pomm->cdev,&omm_fops);
    pomm->cdev.owner = THIS_MODULE;
    pomm->cdev.ops   = &omm_fops;
    if( cdev_add(&pomm->cdev,devno,1) )
        return( -1 );
    else
        return( 0 );
}


// Close and cleanup module
static void __exit omm_cleanup_module(void)
{
    int i;
    dev_t dev = MKDEV(ommmajor,OMM_MINOR);

    for( i=0; i<ommbasecnt; ++i )
    {
        if( omm_devs[i] == NULL ) continue;

        if( omm_devs[i]->irq ) {disable_irq(omm_devs[i]->irq);outb(0,omm_devs[i]->base+OMM_CFG);}
        cdev_del(&omm_devs[i]->cdev);
        if( omm_devs[i]->pres ){release_region(omm_devs[i]->base,OMM_REGCNT);}
        if( omm_devs[i]->irq ) free_irq(omm_devs[i]->irq,omm_devs[i]); else kfree(omm_devs[i]);
        omm_devs[i] = NULL;
    }

    unregister_chrdev_region(dev,OMM_DEVCNT);
    printk("\n*** UNLOADING OMM DRIVER ***\n");
}


// Initialize module
static int __init omm_init_module(void)
{
	int i,sts;
    dev_t dev = 0;

	printk("\n*** LOADING OMM DRIVER ***\n");

	if( ommmajor )
	{
		dev = MKDEV(ommmajor,OMM_MINOR);
		sts = register_chrdev_region(dev,OMM_DEVCNT,OMM_DEVNAM);
	}
	else
	{
		sts = alloc_chrdev_region(&dev,OMM_MINOR,OMM_DEVCNT,OMM_DEVNAM);
		ommmajor = MAJOR(dev);
	}

	if( sts )
	{
		printk("Failure to assign major device number for %s\n",OMM_DEVNAM);
		return( sts );
	}
	else
		printk("Assigned major device number %d for %s\n",ommmajor,OMM_DEVNAM);


	for( i=0; i<ommbasecnt; ++i )
	{
    	omm_devs[i] = (Omm*)kmalloc(sizeof(Omm),GFP_KERNEL);
	    if( omm_devs[i] )
		    memset(omm_devs[i],0,sizeof(Omm));
    	else
        {
		    printk("Failure to allocate kernel memory for %s\n",OMM_DEVNAM);
    		omm_cleanup_module();
	    	return( -ENOMEM );
        }

		omm_devs[i]->base  = ommbase[i];
		omm_devs[i]->major = ommmajor;
		omm_devs[i]->minor = i;

        if( ommirq )
        {
            sts = request_irq(ommirq,handler,SA_INTERRUPT|SA_SHIRQ,"omm-mm-xt",omm_devs[i]);
            if( sts == 0 )
    	    	omm_devs[i]->irq = ommirq;
            else
           	{
            	printk("Failure to request IRQ-%d for %s\n",ommirq,OMM_DEVNAM);
	            omm_cleanup_module();
           		return( sts );
            }
        }

        omm_devs[i]->pres = request_region(omm_devs[i]->base,OMM_REGCNT,OMM_DEVNAM);
        if( omm_devs[i]->pres == NULL )
        {
    		printk("Failure to allocate IO ports 0x%2.2X..0x%2.2X for %s\n",omm_devs[i]->base,omm_devs[i]->base+OMM_REGCNT,OMM_DEVNAM);
	    	omm_cleanup_module();
		    return( -1 );
        }

        sts = omm_setup_cdev(omm_devs[i],i);
        if( sts )
        {
    		printk("Failure to setup cdev %s\n",OMM_DEVNAM);
	    	omm_cleanup_module();
		    return( -1 );
        }

		init_MUTEX(&omm_devs[i]->sem);
        init_waitqueue_head(&omm_devs[i]->omm_wait);
        if( omm_devs[i]->irq ) {enable_irq(omm_devs[i]->irq);outb(4,omm_devs[i]->base+OMM_CFG);}
		printk("OMM device omm%d has base addr of 0x%3.3X",i,omm_devs[i]->base);
		if( omm_devs[i]->irq ) printk(" irq %d",ommirq); 

	}
	printk("\n*** OMM DRIVER LOAD COMPLETE ***\n");

	return( 0 );
}


// Registration macors
module_init(omm_init_module);
module_exit(omm_cleanup_module);


// Module ID symbols
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Kline");
MODULE_DESCRIPTION("OMM Linux device driver");
