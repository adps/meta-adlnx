#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#include <linux/poll.h>

#define DEVICE_NAME "adz2"


static struct class adz2_class = 
{
    .name = DEVICE_NAME,
    .owner = THIS_MODULE,
};

#define FPGA_IRQ_DISABLED 	0
#define FPGA_IRQ_ENABLED	1
#define FPGA_IRQ_INDEX          61


static volatile int     g_nIRQState = FPGA_IRQ_DISABLED;
static spinlock_t       g_slIrqLock;
static spinlock_t       g_slOpen;
static dev_t            g_sDevNo;
static struct cdev*     g_pAdz2Cdev;
static struct device*   g_pAdz2Device;
static int              g_nDevNum = 0;
static int              g_nDevIndex = 0;
static int              g_nOpened = 0;             
//static int              g_nPending = 0;

static DECLARE_WAIT_QUEUE_HEAD( g_wqIRQWaitQueue );

unsigned long g_nIRQFlags;

#define ADZ2_IOC_MAGIC  'z'
#define ADZ2_ARM_IRQ   _IO(ADZ2_IOC_MAGIC,1)
#define ADZ2_FIRE_IRQ  _IO(ADZ2_IOC_MAGIC,2)

// Handles IRQ event, disabling interrupt
static irqreturn_t adz2_intr_handler( 
        int nIrq, 
        void* pContext 
    )
{
    printk( KERN_DEBUG DEVICE_NAME ": adz2_intr_handler interrupt detected.\n");
    spin_lock_irqsave(&g_slIrqLock, g_nIRQFlags);
    disable_irq_nosync( FPGA_IRQ_INDEX );
    g_nIRQState = FPGA_IRQ_DISABLED;
    wake_up_all( &g_wqIRQWaitQueue );
    //wake_up_interruptible( &g_wqIRQWaitQueue );
    spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
    return IRQ_HANDLED;
}

static void _adz2_rearm(void)
{
    spin_lock_irqsave(&g_slIrqLock, g_nIRQFlags);
    if( g_nIRQState == FPGA_IRQ_DISABLED )
    {
        enable_irq( FPGA_IRQ_INDEX );
        g_nIRQState = FPGA_IRQ_ENABLED;
        printk( KERN_DEBUG DEVICE_NAME ": _adz2_rearm enabled interrupt.\n");
    }
    else
    {
        printk( KERN_DEBUG DEVICE_NAME ": _adz2_rearm interrupt is currently enabled.\n");
    }
    spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
}

static void _adz2_cancel(void)
{
    spin_lock_irqsave(&g_slIrqLock, g_nIRQFlags);
    if( g_nIRQState != FPGA_IRQ_DISABLED )
    {
        disable_irq_nosync( FPGA_IRQ_INDEX );
        g_nIRQState = FPGA_IRQ_DISABLED;
        printk( KERN_DEBUG DEVICE_NAME ": _adz2_cancel disabled interrupt.\n");
        //wake_up_interruptible( &g_wqIRQWaitQueue );
    }
    else
    {
        printk( KERN_DEBUG DEVICE_NAME ": _adz2_cancel interrupt already disabled.\n");
    }
    wake_up_all( &g_wqIRQWaitQueue );
    spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
}

static long adz2_ioctl( //struct inode *inode,
        struct file *file,
	unsigned int ioctl_num,
	unsigned long ioctl_param
    )
{
    if( _IOC_TYPE(ioctl_num) != ADZ2_IOC_MAGIC)
    {
        printk( KERN_DEBUG DEVICE_NAME ": adz2_ioctl unexpected _IOC_TYPE.\n");
        return ENOTTY;
    }
    
    if( ioctl_num == ADZ2_ARM_IRQ )
    {
        printk( KERN_DEBUG DEVICE_NAME ": adz2_ioctl rearmed interrupt.\n");
        _adz2_rearm();
        return 0;
    }
    else if( ioctl_num == ADZ2_FIRE_IRQ )
    {
        printk( KERN_DEBUG DEVICE_NAME ": adz2_ioctl fired/cancelled interrupt.\n");
        _adz2_cancel();
        return 0;
    }
    else
    {
        printk( KERN_DEBUG DEVICE_NAME ": adz2_ioctl unexpected _IOC_TYPE.\n");
        return EINVAL;
    }
}

static unsigned int adz2_poll(
        struct file *file, 
        poll_table *wait
    )
{
    unsigned int nReturn = 0;
    printk( KERN_DEBUG DEVICE_NAME ": adz2_poll called.\n");
    /*
    spin_lock_irqsave(&g_slIrqLock, g_nIRQFlags);
    if( g_nIRQState == FPGA_IRQ_DISABLED )
    {
        nReturn = POLLIN;
    }
    spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
    if( nReturn != 0 )
    {
        printk( KERN_DEBUG DEVICE_NAME ": adz2_poll returning without waiting (interrupt disabled).\n");
        return nReturn;
    }*/
    printk( KERN_DEBUG DEVICE_NAME ": adz2_poll waiting for interrupt.\n");
    poll_wait(file, &g_wqIRQWaitQueue, wait);
    spin_lock_irqsave(&g_slIrqLock, g_nIRQFlags);
    if( g_nIRQState == FPGA_IRQ_DISABLED )
    {
        nReturn = POLLIN;
    }
    spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
    printk( KERN_DEBUG DEVICE_NAME ": adz2_poll returning after waking up.\n");
    return nReturn;
}

static int adz2_open(
        struct inode *inode, 
        struct file *filp
    )
{
    int nError = 0;
    printk( KERN_DEBUG DEVICE_NAME ": adz2_open.\n");
    spin_lock( &g_slOpen );
    if( g_nOpened == 0 )
    {
        g_nOpened++;
    }
    else
    {
        nError = -EBUSY;
    }
    spin_unlock( &g_slOpen );
    return nError;
}

static int adz2_release(
        struct inode *inode, 
        struct file *filp
    )
{
    _adz2_cancel();
    printk( KERN_DEBUG DEVICE_NAME ": adz2_release.\n");
    spin_lock( &g_slOpen );
    if( g_nOpened == 1 )
    {
        g_nOpened--;
    }
    spin_unlock( &g_slOpen );
    return 0;
}

static ssize_t adz2_read( 
        struct file * pFile, 
        char* szData, 
        size_t nLen, 
        loff_t* pOff
    )
{
    printk( KERN_DEBUG DEVICE_NAME ": adz2_read.\n");
    return 0;
}

static ssize_t adz2_write(
        struct file * pFile, 
        const char* szData, 
        size_t nLen, 
        loff_t* pOff 
    )
{
    printk( KERN_DEBUG DEVICE_NAME ": adz2_write.\n");
    return 0;
}

static struct file_operations device_ops = {
    .owner   = THIS_MODULE,
    .open    = adz2_open,
    .release = adz2_release,
    .read = adz2_read,
    .write = adz2_write,
    .poll = adz2_poll,
    .unlocked_ioctl = adz2_ioctl,
    .compat_ioctl = adz2_ioctl
};

int init_module(void)
{
    printk( KERN_INFO DEVICE_NAME ": init_module started.\n");

    spin_lock_init(&g_slIrqLock);
    spin_lock_init(&g_slOpen);

    if( class_register(&adz2_class) )
    {
            printk(KERN_ERR DEVICE_NAME ": Failed to class_register\n");
            return -1;
    }

    if( alloc_chrdev_region( &g_sDevNo , 0, 1, DEVICE_NAME ) )
    {
            printk(KERN_ERR DEVICE_NAME ": Failed to alloc_chrdev_region\n" ); 	
            class_unregister(&adz2_class);
            return -1;
    }

    g_pAdz2Cdev = cdev_alloc();
    g_pAdz2Cdev->ops = &device_ops; 
    g_pAdz2Cdev->owner = THIS_MODULE; 	

    if(cdev_add( g_pAdz2Cdev, g_sDevNo, 1) < 0 ) 
    {
        printk( KERN_ERR DEVICE_NAME ": Unable to add cdev\n");
        unregister_chrdev_region(g_sDevNo, 1);	
        class_unregister(&adz2_class);
        return -1;
    }

    g_nDevNum = MKDEV( MAJOR(g_sDevNo), MINOR(g_sDevNo) + g_nDevIndex );

    g_pAdz2Device = device_create( 
            &adz2_class, (struct device*)0,
            g_nDevNum, (void*)0, /* drvdata */
            "%s%d",
            DEVICE_NAME,
            g_nDevIndex
        );
    if( g_pAdz2Device == 0 ) 
    {
        printk( KERN_ERR DEVICE_NAME ": Failed to create device\n");
        cdev_del(g_pAdz2Cdev);
        unregister_chrdev_region(g_sDevNo, 1);	
        class_unregister(&adz2_class);
        return -1;
    }	
	
    spin_lock_irqsave(&g_slIrqLock, g_nIRQFlags);
    if( request_irq( FPGA_IRQ_INDEX, adz2_intr_handler, 0, DEVICE_NAME, NULL ) != 0 )
    {
        spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
        printk(KERN_ERR DEVICE_NAME ": cannot register IRQ %d\n", FPGA_IRQ_INDEX );
        return -1;
    }
    disable_irq( FPGA_IRQ_INDEX );
    g_nIRQState = FPGA_IRQ_DISABLED;
    spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
    printk( KERN_INFO DEVICE_NAME ": registered IRQ %d\n", FPGA_IRQ_INDEX );
    printk( KERN_INFO DEVICE_NAME ": init_module finished.\n");
    return 0;
}

void cleanup_module()
{
    printk( KERN_INFO DEVICE_NAME ": cleanup_module started.\n");
    spin_lock_irqsave(&g_slIrqLock, g_nIRQFlags);
    if( g_nIRQState == FPGA_IRQ_ENABLED )
    {
        disable_irq( FPGA_IRQ_INDEX );
    }
    free_irq( FPGA_IRQ_INDEX, NULL );
    spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
    device_destroy( &adz2_class, g_nDevNum );
    cdev_del(g_pAdz2Cdev);
    unregister_chrdev_region(g_sDevNo, 1);	
    class_unregister(&adz2_class);
    printk( KERN_INFO DEVICE_NAME ": cleanup_module finished.\n");
}

MODULE_LICENSE("GPL");

