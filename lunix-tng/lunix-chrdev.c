/*
 * lunix-chrdev.c
 *
 * Implementation of character devices
 * for Lunix:TNG
 *
 * < Your name here >
 *
 */

#include <linux/mm.h>
#include <linux/fs.h>
#include <linux/init.h>
#include <linux/list.h>
#include <linux/cdev.h>
#include <linux/poll.h>
#include <linux/slab.h>
#include <linux/sched.h>
#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/mmzone.h>
#include <linux/vmalloc.h>
#include <linux/spinlock.h>

#include "lunix.h"
#include "lunix-chrdev.h"
#include "lunix-lookup.h"

/*
 * Global data
 */
struct cdev lunix_chrdev_cdev;


/*
 * Just a quick [unlocked] check to see if the cached
 * chrdev state needs to be updated from sensor measurements.
 */
static int lunix_chrdev_state_needs_refresh(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	WARN_ON ( !(sensor = state->sensor));
	
	if (state->buf_timestamp == sensor->msr_data[state->type]->last_update)
		return 0;
	
	return 1;

}

/*
 * Updates the cached state of a character device
 * based on sensor data. Must be called with the
 * character device state lock held.
 */
static int lunix_chrdev_state_update(struct lunix_chrdev_state_struct *state)
{
	struct lunix_sensor_struct *sensor;
	
	debug("leaving\n");

	/*
	 * Grab the raw data quickly, hold the
	 * spinlock for as little as possible.
	 */
	
	sensor=state->sensor;

	if (!lunix_chrdev_state_needs_refresh(state))
        return -EAGAIN;
	uint32_t rawdata;
	
	spin_lock(&sensor->lock);
	state->buf_timestamp=sensor->msr_data[state->type]->last_update;
	rawdata=sensor->msr_data[state->type]->values[0];
	spin_unlock(&sensor->lock);
	/* Why use spinlocks? See LDD3, p. 119 */

	/*
	 * Any new data available?
	 */
	/*
	if (lunix_chrdev_state_needs_refresh(state)==0)
           goto out;	
	*/
	/*
	 * Now we can take our time to format them,
	 * holding only the private state semaphore
	 */

	
	long mydata;
	
	switch (state->type){
		case BATT: mydata=lookup_voltage[rawdata]; break;
		case TEMP: mydata=lookup_temperature[rawdata]; break;
		case LIGHT: mydata=lookup_light[rawdata]; break;
	}

	int integer_part = mydata / 1000;
	int decimal_part = mydata % 1000;

	state->buf_lim = snprintf(state->buf_data, LUNIX_CHRDEV_BUFSZ, "%d.%d\n", integer_part, decimal_part);
	
out:
	debug("leaving\n");
	return 0;
}

/*************************************
 * Implementation of file operations
 * for the Lunix character device
 *************************************/

static int lunix_chrdev_open(struct inode *inode, struct file *filp)
{
    /* Declarations */
    int minor = iminor(inode);
    int type = minor & 7; /* check if type = 0 ,1 ,2*/
    int sensor_index = (minor - type) >> 3; /*sensor index = (minor - type) / 8 */

    struct lunix_chrdev_state_struct *dev = (struct lunix_chrdev_state_struct*) kmalloc(sizeof(struct lunix_chrdev_state_struct), GFP_KERNEL);
    dev->type = type;
    dev->sensor = lunix_sensors + sensor_index;
    dev->buf_lim = 1;
    dev->buf_data[0] = '\0';
    dev->buf_timestamp = 0;
    sema_init(&dev->lock, 1);

    int ret;
    
    debug("entering\n");
    ret = -ENODEV;
    if ((ret = nonseekable_open(inode, filp)) < 0)
        goto out;

    filp->private_data = dev;
    /*
     * Associate this open file with the relevant sensor based on
     * the minor number of the device node [/dev/sensor<NO>-<TYPE>]
     */
    
    /* Allocate a new Lunix character device private state structure */
    /* ? */
out:
    debug("leaving, with ret = %d\n", ret);
    return ret;
}

static int lunix_chrdev_release(struct inode *inode, struct file *filp)
{
	debug("Freeing the resources allocated in filp->private_data");
	kfree(filp->private_data);
	return 0;
}

static long lunix_chrdev_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	/* Why? */
	return -EINVAL;
}

static ssize_t lunix_chrdev_read(struct file *filp, char __user *usrbuf, size_t cnt, loff_t *f_pos)
{
	ssize_t ret;

	struct lunix_sensor_struct *sensor;
	struct lunix_chrdev_state_struct *state;

	state = filp->private_data;
	WARN_ON(!state);

	sensor = state->sensor;
	WARN_ON(!sensor);

	/* Lock? */
	
	if (down_interruptible(&state->lock))
        	return -ERESTARTSYS;
	/*
	 * If the cached character device state needs to be
	 * updated by actual sensor data (i.e. we need to report
	 * on a "fresh" measurement, do so
	 */
	if (*f_pos == 0) {
		while (lunix_chrdev_state_update(state) == -EAGAIN) {
			/* ? */
			/* The process needs to sleep */
			/* See LDD3, page 153 for a hint */
			
			up(&state->lock);
			if (filp->f_flags & O_NONBLOCK)
            	return -EAGAIN;
            	//PDEBUG("\"%s\" reading: going to sleep\n", current->comm);
            
			if (wait_event_interruptible(sensor->wq, (lunix_chrdev_state_needs_refresh(state))))
          		return -ERESTARTSYS; 
          	
			if (down_interruptible(&state->lock))
           		return -ERESTARTSYS;
		}
	}

	/* End of file */
	
	if(*f_pos==state->buf_lim){
		ret = 0;
		*f_pos = 0; /* Auto-rewind on EOF mode? */
		goto out;
	}
	/* Determine the number of cached bytes to copy to userspace */
	
	if(cnt > state->buf_lim)
		cnt = state->buf_lim;
	
	if (copy_to_user(usrbuf, state->buf_data, cnt)) {
        	ret = -EFAULT;
        	goto out;
    }

    	*f_pos += cnt;
    	ret = cnt;	
	*f_pos = 0;
	
out:
	/* Unlock? */
	up(&state->lock);
	return ret;
}

static int lunix_chrdev_mmap(struct file *filp, struct vm_area_struct *vma)
{
	return -EINVAL;
}

static struct file_operations lunix_chrdev_fops = 
{
    .owner          = THIS_MODULE,
	.open           = lunix_chrdev_open,
	.release        = lunix_chrdev_release,
	.read           = lunix_chrdev_read,
	.unlocked_ioctl = lunix_chrdev_ioctl,
	.mmap           = lunix_chrdev_mmap
};
//check LDD Ch.3 p.57
static void lunix_setup_cdev (struct cdev *dev, int index) {
	int err, devno = MKDEV(LUNIX_CHRDEV_MAJOR, index);
	cdev_init(dev, &lunix_chrdev_fops);
	(*dev).owner = THIS_MODULE;
	(*dev).ops = &lunix_chrdev_fops;
	err = cdev_add(dev, devno, 1);
	if (err)
		printk(KERN_NOTICE "Error %d adding lunix%d", err, index);
}

int lunix_chrdev_init(void)
{
	/*
	 * Register the character device with the kernel, asking for
	 * a range of minor numbers (number of sensors * 8 measurements / sensor)
	 * beginning with LINUX_CHRDEV_MAJOR:0
	 */
	int ret;
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
	
	debug("initializing character device\n");
	cdev_init(&lunix_chrdev_cdev, &lunix_chrdev_fops);
	lunix_chrdev_cdev.owner = THIS_MODULE;
	
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	
	ret = register_chrdev_region(dev_no, lunix_minor_cnt, "lunix");

	if (ret < 0) {
		debug("failed to register region, ret = %d\n", ret);
		goto out;
	}	
	
	int i, j;
	for(i = 0; i < lunix_sensor_cnt; i++)
		for(j = 0; j < 3; j++){
			int minor = i * 8 + j;
			lunix_setup_cdev(&lunix_chrdev_cdev, minor);
		}

	if (ret < 0) {
		debug("failed to add character device\n");
		goto out_with_chrdev_region;
	}
	debug("completed successfully\n");
	return 0;

out_with_chrdev_region:
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
out:
	return ret;
}

void lunix_chrdev_destroy(void)
{
	dev_t dev_no;
	unsigned int lunix_minor_cnt = lunix_sensor_cnt << 3;
		
	debug("entering\n");
	dev_no = MKDEV(LUNIX_CHRDEV_MAJOR, 0);
	cdev_del(&lunix_chrdev_cdev);
	unregister_chrdev_region(dev_no, lunix_minor_cnt);
	debug("leaving\n");
}
