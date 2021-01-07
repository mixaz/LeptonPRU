/*
 * Kernel module for IR camera FLIR Lepton using SPI bit-banging on PRU core
 * Designed to be used in conjunction with a modified pru_rproc driver.
 *
 * This file is a part of the LeptonPRU project.
 *
 * Copyright (C) 2018-2019 Mikhail Zemlyanukha <gmixaz@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.
 */

#include <asm/atomic.h>
#include <asm/uaccess.h>
#include <asm/div64.h>

#include <linux/module.h>
#include <linux/err.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wait.h>
#include <linux/poll.h>

#include <linux/platform_device.h>
#include <linux/pruss.h>
#include <linux/remoteproc.h>
#include <linux/miscdevice.h>

#include <linux/io.h>
#include <linux/interrupt.h>
#include <linux/irqreturn.h>
#include <linux/slab.h>
#include <linux/genalloc.h>
#include <linux/mm.h>
#include <linux/dma-mapping.h>

#include <linux/kobject.h>
#include <linux/string.h>

#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/of_address.h>
#include <linux/of_device.h>

#include <linux/sysfs.h>
#include <linux/fs.h>

#include <linux/ktime.h>

#include "leptonpru.h"
#include "leptonpru_int.h"

#define USE_PRUS 1

/* Buffer states */
enum bufstates {
	STATE_BL_BUF_ALLOC,
	STATE_BL_BUF_MAPPED,
	STATE_BL_BUF_UNMAPPED,
	STATE_BL_BUF_DROPPED
};

/* Forward declration */
static const struct file_operations pru_beaglelogic_fops;

struct logic_buffer {
	void *buf;
	dma_addr_t phys_addr;

	unsigned short state;
	unsigned short read_state;
};

struct beaglelogic_private_data {
	const char *fw_names[PRUSS_NUM_PRUS];
};

struct beaglelogicdev {
	/* Misc device descriptor */
	struct miscdevice miscdev;

	/* Handle to pruss structure and PRU0 SRAM */
	struct pruss *pruss;
	struct rproc *pru0;
	struct pruss_mem_region pru0sram;
	const struct beaglelogic_private_data *fw_data;

	/* IRQ numbers */
	uint16_t to_bl_irq;
	uint16_t from_bl_irq_1;
	uint16_t from_bl_irq_2;

	/* Private data */
	struct device *p_dev; /* Parent platform device */

	/* Locks */
	struct mutex mutex;

	/* Buffer management */
	struct logic_buffer buffers[FRAMES_NUMBER];
	wait_queue_head_t wait;

	/* ISR Bookkeeping */
	uint32_t previntcount;	/* Previous interrupt count read from PRU */

	/* Firmware capabilities */
	struct capture_context *cxt_pru;

	/* State */
	uint32_t state;
	uint32_t lasterror;
};

struct logic_buffer_reader {
	struct beaglelogicdev *bldev;
};

#define to_beaglelogicdev(dev)	container_of((dev), \
		struct beaglelogicdev, miscdev)

#define DRV_NAME		"leptonpru"
#define DRV_VERSION		"0.1"

/* Begin Buffer Management section */

/* Allocate DMA buffers for the PRU
 * This method acquires & releases the device mutex */
static int beaglelogic_memalloc(struct device *dev)
{
	struct beaglelogicdev *bldev = dev_get_drvdata(dev);
	int i;
	void *buf;

	/* Check if BL is in use */
	if (!mutex_trylock(&bldev->mutex))
		return -EBUSY;

	/* Allocate DMA buffers */
	for (i = 0; i < FRAMES_NUMBER; i++) {
		buf = kmalloc(sizeof(leptonpru_mmap), GFP_KERNEL);
		if (!buf)
			goto failrelease;

		/* Fill with 0xFF */
		memset(buf, 0xFF, sizeof(leptonpru_mmap));

		/* Set the buffers */
		bldev->buffers[i].buf = buf;
		bldev->buffers[i].phys_addr = virt_to_phys(buf);
	}

	/* Write log and unlock */
	dev_info(dev, "Successfully allocated %d bytes of memory.\n",
			FRAMES_NUMBER * sizeof(leptonpru_mmap));

	mutex_unlock(&bldev->mutex);

	/* Done */
	return 0;
failrelease:
	for (i = 0; i < FRAMES_NUMBER; i++) {
		if (bldev->buffers[i].buf)
			kfree(bldev->buffers[i].buf);
	}
	dev_err(dev, "Sample buffer allocation:");
	mutex_unlock(&bldev->mutex);
	return -ENOMEM;
}

/* Frees the DMA buffers and the bufferlist */
static void beaglelogic_memfree(struct device *dev)
{
	struct beaglelogicdev *bldev = dev_get_drvdata(dev);
	int i;

	mutex_lock(&bldev->mutex);
	for (i = 0; i < FRAMES_NUMBER; i++)
		if (bldev->buffers[i].buf)
			kfree(bldev->buffers[i].buf);
	mutex_unlock(&bldev->mutex);
}

/* No argument checking for the map/unmap functions */
static int beaglelogic_map_buffer(struct device *dev, struct logic_buffer *buf)
{
	dma_addr_t dma_addr;

	/* If already mapped, do nothing */
	if (buf->state == STATE_BL_BUF_MAPPED)
		return 0;

	dma_addr = dma_map_single(dev, buf->buf, sizeof(leptonpru_mmap), DMA_TO_DEVICE);

	dev_info(dev,"beaglelogic_map_buffer: %x, addr: %x, size: %d\n",buf,dma_addr, sizeof(leptonpru_mmap));

	if (dma_mapping_error(dev, dma_addr))
		goto fail;

	buf->phys_addr = dma_addr;
	buf->state = STATE_BL_BUF_MAPPED;

	return 0;
fail:
	dev_err(dev, "DMA Mapping error. \n");
	return -1;
}

static void beaglelogic_unmap_buffer(struct device *dev,
                                     struct logic_buffer *buf)
{
	dev_info(dev,"beaglelogic_unmap_buffer: %x - %x\n",buf,buf->phys_addr);
	dma_unmap_single(dev, buf->phys_addr, sizeof(leptonpru_mmap), DMA_TO_DEVICE);
	buf->state = STATE_BL_BUF_UNMAPPED;
}

/* Map all the buffers. This is done just before beginning a sample operation
 * NOTE: PRUs are halted at this time */
static int beaglelogic_map_and_submit_all_buffers(struct device *dev)
{
	struct beaglelogicdev *bldev = dev_get_drvdata(dev);
	struct buflist *pru_buflist = bldev->cxt_pru->list_head;
	int i, j;
	dma_addr_t addr;

	dev_info(dev,"beaglelogic_map_and_submit_all_buffers\n");

	/* Write buffer table to the PRU memory */
	for (i = 0; i < FRAMES_NUMBER; i++) {
		if (beaglelogic_map_buffer(dev, &bldev->buffers[i]))
			goto fail;
		addr = bldev->buffers[i].phys_addr;
		pru_buflist[i].dma_start_addr = addr;
		pru_buflist[i].dma_end_addr = addr + sizeof(leptonpru_mmap);
	}

	/* Update state to ready */
	if (i)
		bldev->state = STATE_BL_ARMED;

	return 0;
fail:
	/* Unmap the buffers */
	for (j = 0; j < i; j++)
		beaglelogic_unmap_buffer(dev, &bldev->buffers[j]);

	dev_err(dev, "DMA Mapping failed at i=%d\n", i);

	bldev->state = STATE_BL_ERROR;
	return 1;
}

/* End Buffer Management section */

/* Begin Device Attributes Configuration Section
 * All set operations lock and unlock the device mutex */

/* End Device Attributes Configuration Section */

/* Send command to the PRU firmware */
static int beaglelogic_send_cmd(struct beaglelogicdev *bldev, uint32_t cmd)
{
	struct device *dev = bldev->miscdev.this_device;
#define TIMEOUT     1000000
	uint32_t timeout = TIMEOUT;

	dev_info(dev, "command %d to PRU\n",cmd);

	bldev->cxt_pru->cmd = cmd;

	/* Wait for firmware to process the command */
	while (--timeout && bldev->cxt_pru->cmd != 0)
		cpu_relax();

	if (timeout == 0) {
		dev_info(dev, "timeout when sending command %d to PRU\n",cmd);
		return -100;
	}

	dev_info(dev, "command %d to PRU, response=%d\n",cmd,bldev->cxt_pru->resp);

	return bldev->cxt_pru->resp;
}

/* Request the PRU firmware to stop capturing */
static void beaglelogic_request_stop(struct beaglelogicdev *bldev)
{
	struct device *dev = bldev->miscdev.this_device;
	dev_info(dev, "beaglelogic_request_stop\n");
	/* Trigger interrupt */
	pruss_intc_trigger(bldev->to_bl_irq);
	beaglelogic_send_cmd(bldev,CMD_STOP);
}

/* This is [to be] called from a threaded IRQ handler */
irqreturn_t beaglelogic_serve_irq(int irqno, void *data)
{
	struct beaglelogicdev *bldev = data;
	struct device *dev = bldev->miscdev.this_device;
	uint32_t state;

//	dev_info(dev,"Beaglelogic IRQ #%d\n", irqno);
	if (irqno == bldev->from_bl_irq_1) {
		wake_up_interruptible(&bldev->wait);
	}
	else if (irqno == bldev->from_bl_irq_2) {
		/* This interrupt occurs twice:
		 *  1. After a successful configuration of PRU capture
		 *  2. After stop  */
		state = bldev->state;
		if (state <= STATE_BL_ARMED) {
			dev_info(dev, "config written, Lepton PRU ready\n");
			return IRQ_HANDLED;
		}
		else if (state != STATE_BL_REQUEST_STOP &&
				state != STATE_BL_RUNNING) {
			dev_info(dev, "Unexpected stop request\n");
			bldev->state = STATE_BL_ERROR;
			return IRQ_HANDLED;
		}
                dev_info(dev, "waking up\n");
		bldev->state = STATE_BL_INITIALIZED;
		wake_up_interruptible(&bldev->wait);
	}

	return IRQ_HANDLED;
}

/* Begin the sampling operation [This takes the mutex] */
int beaglelogic_start(struct device *dev) {
    struct beaglelogicdev *bldev = dev_get_drvdata(dev);
    struct capture_context *cxt = bldev->cxt_pru;

    /* This mutex will be locked for the entire duration BeagleLogic runs */
    mutex_lock(&bldev->mutex);

#if USE_PRUS == 1
    beaglelogic_send_cmd(bldev, CMD_START);
#endif
    /* All set now. Start the PRUs and wait for IRQs */
    bldev->state = STATE_BL_RUNNING;
    bldev->lasterror = 0;

    dev_info(dev, "capture started\n");
    return 0;
}

/* Request stop. Stop will effect only after the last buffer is written out */
void beaglelogic_stop(struct device *dev)
{
  struct beaglelogicdev *bldev = dev_get_drvdata(dev);

  if (mutex_is_locked(&bldev->mutex)) {
    if (bldev->state == STATE_BL_RUNNING)
    {

#if USE_PRUS == 1

      beaglelogic_request_stop(bldev);
      bldev->state = STATE_BL_REQUEST_STOP;

      /* Wait for the PRU to signal completion */
      wait_event_interruptible(bldev->wait,
                               bldev->state == STATE_BL_INITIALIZED);

#endif

    }
    /* Release */
    mutex_unlock(&bldev->mutex);

    dev_info(dev, "capture session ended\n");
  }
}

/* fops */
static int beaglelogic_f_open(struct inode *inode, struct file *filp)
{
	struct logic_buffer_reader *reader;
	struct beaglelogicdev *bldev = to_beaglelogicdev(filp->private_data);
	struct device *dev = bldev->miscdev.this_device;
    struct capture_context *cxt = bldev->cxt_pru;

	reader = devm_kzalloc(dev, sizeof(*reader), GFP_KERNEL);
	reader->bldev = bldev;

	filp->private_data = reader;

	cxt->list_start = cxt->list_end = 0;

	return 0;
}

/* Reads index of ready buffer . */
ssize_t beaglelogic_f_read (struct file *filp, char __user *buf,
                          size_t sz, loff_t *offset)
{
	struct logic_buffer_reader *reader = filp->private_data;
	struct beaglelogicdev *bldev = reader->bldev;
	struct device *dev = bldev->miscdev.this_device;
	volatile struct capture_context *cxt = bldev->cxt_pru;
	uint8_t nn;

	if (bldev->state == STATE_BL_ERROR)
		return -EIO;

	if (bldev->state != STATE_BL_RUNNING) {
		/* Start the capture */
		if (beaglelogic_start(dev))
			return -ENOEXEC;
	}

	if (!LIST_IS_FULL(cxt->list_start,cxt->list_end)) {
		nn = LIST_COUNTER_PSY(cxt->list_end);
		if (copy_to_user(buf, &nn, 1))
			return -EFAULT;
		return 1;
	}

	// no buffers ready
	if (filp->f_flags & O_NONBLOCK) {
		return 0;
	}

	if (wait_event_interruptible(bldev->wait,
			!LIST_IS_FULL(cxt->list_start,cxt->list_end) || bldev->state != STATE_BL_RUNNING))
		return -ERESTARTSYS;

	if (bldev->state != STATE_BL_RUNNING) {
        // PRUSS stopped to stream data for some reason
        dev_info(dev,"PRUSS stopped stream, state=%d, state_run=%d\n",bldev->state,cxt->state_run);
        return -EIO;
	}

	nn = LIST_COUNTER_PSY(cxt->list_end);
	if (copy_to_user(buf, &nn, 1))
		return -EFAULT;

	return 1;
}

ssize_t beaglelogic_f_write (struct file *filp, const char __user *buf,
                          size_t sz, loff_t *offset)
{
	struct logic_buffer_reader *reader = filp->private_data;
	struct beaglelogicdev *bldev = reader->bldev;
	struct device *dev = bldev->miscdev.this_device;
	uint8_t nn;

	if (bldev->state == STATE_BL_ERROR)
		return -EIO;

	if (bldev->state != STATE_BL_RUNNING) {
		return -EAGAIN;
	}

	if (copy_from_user(&nn, buf, 1))
		return -EFAULT;

//	dev_info(dev,"beaglelogic_f_write: nn=%d",nn);

	LIST_COUNTER_INC2(bldev->cxt_pru->list_end,nn);

	return 1;
}

/* Map the PRU buffers to user space [cache coherency managed by driver] */
int beaglelogic_f_mmap(struct file *filp, struct vm_area_struct *vma)
{
	int ret, nn;
	struct logic_buffer_reader *reader = filp->private_data;
	struct beaglelogicdev *bldev = reader->bldev;
	struct device *dev = bldev->miscdev.this_device;

	unsigned long addr = vma->vm_start;
	int size = vma->vm_end-addr;

	nn = (vma->vm_pgoff << PAGE_SHIFT)/sizeof(leptonpru_mmap);

	dev_info(dev,"beaglelogic_f_mmap vm_start=%lx, size=%d, off=%ld, framebuf=%d\n",
			addr,size,vma->vm_pgoff,nn);

	ret = remap_pfn_range(vma, addr,
			(bldev->buffers[nn].phys_addr) >> PAGE_SHIFT,
			sizeof(leptonpru_mmap),
			vma->vm_page_prot);

	if (ret)
		return -EINVAL;

	return 0;
}

/* Poll the file descriptor */
unsigned int beaglelogic_f_poll(struct file *filp,
		struct poll_table_struct *tbl)
{
	struct logic_buffer_reader *reader = filp->private_data;
	struct beaglelogicdev *bldev = reader->bldev;

	/* Raise an error if polled without starting first */
	if (bldev->state != STATE_BL_RUNNING)
		return -ENOEXEC;

	if(!LIST_IS_FULL(bldev->cxt_pru->list_start,bldev->cxt_pru->list_end)) {
		return (POLLIN | POLLRDNORM);
	}

	poll_wait(filp, &bldev->wait, tbl);

	return 0;
}

/* Device file close handler */
static int beaglelogic_f_release(struct inode *inode, struct file *filp)
{
	struct logic_buffer_reader *reader = filp->private_data;
	struct beaglelogicdev *bldev = reader->bldev;
	struct device *dev = bldev->miscdev.this_device;

	/* Stop & Release */
	beaglelogic_stop(dev);
	devm_kfree(dev, reader);

	return 0;
}

/* File operations struct */
static const struct file_operations pru_beaglelogic_fops = {
	.owner = THIS_MODULE,
	.open = beaglelogic_f_open,
	.read = beaglelogic_f_read,
	.write = beaglelogic_f_write,
	.mmap = beaglelogic_f_mmap,
	.poll = beaglelogic_f_poll,
	.release = beaglelogic_f_release,
};
/* fops */

/* begin sysfs attrs */
static ssize_t bl_bufunitsize_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n", sizeof(leptonpru_mmap));
}

static ssize_t bl_memalloc_show(struct device *dev,
        struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%d\n",
			FRAMES_NUMBER * sizeof(leptonpru_mmap));
}

static ssize_t bl_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct beaglelogicdev *bldev = dev_get_drvdata(dev);
        struct capture_context *cxt = bldev->cxt_pru;

	return scnprintf(buf, PAGE_SIZE,
		"state: %d, state_run: %d, queue:%d (%d-%d), frames received: %u, dropped: %u, unexpected CS: %u, debug: %u\n",
		bldev->state, cxt->state_run,
		LIST_SIZE(cxt->list_start,cxt->list_end),
		cxt->list_start, cxt->list_end,
		cxt->frames_received, cxt->frames_dropped, cxt->unexpected_cs,
		cxt->debug);
}

static ssize_t bl_state_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct beaglelogicdev *bldev = dev_get_drvdata(dev);
	uint32_t val;

	if (kstrtouint(buf, 10, &val))
		return -EINVAL;

	dev_info(dev, "simulator command: %d\n",val);

	if (val == 1)
		beaglelogic_start(dev);
	else if (val == 0)
		beaglelogic_stop(dev);
	else
		return -EINVAL;

	return count;
}

static ssize_t bl_buffers_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct beaglelogicdev *bldev = dev_get_drvdata(dev);
	struct capture_context *cxt = bldev->cxt_pru;
	int i, c, cnt;

	for (i = 0, cnt = 0; i < FRAMES_NUMBER; i++) {
		c = scnprintf(buf, PAGE_SIZE, "%c%c %08x, %u\n",
			i == LIST_COUNTER_PSY(cxt->list_start) ? 's' : ' ',
			i == LIST_COUNTER_PSY(cxt->list_end) ? 'e' : ' ',
			(uint32_t)bldev->buffers[i].phys_addr,
			sizeof(leptonpru_mmap));
		cnt += c;
		buf += c;
	}
	c = scnprintf(buf, PAGE_SIZE, "size=%d\n",  LIST_SIZE(cxt->list_start,cxt->list_end));
	cnt += c;

	return cnt;
}

static ssize_t bl_lasterror_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct beaglelogicdev *bldev = dev_get_drvdata(dev);

	wait_event_interruptible(bldev->wait,
			bldev->state != STATE_BL_RUNNING);


	return scnprintf(buf, PAGE_SIZE, "%d\n", bldev->lasterror);
}

static DEVICE_ATTR(bufunitsize, S_IRUGO,
		bl_bufunitsize_show, NULL);

static DEVICE_ATTR(memalloc, S_IRUGO,
		bl_memalloc_show, NULL);

static DEVICE_ATTR(state, S_IWUSR | S_IRUGO,
		bl_state_show, bl_state_store);

static DEVICE_ATTR(buffers, S_IRUGO,
		bl_buffers_show, NULL);

static DEVICE_ATTR(lasterror, S_IRUGO,
		bl_lasterror_show, NULL);

static struct attribute *beaglelogic_attributes[] = {
	&dev_attr_bufunitsize.attr,
	&dev_attr_memalloc.attr,
	&dev_attr_state.attr,
	&dev_attr_buffers.attr,
	&dev_attr_lasterror.attr,
	NULL
};

static struct attribute_group beaglelogic_attr_group = {
	.attrs = beaglelogic_attributes
};
/* end sysfs attrs */

static const struct of_device_id beaglelogic_dt_ids[];

static int beaglelogic_probe(struct platform_device *pdev)
{
	struct beaglelogicdev *bldev;
	struct device *dev;
	struct device_node *node = pdev->dev.of_node;
	const struct of_device_id *match;
	int ret;

	printk("Lepton PRU: probe\n");

	if (!node)
		return -ENODEV; /* No support for non-DT platforms */

	match = of_match_device(beaglelogic_dt_ids, &pdev->dev);
	if (!match) {
		printk("LeptonPRU: DT node not found\n");
		return -ENODEV;
	}

	/* Allocate memory for our private structure */
	bldev = kzalloc(sizeof(*bldev), GFP_KERNEL);
	if (!bldev) {
		printk("LeptonPRU: can't allocate mem\n");
		ret = -1;
		goto fail;
	}

	bldev->fw_data = match->data;
	bldev->miscdev.fops = &pru_beaglelogic_fops;
	bldev->miscdev.minor = MISC_DYNAMIC_MINOR;
	bldev->miscdev.mode = S_IRUGO | S_IWUGO;
	bldev->miscdev.name = "leptonpru";

	/* Link the platform device data to our private structure */
	bldev->p_dev = &pdev->dev;
	dev_set_drvdata(bldev->p_dev, bldev);

	/* Get a handle to the PRUSS structures */
	dev = &pdev->dev;

	printk("LeptonPRU: getting rproc for prus\n");
	bldev->pru0 = pru_rproc_get(node, 0);
	if (IS_ERR(bldev->pru0)) {
		ret = PTR_ERR(bldev->pru0);
		printk("LeptonPRU: remoteproc for PRU0 fails, err=%d\n",ret);
		bldev->pru0 = NULL;
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "unable to get PRU0: %d\n", ret);
		goto fail_free;
	}

	bldev->pruss = pruss_get(bldev->pru0);
	if (IS_ERR(bldev->pruss)) {
		ret = PTR_ERR(bldev->pruss);
		if (ret != -EPROBE_DEFER)
			dev_err(dev, "Unable to get pruss handle.\n");
		goto fail_put_pru1;
	}

	ret = pruss_request_mem_region(bldev->pruss, PRUSS_MEM_DRAM0,
		&bldev->pru0sram);
	if (ret) {
		dev_err(dev, "Unable to get PRUSS RAM.\n");
		goto fail_pruss_put;
	}

	/* Get interrupts and install interrupt handlers */
	bldev->from_bl_irq_1 = platform_get_irq_byname(pdev, "from_lepton_1");
	if (bldev->from_bl_irq_1 <= 0) {
		ret = bldev->from_bl_irq_1;
		if (ret == -EPROBE_DEFER)
			goto fail_putmem;
	}
	bldev->from_bl_irq_2 = platform_get_irq_byname(pdev, "from_lepton_2");
	if (bldev->from_bl_irq_2 <= 0) {
		ret = bldev->from_bl_irq_2;
		if (ret == -EPROBE_DEFER)
			goto fail_putmem;
	}
	bldev->to_bl_irq = platform_get_irq_byname(pdev, "to_lepton");
	if (bldev->to_bl_irq<= 0) {
		ret = bldev->to_bl_irq;
		if (ret == -EPROBE_DEFER)
			goto fail_putmem;
	}

	ret = request_irq(bldev->from_bl_irq_1, beaglelogic_serve_irq,
		IRQF_ONESHOT, dev_name(dev), bldev);
	if (ret) goto fail_putmem;

	ret = request_irq(bldev->from_bl_irq_2, beaglelogic_serve_irq,
		IRQF_ONESHOT, dev_name(dev), bldev);
	if (ret) goto fail_free_irq1;

	ret = rproc_boot(bldev->pru0);
	if (ret) {
		dev_err(dev, "Failed to boot PRU0: %d\n", ret);
		goto fail_free_irqs;
	}

	printk("Lepton PRU loaded and initializing\n");

	/* Once done, register our misc device and link our private data */
	ret = misc_register(&bldev->miscdev);
	if (ret)
		goto fail_shutdown_prus;
	dev = bldev->miscdev.this_device;
	dev_set_drvdata(dev, bldev);

	/* Set up locks */
	mutex_init(&bldev->mutex);
	init_waitqueue_head(&bldev->wait);

	/* Power on in disabled state */
	bldev->state = STATE_BL_DISABLED;

	/* Capture context structure is at location 0000h in PRU0 SRAM */
	bldev->cxt_pru = bldev->pru0sram.va + 0;

	if (bldev->cxt_pru->magic == FW_MAGIC)
		dev_info(dev, "Valid PRU capture context structure "\
				"found at offset %04X\n", 0);
	else {
		dev_err(dev, "Firmware error!\n");
		goto faildereg;
	}

//	ret = of_property_read_u32(node,"pin-GPS-100HZ",&bldev->cxt_pru->pin_gps_100hz);
//	if (ret) {
//		dev_err(&pdev->dev, "No pin-GPS-100HZ in DT\n");
//        goto faildereg;
//	}
//
    /* We got configuration from PRUs, now mark device init'd */
    bldev->state = STATE_BL_INITIALIZED;

    beaglelogic_memalloc(dev);
    beaglelogic_map_and_submit_all_buffers(dev);

	/* Display our init'ed state */
	dev_info(dev, "Lepton PRU initialized OK\n");

	/* Once done, create device files */
	ret = sysfs_create_group(&dev->kobj, &beaglelogic_attr_group);
	if (ret) {
		dev_err(dev, "Registration failed.\n");
		goto faildereg;
	}

	return 0;

faildereg:
	misc_deregister(&bldev->miscdev);
fail_shutdown_prus:
//	rproc_shutdown(bldev->pru1);
fail_shutdown_pru0:
	rproc_shutdown(bldev->pru0);
fail_free_irqs:
	free_irq(bldev->from_bl_irq_2, bldev);
fail_free_irq1:
	free_irq(bldev->from_bl_irq_1, bldev);
fail_putmem:
	if (bldev->pru0sram.va)
	    pruss_release_mem_region(bldev->pruss, &bldev->pru0sram);
fail_pruss_put:
	pruss_put(bldev->pruss);
fail_put_pru1:
//	pru_rproc_put(bldev->pru1);
fail_put_pru0:
	pru_rproc_put(bldev->pru0);
fail_free:
	kfree(bldev);
fail:
	return ret;
}

static int beaglelogic_remove(struct platform_device *pdev)
{
	struct beaglelogicdev *bldev = platform_get_drvdata(pdev);
	struct device *dev = bldev->miscdev.this_device;

	/* Remove the sysfs attributes */
	sysfs_remove_group(&dev->kobj, &beaglelogic_attr_group);

	/* Deregister the misc device */
	misc_deregister(&bldev->miscdev);

	/* Shutdown the PRUs */
//	rproc_shutdown(bldev->pru1);
	rproc_shutdown(bldev->pru0);

	/* Free IRQs */
	free_irq(bldev->from_bl_irq_2, bldev);
	free_irq(bldev->from_bl_irq_1, bldev);

	/* Release handles to PRUSS memory regions */
	pruss_release_mem_region(bldev->pruss, &bldev->pru0sram);
//	pru_rproc_put(bldev->pru1);
	pru_rproc_put(bldev->pru0);
	pruss_put(bldev->pruss);

	/* Free all buffers */
	beaglelogic_memfree(dev);

	/* Free up memory */
	kfree(bldev);

	/* Print a log message to announce unloading */
	printk("Lepton PRU unloaded\n");
	return 0;
}

static const struct of_device_id beaglelogic_dt_ids[] = {
	{ .compatible = "take4aps,leptonpru", },
	{ /* sentinel */ },
};
MODULE_DEVICE_TABLE(of, beaglelogic_dt_ids);

static struct platform_driver beaglelogic_driver = {
	.driver = {
		.name = DRV_NAME,
		.owner = THIS_MODULE,
		.of_match_table = beaglelogic_dt_ids,
	},
	.probe = beaglelogic_probe,
	.remove = beaglelogic_remove,
};

module_platform_driver(beaglelogic_driver);

MODULE_AUTHOR("Mikhail Zemlyanukha <gmixaz@gmail.com>");
MODULE_DESCRIPTION("PRU GPIOs streamer");
MODULE_LICENSE("GPL");
MODULE_VERSION(DRV_VERSION);
