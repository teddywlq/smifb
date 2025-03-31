/*
 * Copyright 2016 SiliconMotion Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 */
#include <linux/module.h>
#include <linux/console.h>
#include <drm/drmP.h>
#include <drm/drm_crtc_helper.h>
#include "smi_drv.h"
#if LINUX_VERSION_CODE >= KERNEL_VERSION(5, 1, 0)
#include <drm/drm_probe_helper.h>
#endif

#include "hw750.h"
#include "hw768.h"
#include "hw770.h"


int smi_modeset = -1;
int smi_indent = 0;
int smi_bpp = 32;
int force_connect = 0;
int smi_pat = 0xff;
int lvds_channel = 0;
int usb_host = 0;
int audio_en = 0;
int fixed_width = 0;
int fixed_height = 0;
int hwi2c_en = 0;
int swcur_en = 0;
int edid_mode = 1;
int smi_debug = 0;
int lcd_scale = 0;
int pwm_ctrl = 0;
int clk_phase = -1;



extern void hw750_suspend(struct smi_750_register * pSave);
extern void hw750_resume(struct smi_750_register * pSave);


module_param(smi_pat,int, S_IWUSR | S_IRUSR);


MODULE_PARM_DESC(modeset, "Disable/Enable modesetting");
module_param_named(modeset, smi_modeset, int, 0400);
MODULE_PARM_DESC(bpp, "Max bits-per-pixel (default:32)");
module_param_named(bpp, smi_bpp, int, 0400);
MODULE_PARM_DESC(nopnp, "Force conncet to the monitor without monitor EDID (default:0) bit0:DVI,bit1:VGA,bit2:HDMI SM770: bit3-5 HDMI0-2, bit6-7 DP0-1");
module_param_named(nopnp, force_connect, int, 0400);
MODULE_PARM_DESC(lvds, "LVDS Channel, 0=disable 1=single channel, 2=dual channel (default:0)");
module_param_named(lvds, lvds_channel, int, 0400);
MODULE_PARM_DESC(width, "Fixed mode width for LVDS or nopnp (default:0)");
module_param_named(width, fixed_width, int, 0400);
MODULE_PARM_DESC(height, "Fixed mode height for LVDS or nopnp (default:0)");
module_param_named(height, fixed_height, int, 0400);
MODULE_PARM_DESC(usbhost, "SM768 USB EHCI Host Disable/Enable(default:0)");
module_param_named(usbhost, usb_host, int, 0400);
MODULE_PARM_DESC(audio, "SM768/SM770 Audio, 0=diable 1=use UDA1345 Codec, 2=use WM8978 Codec(default:0), SM770 only support HDMI/DP Audio");
module_param_named(audio, audio_en, int, 0400);
MODULE_PARM_DESC(hwi2c, "HW I2C for EDID reading for SM750/SM768, 0=SW I2C 1=HW I2C(default:0)");
module_param_named(hwi2c, hwi2c_en, int, 0400);
MODULE_PARM_DESC(swcur, "Use Software cursor, 0=HW Cursor 1=SW Cursor(default:0)");
module_param_named(swcur, swcur_en, int, 0400);
MODULE_PARM_DESC(edidmode, "Use Monitor EDID mode timing, 0 = Driver build-in mode timing 1 = Monitor EDID mode timing(default:1)");
module_param_named(edidmode, edid_mode, int, 0400);
MODULE_PARM_DESC(debug, "Driver debug log enable, 0 = disable 1 = enable (default:0)");
module_param_named(debug, smi_debug, int, 0400);
MODULE_PARM_DESC(lcdscale, "LCD(LVDS) scale  enable, 0 = disable 1 = enable (default:0)");
module_param_named(lcdscale, lcd_scale, int, 0400);
MODULE_PARM_DESC(pwm, "PWM Value, 0 = disable (default:0) bit 0-3: PWM 0/1/2 bit4-7: PWM Divider bit8-19: PWM Low Counter bit20-31: PWM High Counter");
module_param_named(pwm, pwm_ctrl, int, 0400);
MODULE_PARM_DESC(clkphase, "Panel Mode Clock phase, -1 = Use Mode table (Default)  0 = Negative 1 = Postive");
module_param_named(clkphase, clk_phase, int, 0400);



/*
 * This is the generic driver code. This binds the driver to the drm core,
 * which then performs further device association and calls our graphics init
 * functions
 */
#define PCI_VENDOR_ID_SMI 	0x126f
#define PCI_DEVID_SM750	0x0750
#define PCI_DEVID_SM768	0x0768
#define PCI_DEVID_SM770 0x0770

static struct drm_driver driver;

/* only bind to the smi chip in qemu */
static const struct pci_device_id pciidlist[] = {
	{ PCI_VENDOR_ID_SMI, PCI_DEVID_SM750, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_SMI, PCI_DEVID_SM768, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{ PCI_VENDOR_ID_SMI, PCI_DEVID_SM770, PCI_ANY_ID, PCI_ANY_ID, 0, 0, 0 },
	{
		0,
	}
};


static int smi_kick_out_firmware_fb(struct pci_dev *pdev)
{
	struct apertures_struct *ap;
	bool primary = false;

	ap = alloc_apertures(1);
	if (!ap)
		return -ENOMEM;

	ap->ranges[0].base = pci_resource_start(pdev, 0);
	ap->ranges[0].size = pci_resource_len(pdev, 0);

#ifdef CONFIG_X86
	primary = pdev->resource[PCI_ROM_RESOURCE].flags & IORESOURCE_ROM_SHADOW;
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0))
	drm_fb_helper_remove_conflicting_framebuffers(ap, "smidrmfb", primary);
#else
	remove_conflicting_framebuffers(ap, "smidrmfb", primary);
#endif


	kfree(ap);

	return 0;
}

static void claim(void)
{
	printk("+-------------SMI Driver Information------------+\n");
	printk("Release type: " RELEASE_TYPE "\n");
	printk("Driver version: v" _version_ "\n");
	printk("Support products: " SUPPORT_CHIP "\n");
	printk("+-----------------------------------------------+\n");
}


static int smi_pci_probe(struct pci_dev *pdev,
			    const struct pci_device_id *ent)
{
	int ret;

	ret = smi_kick_out_firmware_fb(pdev);
	if (ret)
		return ret;

	claim();
	if (ent->vendor != PCI_VENDOR_ID_SMI &&
	    !(ent->device == PCI_DEVID_LYNX_EXP || ent->device == PCI_DEVID_SM768 || ent->device == PCI_DEVID_SM770)) {
		return -ENODEV;
	}

	if (ent->vendor == PCI_VENDOR_ID_SMI && (ent->device == PCI_DEVID_SM768 || ent->device == PCI_DEVID_SM770)) {
		
		if (lvds_channel != 0) {
			dbg_msg("LVDS channel set to %d\n", lvds_channel);
		}
	}

	
	return drm_get_pci_dev(pdev, ent, &driver);
}

static void smi_pci_remove(struct pci_dev *pdev)
{
	struct drm_device *dev = pci_get_drvdata(pdev);

	drm_put_dev(dev);
}



static int smi_drm_freeze(struct drm_device *dev)
{
	struct smi_device *sdev = dev->dev_private;

	ENTER();
	
	drm_kms_helper_poll_disable(dev);

	pci_save_state(dev->pdev);

	if (sdev->mode_info.gfbdev) {
		console_lock();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0))
		drm_fb_helper_set_suspend(&sdev->mode_info.gfbdev->helper, 1);
	
#else
		fb_set_suspend(sdev->mode_info.gfbdev->helper.fbdev, 1);

#endif
		console_unlock();
	}

	if (sdev->specId == SPC_SM750){
         hw750_suspend(sdev->regsave);
	}else if(sdev->specId == SPC_SM768){
#ifndef NO_AUDIO
		 if(audio_en)
			 smi_audio_suspend(sdev);
#endif
         hw768_suspend(sdev->regsave_768);
    }else if(sdev->specId == SPC_SM770){
#ifndef NO_AUDIO
		if(audio_en)
			 smi_audio_suspend(sdev);
#endif
		hw770_suspend(sdev->regsave_770);
    }
	LEAVE(0);

}

static int smi_drm_thaw(struct drm_device *dev)
{
	struct smi_device *sdev = dev->dev_private;

	ENTER();

	drm_mode_config_reset(dev);
	drm_helper_resume_force_mode(dev);

	if (sdev->mode_info.gfbdev) {
		console_lock();
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,3,0))
		drm_fb_helper_set_suspend(&sdev->mode_info.gfbdev->helper, 0);
#else
		fb_set_suspend(sdev->mode_info.gfbdev->helper.fbdev, 0);
#endif
		smi_fb_zfill(dev, sdev->mode_info.gfbdev);

		console_unlock();
	}

	
	if(sdev->specId == SPC_SM750){

			hw750_resume(sdev->regsave);
	}else if(sdev->specId == SPC_SM768){

			hw768_resume(sdev->regsave_768);
#ifndef NO_AUDIO
			if(audio_en)
			smi_audio_resume(sdev);
#endif
		if(sdev->m_connector & USE_DVI){
			if (lvds_channel == 1){
				hw768_enable_lvds(1);
				DisableDoublePixel(0);
			}
			else if (lvds_channel == 2) {
				hw768_enable_lvds(2);
				EnableDoublePixel(0);
			}
		}
	}else if(sdev->specId == SPC_SM770){
				hw770_resume(sdev->regsave_770);
#ifndef NO_AUDIO
				if(audio_en)
					smi_audio_resume(sdev);
#endif
			
	}
	LEAVE(0);
}


static int smi_drm_resume(struct drm_device *dev)
{
	struct smi_device *UNUSED(sdev) = dev->dev_private;
	int ret;

	ENTER();

	if (pci_enable_device(dev->pdev))
		return -EIO;

	ret = smi_drm_thaw(dev);
	if (ret)
		return ret;

	drm_kms_helper_poll_enable(dev);

	LEAVE(0);
}


static int smi_pm_suspend(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *ddev = pci_get_drvdata(pdev);
	int error;

	error = smi_drm_freeze(ddev);
	if (error)
		return error;

	pci_disable_device(pdev);
	pci_set_power_state(pdev, PCI_D3hot);
	return 0;

}


static int smi_pm_resume(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *ddev = pci_get_drvdata(pdev);
	return smi_drm_resume(ddev);
}




static int smi_pm_freeze(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *ddev = pci_get_drvdata(pdev);

	if (!ddev || !ddev->dev_private)
		return -ENODEV;
	return smi_drm_freeze(ddev);

}


static int smi_pm_thaw(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *ddev = pci_get_drvdata(pdev);
	return smi_drm_thaw(ddev);

}


static int smi_pm_poweroff(struct device *dev)
{
	struct pci_dev *pdev = to_pci_dev(dev);
	struct drm_device *ddev = pci_get_drvdata(pdev);

	return smi_drm_freeze(ddev);


}

static int smi_enable_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct smi_device *sdev = dev->dev_private;
	
	if (sdev->specId == SPC_SM750) {
		hw750_en_dis_interrupt(1, pipe);
	} else if (sdev->specId == SPC_SM768) {
		hw768_en_dis_interrupt(1, pipe);
	}else if (sdev->specId == SPC_SM770) {
		hw770_en_dis_interrupt(1, pipe);
	}
	return 0;
}


static void smi_disable_vblank(struct drm_device *dev, unsigned int pipe)
{
	struct smi_device *sdev = dev->dev_private;
	
	if (sdev->specId == SPC_SM750) {
		hw750_en_dis_interrupt(0, pipe);
	} else if (sdev->specId == SPC_SM768) {
		hw768_en_dis_interrupt(0, pipe);
	} else if (sdev->specId == SPC_SM770) {
		hw770_en_dis_interrupt(0, pipe);
	}
}


static void smi_irq_preinstall(struct drm_device *dev)
{
	//To Do....
	/* Disable *all* interrupts */

	/* Clear bits if they're already high */

}

static int smi_irq_postinstall(struct drm_device *dev)
{
	return 0;
}

static void smi_irq_uninstall(struct drm_device *dev)
{
	struct smi_device *sdev = dev->dev_private;

	/* Disable *all* interrupts */
	if (sdev->specId == SPC_SM750) {
		ddk750_disable_IntMask();
	} else if (sdev->specId == SPC_SM768) {
		ddk768_disable_IntMask();
	} else if (sdev->specId == SPC_SM770) {
		ddk770_disable_IntMask();
	}

}


irqreturn_t smi_drm_interrupt(int irq, void *arg)
{
	struct drm_device *dev = (struct drm_device *)arg;
	
	int handled = 0;
	struct smi_device *sdev = dev->dev_private;
	
	if (sdev->specId == SPC_SM750) {
		if (hw750_check_vsync_interrupt(0)) {
	        /* Clear the panel VSync Interrupt */	
			drm_handle_vblank(dev, 0);		
			handled = 1;
			hw750_clear_vsync_interrupt(0);
	    }   
		if (hw750_check_vsync_interrupt(1)) {			
			drm_handle_vblank(dev, 1);
			handled = 1;
			hw750_clear_vsync_interrupt(1);
		}
	} else if (sdev->specId == SPC_SM768) {
		if (hw768_check_vsync_interrupt(0)) {
			/* Clear the panel VSync Interrupt */
			drm_handle_vblank(dev, 0);
			handled = 1;
			hw768_clear_vsync_interrupt(0);
		}	
		if (hw768_check_vsync_interrupt(1)) {		
			drm_handle_vblank(dev, 1);
			handled = 1;
			hw768_clear_vsync_interrupt(1);
		}
	} else if(sdev->specId == SPC_SM770){
		if (hw770_check_vsync_interrupt(0)) {
			/* Clear the panel VSync Interrupt */
			drm_handle_vblank(dev, 0);
			handled = 1;
			hw770_clear_vsync_interrupt(0);
		}
		if (hw770_check_vsync_interrupt(1)) {
			drm_handle_vblank(dev, 1);
			handled = 1;
			hw770_clear_vsync_interrupt(1);
		}
	}
	
	if (handled)
		return IRQ_HANDLED;
	return IRQ_NONE;
}


irqreturn_t smi_hdmi0_hardirq(int irq, void *dev_id)
{
	int ret;

	ret = hw770_check_pnp_interrupt(0);
	if (ret)
	{
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

irqreturn_t smi_hdmi1_hardirq(int irq, void *dev_id)
{
	int ret;

	ret = hw770_check_pnp_interrupt(1);
	if (ret)
	{
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}

irqreturn_t smi_hdmi2_hardirq(int irq, void *dev_id)
{
	int ret;

	ret = hw770_check_pnp_interrupt(2);
	if (ret)
	{
		return IRQ_WAKE_THREAD;
	}

	return IRQ_NONE;
}


irqreturn_t smi_hdmi0_pnp_handler(int irq, void *dev_id)
{
	struct drm_display_mode *mode;
	struct smi_device *sdev;
	struct drm_device *dev;
	logicalMode_t logicalMode;
	unsigned long refresh_rate;
	struct drm_crtc *crtc0;
	struct smi_770_fb_info fb_info = {0};
	int monitor_status = 0;
	int ret = 0;

	dev = dev_id;
	sdev = dev->dev_private;
	monitor_status = hw770_hdmi_detect(0);
	if(!monitor_status)
		return IRQ_HANDLED;

	crtc0 = sdev->smi_enc_tab[2]->crtc;
	if (crtc0) {
		printk("CRTC0 found: %p\n", crtc0);
	} else {
		printk("CRTC0 not found\n");
		goto error;
	}

	mode = &crtc0->mode;

	if(!mode)
		goto error;

	refresh_rate = drm_mode_vrefresh(mode);
	hw770_get_current_fb_info(0,&fb_info);

	logicalMode.valid_edid = false;
	if (sdev->hdmi0_edid && drm_edid_header_is_valid((u8 *)sdev->hdmi0_edid) == 8)
		logicalMode.valid_edid = true;

	logicalMode.x = mode->hdisplay;
	logicalMode.y = mode->vdisplay;
	logicalMode.bpp = smi_bpp;
	logicalMode.hz = refresh_rate;
	logicalMode.pitch = 0;
	logicalMode.dispCtrl = 0;

	if (monitor_status)
	{
		printk("HDMI0: Monitor status is $%d\n", monitor_status);
		hw770_setMode(&logicalMode, *mode);
		ret = hw770_set_hdmi_mode(&logicalMode, *mode, sdev->is_hdmi[0], INDEX_HDMI0);
		if (ret != 0)
		{
			printk("HDMI Mode not supported!\n");
			goto error;
		}
		hw770_set_current_pitch(INDEX_HDMI0,&fb_info);
	}
	return IRQ_HANDLED;
error:
	return IRQ_HANDLED;

}

irqreturn_t smi_hdmi1_pnp_handler(int irq, void *dev_id)
{
	struct drm_display_mode *mode;
	struct smi_device *sdev;
	struct drm_device *dev;
	logicalMode_t logicalMode;
	unsigned long refresh_rate;
	struct drm_crtc *crtc1;
	struct smi_770_fb_info fb_info = {0};

	int monitor_status = 0;
	int ret = 0;

	dev = dev_id;
	sdev = dev->dev_private;
	monitor_status = hw770_hdmi_detect(1);
	if(!monitor_status)
		return IRQ_HANDLED;

	crtc1 = sdev->smi_enc_tab[3]->crtc;
	if (crtc1) {
		printk("CRTC1 found: %p\n", crtc1);
	} else {
		printk("CRTC1 not found\n");
		goto error;
	}

	mode = &crtc1->mode;


	if(!mode)
		goto error;

	refresh_rate = drm_mode_vrefresh(mode);
	hw770_get_current_fb_info(1,&fb_info);
	logicalMode.valid_edid = false;
	if (sdev->hdmi1_edid && drm_edid_header_is_valid((u8 *)sdev->hdmi1_edid) == 8)
		logicalMode.valid_edid = true;

	logicalMode.x = mode->hdisplay;
	logicalMode.y = mode->vdisplay;
	logicalMode.bpp = smi_bpp;
	logicalMode.hz = refresh_rate;
	logicalMode.pitch = 0;
	logicalMode.dispCtrl = 1;

	if (monitor_status)
	{
		printk("HDMI1: Monitor status is $%d\n", monitor_status);
		hw770_setMode(&logicalMode, *mode);
		ret = hw770_set_hdmi_mode(&logicalMode, *mode, sdev->is_hdmi[1], INDEX_HDMI1);
		if (ret != 0)
		{
			printk("HDMI Mode not supported!\n");
			goto error;
		}
		hw770_set_current_pitch(INDEX_HDMI1,&fb_info);
	}
	return IRQ_HANDLED;
error:
	return IRQ_HANDLED;

}

irqreturn_t smi_hdmi2_pnp_handler(int irq, void *dev_id)
{
	struct drm_display_mode *mode;
	struct smi_device *sdev;
	struct drm_device *dev;
	logicalMode_t logicalMode;
	unsigned long refresh_rate;
	struct drm_crtc *crtc2;
	struct smi_770_fb_info fb_info = {0};
	int monitor_status = 0;
	int ret = 0;

	dev = dev_id;
	sdev = dev->dev_private;
	monitor_status = hw770_hdmi_detect(2);
	if(!monitor_status)
		return IRQ_HANDLED;

	crtc2 = sdev->smi_enc_tab[4]->crtc;
	if (crtc2) {
		printk("CRTC2 found: %p\n", crtc2);
	} else {
		printk("CRTC2 not found\n");
		goto error;
	}

	mode = &crtc2->mode;

	if(!mode)
		goto error;

	refresh_rate = drm_mode_vrefresh(mode);
	hw770_get_current_fb_info(2,&fb_info);
	logicalMode.valid_edid = false;
	if (sdev->hdmi2_edid && drm_edid_header_is_valid((u8 *)sdev->hdmi2_edid) == 8)
		logicalMode.valid_edid = true;

	logicalMode.x = mode->hdisplay;
	logicalMode.y = mode->vdisplay;
	logicalMode.bpp = smi_bpp;
	logicalMode.hz = refresh_rate;
	logicalMode.pitch = 0;
	logicalMode.dispCtrl = 2;


	if (monitor_status)
	{
		printk("HDMI2: Monitor status is $%d\n", monitor_status);
		hw770_setMode(&logicalMode, *mode);
		ret = hw770_set_hdmi_mode(&logicalMode, *mode, sdev->is_hdmi[2], INDEX_HDMI2);
		if (ret != 0)
		{
			printk("HDMI Mode not supported!\n");
			goto error;
		}
		hw770_set_current_pitch(INDEX_HDMI2,&fb_info);
	}
	return IRQ_HANDLED;
error:
	return IRQ_HANDLED;

}






static const struct file_operations smi_driver_fops = {
	.owner = THIS_MODULE,
	.open = drm_open,
	.release = drm_release,
	.unlocked_ioctl = drm_ioctl,
	.mmap = smi_mmap,
	.poll = drm_poll,
#ifdef CONFIG_COMPAT
	.compat_ioctl = drm_compat_ioctl,
#endif
	.read = drm_read,
	.llseek = no_llseek,
};

#if LINUX_VERSION_CODE >= KERNEL_VERSION(5,1,0)
#define DRIVER_IRQ_SHARED 0
#endif

static struct drm_driver driver = {
#ifdef PRIME
	.driver_features = DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED |DRIVER_GEM |DRIVER_PRIME | DRIVER_MODESET,
#else
	.driver_features = DRIVER_HAVE_IRQ | DRIVER_IRQ_SHARED |DRIVER_GEM | DRIVER_MODESET,
#endif
	.load = smi_driver_load,
	.unload = smi_driver_unload,
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)))
	.set_busid = drm_pci_set_busid,
#endif	
	.fops = &smi_driver_fops,
	.name = DRIVER_NAME,
	.desc = DRIVER_DESC,
	.date = DRIVER_DATE,
	.major = DRIVER_MAJOR,
	.minor = DRIVER_MINOR,
	.patchlevel = DRIVER_PATCHLEVEL,
#if (LINUX_VERSION_CODE <= KERNEL_VERSION(3,12,0))
	.gem_init_object = smi_gem_init_object,
#endif
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0))
	.gem_free_object = smi_gem_free_object,
#else
	.gem_free_object_unlocked = smi_gem_free_object,
#endif	
	.dumb_create = smi_dumb_create,
	.dumb_map_offset = smi_dumb_mmap_offset,
#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,12,0))
	.dumb_destroy = smi_dumb_destroy,
#else
	.dumb_destroy = drm_gem_dumb_destroy,
#endif
#if ((LINUX_VERSION_CODE >= KERNEL_VERSION(4,4,0)) && \
	(LINUX_VERSION_CODE < KERNEL_VERSION(4,12,0)))
	.get_vblank_counter	= drm_vblank_no_hw_counter,
#endif	
	.enable_vblank		= smi_enable_vblank,
	.disable_vblank		= smi_disable_vblank,
	.irq_preinstall = smi_irq_preinstall,
	.irq_postinstall = smi_irq_postinstall,
	.irq_uninstall = smi_irq_uninstall,
	.irq_handler		= smi_drm_interrupt,
#ifdef PRIME
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,14,0))
	.prime_handle_to_fd	= drm_gem_prime_handle_to_fd,
	.prime_fd_to_handle	= drm_gem_prime_fd_to_handle,

	.gem_prime_import	= drm_gem_prime_import,
	.gem_prime_export	= drm_gem_prime_export,

	.gem_prime_get_sg_table	= smi_gem_prime_get_sg_table,
	.gem_prime_import_sg_table = smi_gem_prime_import_sg_table,
	.gem_prime_vmap		= smi_gem_prime_vmap,
	.gem_prime_vunmap	= smi_gem_prime_vunmap,
	.gem_prime_pin		= smi_gem_prime_pin,
	.gem_prime_unpin 	= smi_gem_prime_unpin,
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0))
	.gem_prime_res_obj = smi_gem_prime_res_obj,
#endif
#endif

};

static const struct dev_pm_ops smi_pm_ops = {
	.suspend = smi_pm_suspend,
	.resume = smi_pm_resume,
	.freeze = smi_pm_freeze,
	.thaw = smi_pm_thaw,
	.poweroff = smi_pm_poweroff,
	.restore = smi_pm_resume,
};

static struct pci_driver smi_pci_driver = {
	.name = DRIVER_NAME,
	.id_table = pciidlist,
	.probe = smi_pci_probe,
	.remove = smi_pci_remove,
	.driver.pm = &smi_pm_ops,
};

static int __init smi_init(void)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0))
#ifdef CONFIG_VGA_CONSOLE
	if (vgacon_text_force() && smi_modeset == -1)
		return -EINVAL;
#endif
#else
	if (vgacon_text_force() && smi_modeset == -1)
		return -EINVAL;

#endif
	if (smi_modeset == 0)
		return -EINVAL;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0))
	return drm_pci_init(&driver, &smi_pci_driver);
#else
	return pci_register_driver(&smi_pci_driver);
#endif
}

static void __exit smi_exit(void)
{
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0))
	drm_pci_exit(&driver, &smi_pci_driver);
#else
	pci_unregister_driver(&smi_pci_driver);
#endif
}

module_init(smi_init);
module_exit(smi_exit);

MODULE_DEVICE_TABLE(pci, pciidlist);
MODULE_AUTHOR(DRIVER_AUTHOR);
MODULE_DESCRIPTION(DRIVER_DESC);
MODULE_LICENSE("GPL");
MODULE_VERSION(_version_);

