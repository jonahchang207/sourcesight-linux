/*
 * person_mouse.c — Kernel-level virtual mouse for SourceSight.
 *
 * Vendored from the CS2AiAimbot/Nod project (GPL-2.0) with the device
 * interface kept byte-for-byte identical so the user-space controller is
 * interchangeable between the two projects.
 *
 * Creates /dev/person-mouse, which accepts relative cursor-position writes
 * and injects them into the kernel's input subsystem via uinput.  This
 * bypasses the compositor (X11/Wayland) entirely — the kernel places the
 * event on every input-device fd, so it works under any display server.
 *
 * Usage
 * -----
 *   1.  insmod person_mouse.ko
 *       → /dev/person-mouse and /sys/class/input/inputN
 *
 *   2.  Set the virtual-screen bounds (ioctl PERSON_MOUSE_SET_SIZE):
 *         struct pm_size { int w, h; };
 *         ioctl(fd, 0x4008AA01, &(struct pm_size){1920, 1080});
 *
 *   3.  Move the cursor (write "dx dy\n") or left-click (write "click\n"):
 *         dprintf(fd, "%d %d\n", dx, dy);  // relative movement
 *         dprintf(fd, "click\n");           // left-button press/release
 *
 *   4.  rmmod person_mouse cleans up the device.
 *
 * License: GPL-2.0
 */

#include <linux/cdev.h>
#include <linux/atomic.h>
#include <linux/device.h>
#include <linux/fs.h>
#include <linux/input.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/string.h>


#define DEVICE_NAME  "person-mouse"
#define CLASS_NAME   "person_mouse"
#define WRITE_BUF_SZ 64
/* Large enough for a 4K desktop correction, bounded against input abuse. */
#define PM_MAX_DELTA 8192

MODULE_LICENSE("GPL");
MODULE_AUTHOR("SourceSight");
MODULE_DESCRIPTION("Virtual mouse with a bounded relative-event interface (vendored from Nod)");
MODULE_VERSION("1.1");

/* ── ioctl definitions ──────────────────────────────────────────────── */

struct pm_size {
	int width;
	int height;
};

#define PM_IOC_MAGIC  0xAA
#define PM_IOC_SETSIZE  _IOW(PM_IOC_MAGIC, 1, struct pm_size)

/* ── per-device state ───────────────────────────────────────────────── */

struct pm_device {
	struct cdev cdev;
	dev_t devt;
	struct class *cls;
	struct device *clsdev;
	struct input_dev *idev;   /* uinput virtual device */
	int width;
	int height;
	atomic_t open_count;
	bool idev_created;
};

static struct pm_device pm;

/* ── helpers ────────────────────────────────────────────────────────── */

static int pm_create_input(struct pm_device *pm)
{
	int ret;

	pm->idev = input_allocate_device();
	if (!pm->idev) {
		pr_err("person_mouse: input_allocate_device failed\n");
		return -ENOMEM;
	}

	pm->idev->name  = "SourceSight Virtual Mouse";
	pm->idev->id.bustype = BUS_VIRTUAL;
	pm->idev->id.vendor   = 0x0001;
	pm->idev->id.product  = 0x0001;
	pm->idev->id.version  = 1;

	/* Absolute position */
	set_bit(EV_ABS, pm->idev->evbit);
	set_bit(ABS_X,  pm->idev->absbit);
	set_bit(ABS_Y,  pm->idev->absbit);

	/* Also report as a mouse (for relative-aware apps) */
	set_bit(EV_REL, pm->idev->evbit);
	set_bit(REL_X,  pm->idev->relbit);
	set_bit(REL_Y,  pm->idev->relbit);

	/* Buttons (left-click support) */
	set_bit(EV_KEY, pm->idev->evbit);
	set_bit(BTN_LEFT, pm->idev->keybit);
	set_bit(BTN_MOUSE, pm->idev->keybit);

	if (pm->width > 0 && pm->height > 0) {
		input_set_abs_params(pm->idev, ABS_X, 0, pm->width,  0, 0);
		input_set_abs_params(pm->idev, ABS_Y, 0, pm->height, 0, 0);
	} else {
		/* Default range; call PM_IOC_SETSIZE to set real bounds */
		input_set_abs_params(pm->idev, ABS_X, 0, 16384, 0, 0);
		input_set_abs_params(pm->idev, ABS_Y, 0, 16384, 0, 0);
	}

	ret = input_register_device(pm->idev);
	if (ret) {
		pr_err("person_mouse: input_register_device failed (%d)\n", ret);
		input_free_device(pm->idev);
		pm->idev = NULL;
		return ret;
	}

	pm->idev_created = true;
	pr_info("person_mouse: input device registered as %s\n", pm->idev->name);
	return 0;
}

/* ── character-device file operations ───────────────────────────────── */

static int pm_open(struct inode *inode, struct file *filp)
{
	int ret;

	/* One trusted controller owns injection at a time. */
	if (atomic_cmpxchg(&pm.open_count, 0, 1) != 0)
		return -EBUSY;

	filp->private_data = &pm;

	if (!pm.idev_created || !pm.idev) {
		ret = pm_create_input(&pm);
		if (ret)
			goto out_release;
	}

	return 0;

out_release:
	atomic_set(&pm.open_count, 0);
	return ret;
}

static int pm_release(struct inode *inode, struct file *filp)
{
	atomic_set(&pm.open_count, 0);
	return 0;
}

static ssize_t pm_write(struct file *filp, const char __user *ubuf,
			size_t len, loff_t *off)
{
	struct pm_device *pm = filp->private_data;
	char kbuf[WRITE_BUF_SZ];
	char *command;
	int dx, dy;
	char trailing;

	if (len == 0)
		return 0;
	if (len >= WRITE_BUF_SZ)
		return -EMSGSIZE;

	if (copy_from_user(kbuf, ubuf, len))
		return -EFAULT;
	kbuf[len] = '\0';
	command = strim(kbuf);

	/* Movement: "dx dy".  A click command emits one left-button
	 * press/release at the cursor's current position.
	 */
	if (!strcmp(command, "click")) {
		if (!pm->idev)
			return -ENODEV;
		input_report_key(pm->idev, BTN_LEFT, 1);
		input_sync(pm->idev);
		input_report_key(pm->idev, BTN_LEFT, 0);
		input_sync(pm->idev);
		return len;
	}

	if (sscanf(command, "%d %d %c", &dx, &dy, &trailing) != 2)
		return -EINVAL;
	if (dx > PM_MAX_DELTA || dx < -PM_MAX_DELTA ||
	    dy > PM_MAX_DELTA || dy < -PM_MAX_DELTA)
		return -ERANGE;

	if (!pm->idev)
		return -ENODEV;
	input_report_rel(pm->idev, REL_X, dx);
	input_report_rel(pm->idev, REL_Y, dy);
	input_sync(pm->idev);

	return len;
}

static long pm_ioctl(struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct pm_device *pm = filp->private_data;

	switch (cmd) {
	case PM_IOC_SETSIZE: {
		struct pm_size sz;
		if (copy_from_user(&sz, (void __user *)arg, sizeof(sz)))
			return -EFAULT;
		if (sz.width <= 0 || sz.height <= 0 ||
		    sz.width > 32767 || sz.height > 32767)
			return -EINVAL;

		pm->width  = sz.width;
		pm->height = sz.height;

		if (pm->idev) {
			input_set_abs_params(pm->idev, ABS_X, 0, pm->width,  0, 0);
			input_set_abs_params(pm->idev, ABS_Y, 0, pm->height, 0, 0);
		}

		pr_info("person_mouse: set screen size %dx%d\n",
			pm->width, pm->height);
		return 0;
	}
	default:
		return -ENOTTY;
	}
}

static const struct file_operations pm_fops = {
	.owner          = THIS_MODULE,
	.open           = pm_open,
	.release        = pm_release,
	.write          = pm_write,
	.unlocked_ioctl = pm_ioctl,
};

/* ── module init / exit ─────────────────────────────────────────────── */

static int __init pm_init(void)
{
	int ret;
	struct device *clsdev;

	/* Allocate device number */
	ret = alloc_chrdev_region(&pm.devt, 0, 1, DEVICE_NAME);
	if (ret < 0) {
		pr_err("person_mouse: alloc_chrdev_region failed (%d)\n", ret);
		return ret;
	}

	cdev_init(&pm.cdev, &pm_fops);
	pm.cdev.owner = THIS_MODULE;

	ret = cdev_add(&pm.cdev, pm.devt, 1);
	if (ret) {
		pr_err("person_mouse: cdev_add failed (%d)\n", ret);
		goto err_unreg_region;
	}

	/* Create device class and node in /dev */
	pm.cls = class_create(CLASS_NAME);
	if (IS_ERR(pm.cls)) {
		ret = PTR_ERR(pm.cls);
		pr_err("person_mouse: class_create failed (%d)\n", ret);
		goto err_cdev_del;
	}

	clsdev = device_create(pm.cls, NULL, pm.devt, NULL, DEVICE_NAME);
	if (IS_ERR(clsdev)) {
		ret = PTR_ERR(clsdev);
		pr_err("person_mouse: device_create failed (%d)\n", ret);
		class_destroy(pm.cls);
		goto err_cdev_del;
	}

	pm.clsdev = clsdev;
	pm.width  = 0;
	pm.height = 0;
	pm.idev   = NULL;
	atomic_set(&pm.open_count, 0);
	pm.idev_created = false;

	pr_info("person_mouse: device /dev/%s ready\n", DEVICE_NAME);
	return 0;

err_cdev_del:
	cdev_del(&pm.cdev);
err_unreg_region:
	unregister_chrdev_region(pm.devt, 1);
	return ret;
}

static void __exit pm_exit(void)
{
	if (pm.idev) {
		if (pm.idev_created)
			input_unregister_device(pm.idev);
		else
			input_free_device(pm.idev);
		pm.idev = NULL;
	}

	if (pm.clsdev) {
		device_destroy(pm.cls, pm.devt);
		pm.clsdev = NULL;
	}
	if (pm.cls) {
		class_destroy(pm.cls);
		pm.cls = NULL;
	}

	cdev_del(&pm.cdev);
	unregister_chrdev_region(pm.devt, 1);

	pr_info("person_mouse: module unloaded\n");
}

module_init(pm_init);
module_exit(pm_exit);
