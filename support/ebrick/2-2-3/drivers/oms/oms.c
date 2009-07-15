// oms.c
// Linux Device Driver for the OMS PC68/78 PC104-based motion controllers
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
#include <linux/delay.h>
#include <linux/init.h>
#include <linux/cdev.h>
#include <linux/fs.h>

#include <asm/uaccess.h>
#include <asm/io.h>

#include "oms.h"


// General defintions / defaults
#define OMS_DEVNAM	"oms"
#define OMS_BUFSIZ  (80)
#define OMS_DEVCNT	(4)
#define OMS_MAJOR	(0)
#define OMS_MINOR	(0)
#define OMS_BASE    (0)
#define OMS_IRQ     (0)


// OMS hardware definitions / defaults
#define OMS_TIMOUT  (50000)
#define OMS_REGCNT  (4)

#define OMS_DATREG  (0)
#define OMS_DONREG  (1)
#define OMS_CTLREG  (2)
#define OMS_STSREG  (3)

#define OMS_STSIRQ  (0x80)
#define OMS_STSTBE  (0x40)
#define OMS_STSIBF  (0x20)
#define OMS_STSDON  (0x10)
#define OMS_STSOVR  (0x08)
#define OMS_STSENC  (0x04)
#define OMS_STSINT  (0x02)
#define OMS_STSCMD  (0x01)
#define OMS_STSERR  (0x0F)

#define OMS_CTLIRQ  (0x80)
#define OMS_CTLTBE  (0x40)
#define OMS_CTLIBF  (0x20)
#define OMS_CTLDON  (0x10)
#define OMS_CTLMSK  (0xF0)
#define OMS_CTLINT  (OMS_CTLIRQ|OMS_CTLDON)


// Define data structures
typedef struct Oms Oms;
struct Oms
{
    int irq;
	int base;
	int major;
	int minor;
    int intcnt;
    int intdat;
    unsigned char cntl;
    unsigned char stat;
    unsigned char done;
    char send[OMS_BUFSIZ];
    char recv[OMS_BUFSIZ];
	dev_t devno;
	struct cdev cdev;
	struct semaphore sem;
    struct resource* pres;
    wait_queue_head_t oms_wait;
};


// Declare global variants
static Oms* oms_devs[OMS_DEVCNT] = {NULL,NULL};


// Module parameters
static int omsirq = OMS_IRQ;
static int omsmajor = OMS_MAJOR;
static int omsbasecnt = 0;
static int omsbase[OMS_DEVCNT] = {OMS_BASE,OMS_BASE,OMS_BASE,OMS_BASE};
module_param(omsirq,int,S_IRUGO);
module_param(omsmajor,int,S_IRUGO);
module_param_array(omsbase,int,&omsbasecnt,0666);


// ----------------------------------------------------------------------------
// File operations related methods
// ----------------------------------------------------------------------------

// Open file
static int oms_open(struct inode* inode,struct file* filp)
{
    unsigned int major,minor;

    major = imajor(inode);
    minor = iminor(inode);
    filp->private_data = oms_devs[minor];

    return( 0 );
}


// Close file
static int oms_release(struct inode* inode,struct file* filp)
{
    return( 0 );
}


// Read
static ssize_t oms_read(struct file* filp,char __user* buf,size_t count,loff_t* f_pos)
{
	int c,i=0,j,sts;
    Oms* poms = filp->private_data;

    down_interruptible(&poms->sem);
    for( c=0; c<count; ++c )
    {
        for( j=0; j<OMS_TIMOUT && (inb(poms->base+OMS_STSREG)&OMS_STSIBF)==0;++j ) schedule();
        if( j==OMS_TIMOUT ) break;
        poms->recv[i++] = inb(poms->base+OMS_DATREG);
    }

    if( c==0 ) poms->recv[i++] = '#';
    poms->recv[i] = '\0';
	sts = copy_to_user(buf,poms->recv,i);
    up(&poms->sem);

	if( sts ) return( -EFAULT ); else return( i );
}


// Write
static ssize_t oms_write(struct file* filp,const char __user* buf,size_t count,loff_t* f_pos)
{
	int i=0,j=0,sts=0;
    Oms* poms = filp->private_data;

    down_interruptible(&poms->sem);
	sts = copy_from_user(poms->send,buf,count);
	if( sts ) {up(&poms->sem);return( -EFAULT );}

    for( i=0; i<count; ++i )
    {
    	for( j=0; j<OMS_TIMOUT && (inb(poms->base+OMS_STSREG)&OMS_STSTBE)==0; ++j ) schedule();
        if( j==OMS_TIMOUT ) break;
        outb(poms->send[i],poms->base+OMS_DATREG); 
    }
    up(&poms->sem);

	return( count );
}


// Control
static int oms_ioctl(struct inode* inode,struct file* filp,unsigned int cmd,unsigned long arg)
{
    int sts = -1;
    Oms* poms = filp->private_data;

    switch( cmd )
    {
    case OMS_IOCRBASE:
        sts = __put_user(poms->base,(int __user*)arg);
        break;
    case OMS_IOCRMAJOR:
        sts = __put_user(poms->major,(int __user*)arg);
        break;
    case OMS_IOCRNAME:
        sts = copy_to_user((int __user*)arg,OMS_DEVNAM,sizeof(OMS_DEVNAM));
        break;
    case OMS_IOCRMINOR:
        sts = __put_user(poms->minor,(int __user*)arg);
        break;
    case OMS_IOCRDEVCNT:
        sts = __put_user(omsbasecnt,(int __user*)arg);
        break;
    case OMS_IOCRMAXDEVCNT:
        sts = __put_user(OMS_DEVCNT,(int __user*)arg);
        break;
    case OMS_IOCWAITONINT:
        interruptible_sleep_on(&poms->oms_wait);
        sts = __put_user(poms->intdat,(int __user*)arg);
        break;
    default:
        sts = -1;
        break;
    }

    return( sts );
}


static struct file_operations oms_fops =
{
    .owner   = THIS_MODULE,
    .read    = oms_read,
    .write   = oms_write,
    .ioctl   = oms_ioctl,
    .open    = oms_open,
    .release = oms_release,
};


// ----------------------------------------------------------------------------
// Module related methods
// ----------------------------------------------------------------------------


// Interrupt handler
static irqreturn_t handler(int a,void* b,struct pt_regs* c)
{
    Oms* poms = (Oms*)b;

    poms->stat = inb(poms->base+OMS_STSREG);
    if( (poms->stat & OMS_STSDON) == 0 ) return( IRQ_NONE );
    poms->done = inb(poms->base+OMS_DONREG);

    poms->intdat = (++poms->intcnt << 16) | (poms->stat << 8) | poms->done;
    wake_up_interruptible(&poms->oms_wait);

    return( IRQ_HANDLED );
}

// Setup cdev structure
static int oms_setup_cdev(Oms* poms,int minor)
{
    dev_t devno = MKDEV(omsmajor,minor);

    cdev_init(&poms->cdev,&oms_fops);
    poms->cdev.owner = THIS_MODULE;
    poms->cdev.ops   = &oms_fops;
    if( cdev_add(&poms->cdev,devno,1) )
        return( -1 );
    else
        return( 0 );
}


// Close and cleanup module
static void __exit oms_cleanup_module(void)
{
    int i;
    dev_t dev = MKDEV(omsmajor,OMS_MINOR);

    for( i=0; i<omsbasecnt; ++i )
    {
        if( oms_devs[i] == NULL ) continue;

        if( oms_devs[i]->irq ) {disable_irq(oms_devs[i]->irq);outb(0,oms_devs[i]->base+OMS_CTLREG);}
        cdev_del(&oms_devs[i]->cdev);
        if( oms_devs[i]->pres ){release_region(oms_devs[i]->base,OMS_REGCNT);}
        if( oms_devs[i]->irq ) free_irq(oms_devs[i]->irq,oms_devs[i]); else kfree(oms_devs[i]);
        oms_devs[i] = NULL;
    }

    unregister_chrdev_region(dev,OMS_DEVCNT);
    printk("\n*** UNLOADING OMS PC104 DRIVER ***\n");
}


// Initialize module
static int __init oms_init_module(void)
{
	int i,sts;
    dev_t dev = 0;

	printk("\n*** LOADING OMS PC104 DRIVER ***\n");
	if( omsmajor )
	{
		dev = MKDEV(omsmajor,OMS_MINOR);
		sts = register_chrdev_region(dev,OMS_DEVCNT,OMS_DEVNAM);
	}
	else
	{
		sts = alloc_chrdev_region(&dev,OMS_MINOR,OMS_DEVCNT,OMS_DEVNAM);
		omsmajor = MAJOR(dev);
	}

	if( sts )
	{
		printk("Failure to assign major device number for %s\n",OMS_DEVNAM);
		return( sts );
	}
	else
		printk("Assigned major device number %d for %s\n",omsmajor,OMS_DEVNAM);

	for( i=0; i<omsbasecnt; ++i )
	{
    	oms_devs[i] = (Oms*)kmalloc(sizeof(Oms),GFP_KERNEL);
	    if( oms_devs[i] )
		    memset(oms_devs[i],0,sizeof(Oms));
    	else
        {
		    printk("Failure to allocate kernel memory for %s\n",OMS_DEVNAM);
    		oms_cleanup_module();
	    	return( -ENOMEM );
        }

		oms_devs[i]->base  = omsbase[i];
		oms_devs[i]->major = omsmajor;
		oms_devs[i]->minor = i;

        if( omsirq )
        {
            sts = request_irq(omsirq,handler,SA_INTERRUPT|SA_SHIRQ,"oms-pc104",oms_devs[i]);
            if( sts == 0 )
    	    	oms_devs[i]->irq = omsirq;
            else
           	{
            	printk("Failure to request IRQ-%d for %s\n",omsirq,OMS_DEVNAM);
	            oms_cleanup_module();
           		return( sts );
            }
        }

        oms_devs[i]->pres = request_region(oms_devs[i]->base,OMS_REGCNT,OMS_DEVNAM);
        if( oms_devs[i]->pres == NULL )
        {
    		printk("Failure to allocate IO ports 0x%2.2X..0x%2.2X for %s\n",oms_devs[i]->base,oms_devs[i]->base+OMS_REGCNT,OMS_DEVNAM);
	    	oms_cleanup_module();
		    return( -1 );
        }

        sts = oms_setup_cdev(oms_devs[i],i);
        if( sts )
        {
    		printk("Failure to setup cdev %s\n",OMS_DEVNAM);
	    	oms_cleanup_module();
		    return( -1 );
        }

		init_MUTEX(&oms_devs[i]->sem);
        init_waitqueue_head(&oms_devs[i]->oms_wait);
        if( oms_devs[i]->irq ) {enable_irq(oms_devs[i]->irq);outb(OMS_CTLINT,oms_devs[i]->base+OMS_CTLREG);}
    	oms_devs[i]->cntl = inb(oms_devs[i]->base+OMS_CTLREG) & OMS_CTLMSK;
    	oms_devs[i]->stat = inb(oms_devs[i]->base+OMS_STSREG);
		printk("OMS device oms%d has base addr of 0x%2.2X",i,oms_devs[i]->base);
		if( oms_devs[i]->irq ) printk(" irq %d",omsirq); 
		printk(" cntl 0x%2.2X stat 0x%2.2X\n",oms_devs[i]->cntl,oms_devs[i]->stat);
	}

	return( 0 );
}


// Registration macors
module_init(oms_init_module);
module_exit(oms_cleanup_module);


// Module ID symbols
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Kline");
MODULE_DESCRIPTION("OMS PC104 motor controller Linux device driver");
