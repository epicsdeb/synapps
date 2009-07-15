// autoshut.c
// Linux Device Driver for auto-shutdown
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


// General defintions / defaults
#define AS_DEVNAM        "autoshut"
#define AS_BUFSIZ        (80)
#define AS_DEVCNT        (1)
#define AS_MAJOR         (0)
#define AS_MINOR         (0)
#define AS_BASE          (0)
#define AS_IRQ           (0)


// Hardware definitions / defaults
#define AS_REGCNT        (1)


// Define data structures
typedef struct As As;
struct As
{
    int irq;
	int base;
	int major;
	int minor;
    int intcnt;
    char send[AS_BUFSIZ];
    char recv[AS_BUFSIZ];
	dev_t devno;
	struct cdev cdev;
	struct semaphore sem;
    struct resource* pres;
    wait_queue_head_t as_wait;
};


// Declare variants
static As* as_devs[AS_DEVCNT] = {NULL};
static char key[] = "autoshut-shutdown";


// Module parameters
static int asirq   = AS_IRQ;
static int asmajor = AS_MAJOR;
static int asbasecnt = 0;
static int asbase[AS_DEVCNT] = {AS_BASE};
module_param(asirq,int,S_IRUGO);
module_param(asmajor,int,S_IRUGO);
module_param_array(asbase,int,&asbasecnt,0666);


// ----------------------------------------------------------------------------
// File operations related methods
// ----------------------------------------------------------------------------

// Open file
static int as_open(struct inode* inode,struct file* filp)
{
    unsigned int major,minor;

    major = imajor(inode);
    minor = iminor(inode);
    filp->private_data = as_devs[minor];

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
    case 1:
        sts = __put_user(pas->base,(int __user*)arg);
        break;
    case 2:
        sts = __put_user(pas->major,(int __user*)arg);
        break;
    case 3:
        sts = copy_to_user((int __user*)arg,AS_DEVNAM,sizeof(AS_DEVNAM));
        break;
    case 4:
        sts = __put_user(pas->minor,(int __user*)arg);
        break;
    case 5:
        sts = __put_user(asbasecnt,(int __user*)arg);
        break;
    case 6:
        sts = __put_user(AS_DEVCNT,(int __user*)arg);
        break;
    case 7:
        interruptible_sleep_on(&pas->as_wait);
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
    As* pas = (As*)b;

    ++pas->intcnt;
    wake_up_interruptible(&pas->as_wait);
printk(">>>AUTOSHUT INTERRUPT %d received\n",pas->intcnt);

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
    int i;
    dev_t dev = MKDEV(asmajor,AS_MINOR);

    for( i=0; i<asbasecnt; ++i )
    {
        if( as_devs[i] == NULL ) continue;

        if( as_devs[i]->irq ) {disable_irq(as_devs[i]->irq);outb_p(0,as_devs[i]->base+2);}
        cdev_del(&as_devs[i]->cdev);
        if( as_devs[i]->pres ){release_region(as_devs[i]->base,AS_REGCNT);}
        if( as_devs[i]->irq ) free_irq(as_devs[i]->irq,as_devs[i]); else kfree(as_devs[i]);
        as_devs[i] = NULL;
    }

    unregister_chrdev_region(dev,AS_DEVCNT);
    printk("\n*** UNLOADING AUTOSHUT DRIVER ***\n");
}


// Initialize module
static int __init as_init_module(void)
{
	int i,sts;
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
	else
		printk("Assigned major device number %d for %s\n",asmajor,AS_DEVNAM);


	for( i=0; i<asbasecnt; ++i )
	{
    	as_devs[i] = (As*)kmalloc(sizeof(As),GFP_KERNEL);
	    if( as_devs[i] )
		    memset(as_devs[i],0,sizeof(As));
    	else
        {
		    printk("Failure to allocate kernel memory for %s\n",AS_DEVNAM);
    		as_cleanup_module();
	    	return( -ENOMEM );
        }

		as_devs[i]->base  = asbase[i];
		as_devs[i]->major = asmajor;
		as_devs[i]->minor = i;

        if( asirq )
        {
            sts = request_irq(asirq,handler,SA_INTERRUPT|SA_SHIRQ,"autoshut",as_devs[i]);
            if( sts == 0 )
    	    	as_devs[i]->irq = asirq;
            else
           	{
            	printk("Failure to request IRQ-%d for %s\n",asirq,AS_DEVNAM);
	            as_cleanup_module();
           		return( sts );
            }
        }

        as_devs[i]->pres = request_region(as_devs[i]->base,AS_REGCNT,AS_DEVNAM);
        if( as_devs[i]->pres == NULL )
        {
    		printk("Failure to allocate IO ports 0x%2.2X..0x%2.2X for %s\n",as_devs[i]->base,as_devs[i]->base+AS_REGCNT,AS_DEVNAM);
	    	as_cleanup_module();
		    return( -1 );
        }

        sts = as_setup_cdev(as_devs[i],i);
        if( sts )
        {
    		printk("Failure to setup cdev %s\n",AS_DEVNAM);
	    	as_cleanup_module();
		    return( -1 );
        }

		init_MUTEX(&as_devs[i]->sem);
        init_waitqueue_head(&as_devs[i]->as_wait);
        if( as_devs[i]->irq ) {enable_irq(as_devs[i]->irq);outb_p(0x10,as_devs[i]->base+2);}
		printk("AUTOSHUT device autoshut%d has base addr of 0x%3.3X",i,as_devs[i]->base);
		if( as_devs[i]->irq ) printk(" irq %d",asirq); 

	}
	printk("\n*** AUTOSHUT DRIVER LOAD COMPLETE ***\n");

	return( 0 );
}


// Registration macors
module_init(as_init_module);
module_exit(as_cleanup_module);


// Module ID symbols
MODULE_LICENSE("GPL");
MODULE_AUTHOR("David Kline");
MODULE_DESCRIPTION("AUTOSHUT Linux device driver");
