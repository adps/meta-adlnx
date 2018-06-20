
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/fs.h>
#include <linux/cdev.h>
#include <linux/interrupt.h>
#include <linux/wait.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <linux/poll.h>

#define DEVICE_NAME "adz2"

#define FPGA_IRQ_DISABLED 	0
#define FPGA_IRQ_ENABLED	1

struct adz2_local 
{
	struct device *dev;
	int nIrq;
	int nIRQState;
	spinlock_t slIrqLock;
	spinlock_t slOpen;
	dev_t dDevNo;
	struct cdev Adz2Cdev;
	struct device* pAdz2Device;
	int nDevNum;
	int nDevIndex;
	int nOpened;	  
//	int nPending;
	unsigned long nIRQFlags;
};

static DECLARE_WAIT_QUEUE_HEAD( g_wqIRQWaitQueue );

static struct adz2_local *id = NULL;

#define ADZ2_IOC_MAGIC  'z'
#define ADZ2_ARM_IRQ   _IO(ADZ2_IOC_MAGIC,1)
#define ADZ2_FIRE_IRQ  _IO(ADZ2_IOC_MAGIC,2)

static struct class adz2_class = 
{
    .name = DEVICE_NAME,
    .owner = THIS_MODULE,
};

// Handles IRQ event, disabling interrupt
static irqreturn_t adz2_intr_handler(int nIrq, void* pContext )
{
  //	struct adz2_local *id = pContext;
	printk( KERN_DEBUG DEVICE_NAME ": adz2_intr_handler interrupt detected.\n");
	spin_lock_irqsave(&(id->slIrqLock), id->nIRQFlags);
	disable_irq_nosync( id->nIrq );
	id->nIRQState = FPGA_IRQ_DISABLED;
	wake_up_all( &g_wqIRQWaitQueue );
	//wake_up_interruptible(&g_wqIRQWaitQueue);
	spin_unlock_irqrestore(&(id->slIrqLock), id->nIRQFlags);
	return IRQ_HANDLED;
}


static void _adz2_rearm(void)
{
	spin_lock_irqsave(&(id->slIrqLock), id->nIRQFlags);
	if( id->nIRQState == FPGA_IRQ_DISABLED )
	{
		printk( KERN_INFO DEVICE_NAME ": Enabling IRQ.\n");
		enable_irq( id->nIrq );
		id->nIRQState = FPGA_IRQ_ENABLED;
		printk( KERN_INFO DEVICE_NAME ": _adz2_rearm enabled interrupt.\n");
	}
	else
	{
	      printk( KERN_INFO DEVICE_NAME ": _adz2_rearm interrupt is currently enabled.\n");
	}
	spin_unlock_irqrestore(&(id->slIrqLock), id->nIRQFlags);
}

static void _adz2_cancel(void)
{
	spin_lock_irqsave(&(id->slIrqLock), id->nIRQFlags);
	if( id->nIRQState != FPGA_IRQ_DISABLED )
	{
		printk( KERN_INFO DEVICE_NAME ": Disabling IRQ1.\n");
		disable_irq_nosync( id->nIrq );
		id->nIRQState = FPGA_IRQ_DISABLED;
		printk( KERN_DEBUG DEVICE_NAME ": _adz2_cancel disabled interrupt.\n");
		//wake_up_interruptible( &g_wqIRQWaitQueue );
	}
	else
	{
		printk( KERN_DEBUG DEVICE_NAME ": _adz2_cancel interrupt already disabled.\n");
	}
	wake_up_all( &g_wqIRQWaitQueue );
	spin_unlock_irqrestore(&(id->slIrqLock), id->nIRQFlags);
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
	
	//spin_lock_irqsave(&g_slIrqLock, g_nIRQFlags);
	//if( g_nIRQState == FPGA_IRQ_DISABLED )
	//{
	//	nReturn = POLLIN;
	//}
	//spin_unlock_irqrestore(&g_slIrqLock, g_nIRQFlags);
	//if( nReturn != 0 )
	//{
	//	printk( KERN_DEBUG DEVICE_NAME ": adz2_poll returning without waiting (interrupt disabled).\n");
	//	return nReturn;
	//}
	printk( KERN_DEBUG DEVICE_NAME ": adz2_poll waiting for interrupt.\n");
	poll_wait(file, &g_wqIRQWaitQueue, wait);
	spin_lock_irqsave(&(id->slIrqLock), id->nIRQFlags);
	if( id->nIRQState == FPGA_IRQ_DISABLED )
	{
		nReturn = POLLIN;
	}
	spin_unlock_irqrestore(&(id->slIrqLock), id->nIRQFlags);
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
	spin_lock( &(id->slOpen) );
	if( id->nOpened == 0 )
	{
		id->nOpened++;
	}
	else
	{
		nError = -EBUSY;
	}
	spin_unlock( &(id->slOpen) );
	return nError;
}


  static int adz2_release(
	struct inode *inode, 
	struct file *filp
	)
{
	_adz2_cancel();
	printk( KERN_DEBUG DEVICE_NAME ": adz2_release.\n");
	spin_lock( &(id->slOpen) );
	if( id->nOpened == 1 )
	{
		id->nOpened--;
	}
	spin_unlock( &(id->slOpen) );
	return 0;
	}

static ssize_t adz2_read( 
	struct file * pFile, 
	char* szData, 
	size_t nLen, 
	loff_t* pOff
	)
{
	printk( KERN_INFO DEVICE_NAME ": adz2_read.\n");
	return 0;
}

static ssize_t adz2_write(
	struct file * pFile, 
	const char* szData, 
	size_t nLen, 
	loff_t* pOff 
	)
{
	printk( KERN_INFO DEVICE_NAME ": adz2_write.\n");
	return 0;
}

  static struct file_operations device_ops = {
	.owner   = THIS_MODULE,
	.open    = adz2_open,
	.release = adz2_release,
	.read    = adz2_read,
	.write   = adz2_write,
	.poll    = adz2_poll,
	.unlocked_ioctl = adz2_ioctl,
	.compat_ioctl = adz2_ioctl
	};

static int adz2_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	int rc = 0;

	printk( KERN_INFO DEVICE_NAME ": Module probe vers 15.\n");

	id = devm_kzalloc(&pdev->dev,sizeof(*id),GFP_KERNEL);
	if(!id)
	{
		dev_err(dev,"Cound not allocate adz2 device\n");
		//Nothing needs to be done before returning here
		return -ENOMEM;
	}

	id->dev = &pdev->dev;
	platform_set_drvdata(pdev, id);

	spin_lock_init(&(id->slIrqLock));
	spin_lock_init(&(id->slOpen));

	id->nIrq = platform_get_irq(pdev,0);
	printk( KERN_INFO DEVICE_NAME 
		": Got IRQ details:\n");
	printk( KERN_INFO DEVICE_NAME 
		":   DEVICE_NAME: %s\n", DEVICE_NAME);
	printk( KERN_INFO DEVICE_NAME 
		":   IRQ Number : %d\n", id->nIrq);

	rc = devm_request_irq(&pdev->dev, id->nIrq, adz2_intr_handler, 0,
		DEVICE_NAME, id);
	if (rc)
	{
		printk( KERN_INFO DEVICE_NAME 
			": Could not allocate interrupt %d.\n",id->nIrq);
		//Nothing needs to be done before returning here
	        return rc;		
	}
	
	printk( KERN_INFO DEVICE_NAME 
		": Allocating chrdev_region\n");
	rc = alloc_chrdev_region( &(id->dDevNo) , 0, 1, DEVICE_NAME );
	if (rc)
	{
		printk(KERN_ERR DEVICE_NAME ": Failed to alloc_chrdev_region\n" ); 
		return rc;
	}
       
	printk( KERN_INFO DEVICE_NAME 
		": Initialising cdev\n");
	cdev_init(&(id->Adz2Cdev), &device_ops);
	//id->Adz2Cdev->ops = &device_ops; 
	id->Adz2Cdev.owner = THIS_MODULE; 	
	
	printk( KERN_INFO DEVICE_NAME 
		": Adding cdev\n");
	rc = cdev_add(&(id->Adz2Cdev), id->dDevNo, 1);
	if (rc < 0) 
	{
		printk( KERN_ERR DEVICE_NAME ": Unable to add cdev\n");
		goto error1;
	}

	printk( KERN_INFO DEVICE_NAME 
		": Registering class\n");
	if (class_register(&adz2_class))
	{
		printk(KERN_ERR DEVICE_NAME ": Failed to class_register\n");
		goto error2;
	}
	id->nDevNum = MKDEV( MAJOR(id->dDevNo), MINOR(id->dDevNo) + id->nDevIndex);

	printk( KERN_INFO DEVICE_NAME 
		": Creating device\n");
	id->pAdz2Device = device_create( 
			      &adz2_class, (struct device*)0,
			      id->nDevNum, (void*)0, // drvdata
			      "%s%d",
			      DEVICE_NAME,
			      id->nDevIndex);
	if( id->pAdz2Device == 0 ) 
	{
		printk( KERN_ERR DEVICE_NAME ": Failed to create device\n");
		rc = -1;
		goto error3;
	}
	
	spin_lock_irqsave(&(id->slIrqLock), id->nIRQFlags);

	printk( KERN_INFO DEVICE_NAME ": Disabling IRQ2.\n");
	disable_irq(id->nIrq);

	id->nIRQState = FPGA_IRQ_DISABLED;
	spin_unlock_irqrestore(&(id->slIrqLock), id->nIRQFlags);

	printk( KERN_INFO DEVICE_NAME ": registered IRQ %d\n", id->nIrq);
	printk( KERN_INFO DEVICE_NAME ": init_module finished.\n");

	printk( KERN_INFO DEVICE_NAME ": IRQ State: %d\n", id->nIRQState);
	_adz2_rearm();
	printk( KERN_INFO DEVICE_NAME ": IRQ State: %d\n", id->nIRQState);


	printk( KERN_INFO DEVICE_NAME 
		": Module probed\n");
	return 0;

 error3: 
	class_unregister(&adz2_class);
 error2: 
	cdev_del(&(id->Adz2Cdev));	
 error1:
	unregister_chrdev_region(id->dDevNo, 1);
	// error1: 
	//free_irq(id->nIrq,id);

	return rc;
}

static struct of_device_id adz2_of_match[] = {
	{.compatible = "ad-datacard", },
	{/* end of list */},
};

static int adz2_remove(struct platform_device *pdev)
{
	printk( KERN_DEBUG DEVICE_NAME 
		": Removing adz2\n");
        
	device_destroy( &adz2_class, id->nDevNum );
	class_unregister(&adz2_class);
	cdev_del(&(id->Adz2Cdev));
	unregister_chrdev_region(id->dDevNo, 1);

	printk( KERN_DEBUG DEVICE_NAME 
		": adz2 removal complete\n");

	return 0;
}

static struct platform_driver adz2_driver = 
{
	.driver = {
		.name = DEVICE_NAME,
		.owner = THIS_MODULE,
		.of_match_table = adz2_of_match,
	},
	.probe = adz2_probe,
	.remove = adz2_remove,
};

int adz2_init(void)
{
	printk( KERN_INFO DEVICE_NAME ": init_module started.\n");

	if( platform_driver_register(&adz2_driver) )
	{
		printk(KERN_ERR DEVICE_NAME ": Failed to class_register\n");
		return -1;
	}
	return 0;
	}

void adz2_exit(void)
{
	printk( KERN_INFO DEVICE_NAME ": cleanup_module started.\n");

	spin_lock_irqsave(&(id->slIrqLock), id->nIRQFlags);
	printk( KERN_INFO DEVICE_NAME ": spinlock.\n");
	if( id->nIRQState == FPGA_IRQ_ENABLED )
	{
		printk( KERN_INFO DEVICE_NAME ": Disabling IRQ.\n");
		disable_irq( id->nIrq );
	printk( KERN_INFO DEVICE_NAME ": Disable.\n");
	}
	printk( KERN_INFO DEVICE_NAME ": Unregistered.\n");
	spin_unlock_irqrestore(&(id->slIrqLock), id->nIRQFlags);
	
	platform_driver_unregister(&adz2_driver);

	printk( KERN_INFO DEVICE_NAME ": cleanup_module finished.\n");
}




module_init(adz2_init);
module_exit(adz2_exit);
MODULE_LICENSE("GPL");

