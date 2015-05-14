#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>

#define DEVICE_NAME "adz1"

unsigned int fpga_irq_index = 61;

static struct class ad_datacard_class = 
{
	.name = DEVICE_NAME,
	.owner = THIS_MODULE,
};

#define FPGA_IRQ_NONE 		0
#define FPGA_IRQ_ASSERTED 	1
#define FPGA_IRQ_CANCELLED 	2
#define FPGA_IRQ_CLEAN_UP 	3

static dev_t dev_no;
static struct cdev* ad_datacard_cdev;

spinlock_t irq_lock;

struct device* ad_datacard_device;

static DECLARE_WAIT_QUEUE_HEAD( irq_wait_queue );

volatile int g_nInstances = 0;
volatile int g_nIRQEnables = 0;
volatile int g_nIRQState = FPGA_IRQ_NONE;
volatile int g_nWaitingThreads = 0;


#define _DEBUG

static irqreturn_t intr_handler(int irq, void* pContext )
{
#ifdef _DEBUG
	printk( DEVICE_NAME": FPGA interrupt detected (%d)!\n", irq );
#endif
	spin_lock(&irq_lock);
	while( g_nIRQEnables != 0 )
	{
		g_nIRQEnables--;
		disable_irq_nosync( fpga_irq_index );
#ifdef _DEBUG
		printk( DEVICE_NAME": IRQ disabled.\n" );
#endif
	}
	g_nIRQState = FPGA_IRQ_ASSERTED;
	spin_unlock(&irq_lock);
	wake_up_all( &irq_wait_queue );
#ifdef _DEBUG
	printk( DEVICE_NAME": FPGA interrupt finished waking threads!\n");
#endif
	return IRQ_HANDLED;
}

static int addatacard_open(struct inode *inode, struct file *filp)
{
	spin_lock(&irq_lock);
	g_nInstances++;
	spin_unlock(&irq_lock);
	printk(KERN_ERR DEVICE_NAME": opened.\n");
	return 0;
}

static int addatacard_release(struct inode *inode, struct file *filp)
{
	spin_lock(&irq_lock);
	g_nInstances--;
	g_nIRQState = FPGA_IRQ_CANCELLED;
	wake_up_all( &irq_wait_queue );
	spin_unlock(&irq_lock);
	printk(KERN_ERR DEVICE_NAME": closed.\n");
	return 0;
}

static ssize_t addatacard_read(struct file * pFile, char* szData, size_t nLen, loff_t* pOff )
{
	int ret = 1;
	DEFINE_WAIT(wait);
	if( nLen != 1 )
	{
#ifdef _DEBUG
		printk( DEVICE_NAME": addatacard_read called with size != 1.\n");
#endif
		return 0;
	}
	g_nWaitingThreads++;
#ifdef _DEBUG
	printk( DEVICE_NAME": User waiting for interrupt.\n");
#endif
	while( g_nIRQState == FPGA_IRQ_NONE )
	{
#ifdef _DEBUG
		printk( DEVICE_NAME": User thread sleeping...\n");
#endif
		prepare_to_wait( &irq_wait_queue, &wait, TASK_INTERRUPTIBLE );
		schedule();
		if (signal_pending(current))
		{
			ret = -EAGAIN;
			finish_wait( &irq_wait_queue, &wait );
			break;
		}
		else
		{
			finish_wait( &irq_wait_queue, &wait );
		}
	}
	szData[0] = g_nIRQState;
#ifdef _DEBUG
	printk( DEVICE_NAME": User interrupt detected.\n");
#endif
	g_nWaitingThreads--;
	return ret;
}

static ssize_t addatacard_write(struct file * pFile, const char* szData, size_t nLen, loff_t* pOff )
{
	if( szData[0] == 0 )
	{
#ifdef _DEBUG
		printk( DEVICE_NAME": User enabled interrupt\n");
#endif
		spin_lock(&irq_lock);
		g_nIRQState = FPGA_IRQ_NONE;
		if( g_nIRQEnables == 0 )
		{
			enable_irq( fpga_irq_index );
			g_nIRQEnables++;
		}
		spin_unlock(&irq_lock);
	}
	else
	{
#ifdef _DEBUG
		printk( DEVICE_NAME": User cancelled interrupt\n");
#endif
		spin_lock(&irq_lock);
		g_nIRQState = FPGA_IRQ_CANCELLED;
		while( g_nIRQEnables > 0 )
		{
			disable_irq( fpga_irq_index );
			wake_up( &irq_wait_queue );
			g_nIRQEnables--;
		}
		//wake_up_all( &irq_wait_queue );
		spin_unlock(&irq_lock);
	}
	return 1;
}

static struct file_operations device_ops = {
        .owner   = THIS_MODULE,
        .open    = addatacard_open,
        .release = addatacard_release,
        .read = addatacard_read,
        .write = addatacard_write,
};

static int devNum = 0;
static int devIndex = 0;
	
int init_module(void)
{
	printk( DEVICE_NAME": Loading...\n");
	
	spin_lock_init(&irq_lock);
	
	if( class_register(&ad_datacard_class) )
	{
		printk(DEVICE_NAME": Failed to class_register\n");
		return -1;
	}
	
	if( alloc_chrdev_region( &dev_no , 0, 1, DEVICE_NAME ) )
	{
		printk(DEVICE_NAME": Failed to alloc_chrdev_region\n" ); 	
		class_unregister(&ad_datacard_class);
		return -1;
	}
		
	ad_datacard_cdev = cdev_alloc();
		
	ad_datacard_cdev->ops = &device_ops; 
	ad_datacard_cdev->owner = THIS_MODULE; 	
	
	if(cdev_add( ad_datacard_cdev, dev_no, 1) < 0 ) 
	{
		printk(KERN_INFO "Unable to add cdev");
		unregister_chrdev_region(dev_no, 1);	
		class_unregister(&ad_datacard_class);
		return -1;
	}	
	
	devNum = MKDEV( MAJOR(dev_no), MINOR(dev_no) + devIndex );
	
	ad_datacard_device = device_create( &ad_datacard_class, (struct device*)0,
					       devNum, (void*)0, /* drvdata */
					       "%s%d",
					       DEVICE_NAME,
					       devIndex );
    if( ad_datacard_device == 0 ) 
	{
		printk( DEVICE_NAME": Failed to create device\n");
		cdev_del(ad_datacard_cdev);
		unregister_chrdev_region(dev_no, 1);	
		class_unregister(&ad_datacard_class);
		return -1;
    }	
	
	spin_lock(&irq_lock);
	if( request_irq( fpga_irq_index, intr_handler, 0, DEVICE_NAME, NULL ) != 0 )
	{
		spin_unlock(&irq_lock);
		printk(KERN_ERR DEVICE_NAME": cannot register IRQ %d\n", fpga_irq_index);
		return -1;
	}
	disable_irq( fpga_irq_index );
	g_nIRQEnables = 0;
	printk(KERN_ERR DEVICE_NAME": registered IRQ %d\n", fpga_irq_index);
	spin_unlock(&irq_lock);
	
	printk( DEVICE_NAME": Module loaded.\n");
	return 0;
}

void cleanup_module(void)
{
	spin_lock(&irq_lock);
	while( g_nIRQEnables == 0 )
	{
		enable_irq( g_nIRQEnables );
		g_nIRQEnables++;
	}
	while( g_nIRQEnables > 1 )
	{
		disable_irq( g_nIRQEnables );
		g_nIRQEnables--;
	}
	free_irq(fpga_irq_index, NULL );
	g_nIRQEnables = 0;
	spin_unlock(&irq_lock);
	
	device_destroy( &ad_datacard_class, devNum );
	
	cdev_del(ad_datacard_cdev);
	
	unregister_chrdev_region(dev_no, 1);	
	
	class_unregister(&ad_datacard_class);

	printk( DEVICE_NAME": Unloaded.\n");
}

MODULE_LICENSE("GPL");
