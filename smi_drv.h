/*
 * Copyright 2016 SiliconMotion Inc.
 *
 * This file is subject to the terms and conditions of the GNU General
 * Public License version 2. See the file COPYING in the main
 * directory of this archive for more details.
 *
 */

#ifndef __SMI_DRV_H__
#define __SMI_DRV_H__

#include "smi_ver.h"

#include <video/vga.h>
#include <drm/drm_crtc.h>
#include <drm/drm_fb_helper.h>
#include <drm/drm_edid.h>

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,9,0))
#include <drm/drm_encoder.h>
#endif

#include <drm/drmP.h>
#include <drm/ttm/ttm_bo_api.h>
#include <drm/ttm/ttm_bo_driver.h>
#include <drm/ttm/ttm_placement.h>
#include <drm/ttm/ttm_memory.h>
#include <drm/ttm/ttm_module.h>
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,17,0))
#include <drm/drm_gem.h>
#endif

#include <linux/i2c-algo-bit.h>
#include <linux/i2c.h>

#define DRIVER_AUTHOR		"SiliconMotion"

#define DRIVER_NAME		"smifb"
#define DRIVER_DESC		"SiliconMotion GPU DRM Driver"
#define DRIVER_DATE		"20260108"

#define DRIVER_MAJOR		1
#define DRIVER_MINOR		5
#define DRIVER_PATCHLEVEL	3

#define SMIFB_CONN_LIMIT 3



#define RELEASE_TYPE "Linux DRM Display Driver Release"
#define SUPPORT_CHIP " SM750, SM768, SM770"


#define _version_	"1.5.3.0"

#undef  NO_WC

#ifdef CONFIG_CPU_LOONGSON3
#define NO_WC
#endif



extern int smi_pat;
extern int smi_debug;

#define PFX "[smi] "
extern int smi_indent;


#define ENTER()	do { \
	if (smi_debug) {\
		printk(KERN_DEBUG PFX "%*c %s\n",smi_indent++,'>',__func__); \
	} \
} while (0)

#define LEAVE(...)	do { \
	if (smi_debug) {\
		printk(KERN_DEBUG PFX "%*c %s\n",--smi_indent,'<',__func__); \
	} \
	return __VA_ARGS__; \
} while (0)

#define dbg_msg(fmt,args...)	do { \
		if (smi_debug) {\
			printk(KERN_DEBUG "[%s]:" fmt,__func__,## args); \
		} \
} while (0)


#define err_msg(fmt,args...)	do { \
		if (smi_debug) {\
			printk(KERN_ERR  fmt, ## args); \
		} \
} while (0)


#define war_msg(fmt,args...)	do { \
		if (smi_debug) {\
			printk(KERN_WARNING fmt, ## args); \
		} \
} while (0)

#define inf_msg(fmt,args...) 	do { \
		if (smi_debug) {\
			printk(KERN_INFO fmt, ## args); \
		} \
} while (0)


#ifdef UNUSED
#elif defined(__GNUC__)
#define UNUSED(x) UNUSED_##x __attribute__((unused))
#elif defined(__LCLINT__)
#define UNUSED(x) /*@unused@*/ x
#elif defined(__cplusplus)
#define UNUSED(x)
#else
#define UNUSED(x) x
#endif

#ifndef CONFIG_LOONGARCH
#define ENABLE_HDMI_IRQ 
#else
#undef  ENABLE_HDMI_IRQ
#endif

#define USE_I2C_ADAPTER 1

#define SMI_MAX_FB_HEIGHT 8192
#define SMI_MAX_FB_WIDTH 8192

#define MAX_CRTC_750 2
#define MAX_CRTC_768 2
#define MAX_CRTC_770 3
#define MAX_CRTC(specId) (specId == SPC_SM750)? MAX_CRTC_750: (specId == SPC_SM768)? MAX_CRTC_768:MAX_CRTC_770

#define MAX_ENCODER_750 2
#define MAX_ENCODER_768 3
#define MAX_ENCODER_770 5
#define MAX_ENCODER(specId) (specId == SPC_SM750)? MAX_ENCODER_750: (specId == SPC_SM768)? MAX_ENCODER_768:MAX_ENCODER_770


#define smi_DPMS_CLEARED (-1)



extern int smi_bpp;
extern int force_connect;
extern int lvds_channel;
extern int audio_en;
extern int fixed_width;
extern int fixed_height;
extern int hwi2c_en;
extern int swcur_en;
extern int edid_mode;
extern int lcd_scale;




struct smi_750_register;
struct smi_768_register;
struct smi_770_register;
struct smi_fbdev;

struct smi_crtc {
	struct drm_crtc			base;
	u8				lut_r[256], lut_g[256], lut_b[256];
	int				last_dpms;
	bool				enabled;
	int crtc_index;
	int CursorOffset;
};

#define to_smi_crtc(x) container_of(x, struct smi_crtc, base)
#define to_smi_encoder(x) container_of(x, struct smi_encoder, base)
#define to_smi_framebuffer(x) container_of(x, struct smi_framebuffer, base)







struct smi_mode_info {
	bool				mode_config_initialized;
	struct smi_crtc		*crtc;
	/* pointer to fbdev info structure */
	struct smi_fbdev		*gfbdev;
};

struct smi_encoder {
	struct drm_encoder		base;
	int				last_dpms;
};





struct smi_framebuffer {
	struct drm_framebuffer		base;
	struct drm_gem_object *obj;
	void* vmapping;
};



struct smi_device {
	struct drm_device		*dev;
	struct snd_card 		*card;
	unsigned long			flags;	

	resource_size_t			rmmio_base;
	resource_size_t			rmmio_size;
	resource_size_t			vram_size;
	resource_size_t			vram_base;
	void __iomem			*rmmio;
	void __iomem 			*vram;

	int specId;
	
	int m_connector;  
	//bit 0: DVI, bit 1: VGA, bit 2: HDMI, bit 3: HDMI1, bit 4: HDMI2, bit 5: DP, bit 6: DP1

	struct drm_encoder *smi_enc_tab[MAX_ENCODER_770];
	struct drm_connector *smi_conn_tab[MAX_ENCODER_770];

	struct smi_mode_info		mode_info;

	int				num_crtc;
	int fb_mtrr;	
	bool				need_dma32;
	struct {
#if LINUX_VERSION_CODE < KERNEL_VERSION(5,0,0)
		struct drm_global_reference mem_global_ref;
		struct ttm_bo_global_ref bo_global_ref;
#endif
		struct ttm_bo_device bdev;
	} ttm;
	bool mm_inited;
	void *vram_save;
	union {
	struct smi_750_register *regsave;
	struct smi_768_register *regsave_768;
		struct smi_770_register *regsave_770;
	};
#ifdef USE_HDMICHIP
	struct edid si9022_edid[2];
#endif
    void *dvi_edid;
	void *vga_edid;
	void *hdmi_edid;

#if USE_I2C_ADAPTER
	struct edid *hdmi0_edid;
	struct edid *hdmi1_edid;
	struct edid *hdmi2_edid;
	struct edid *dp0_edid;
	struct edid *dp1_edid;
#else  //increase edid buffer size
	struct edid hdmi0_edid[4];
	struct edid hdmi1_edid[4];
	struct edid hdmi2_edid[4];
	struct edid dp0_edid[4];
	struct edid dp1_edid[4];
#endif

	struct drm_display_mode *fixed_mode;
	bool is_768hdmi;
	bool is_hdmi[SMIFB_CONN_LIMIT];
	bool is_boot_gpu;

};

struct smi_connector {
	struct drm_connector		base;
	struct i2c_adapter adapter;
	struct i2c_adapter dp_adapter;
	struct i2c_algo_bit_data bit_data;
	unsigned char i2c_scl;
	unsigned char i2c_sda;
	unsigned char i2cNumber;
	bool i2c_hw_enabled;
	struct mutex i2c_lock;
	bool i2c_is_segment;
	bool i2c_is_regaddr;
	int i2c_slave_reg;
	int i2c_slave_number;
	//struct drm_dp_aux dp_aux;
};


struct smi_fbdev {
	struct drm_fb_helper helper;
	struct smi_framebuffer gfb;
	struct list_head fbdev_list;
	int size;
	int x1, y1, x2, y2; /* dirty rect */
	spinlock_t dirty_lock;
};

static inline struct smi_connector *to_smi_connector(struct drm_connector *connector)
{
	return container_of(connector, struct smi_connector, base);
}
struct smi_bo {
	struct ttm_buffer_object bo;
	struct ttm_placement placement;
	struct ttm_bo_kmap_obj kmap;
	struct drm_gem_object gem;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,14,0))
	struct ttm_place placements[3];
#else
	u32 placements[3];
#endif	
	int pin_count;
	struct ttm_bo_kmap_obj dma_buf_vmap;
};
#define gem_to_smi_bo(gobj) container_of((gobj), struct smi_bo, gem)

static inline struct smi_bo *
smi_bo(struct ttm_buffer_object *bo)
{
	return container_of(bo, struct smi_bo, bo);
}


#define to_smi_obj(x) container_of(x, struct smi_gem_object, base)
#define DRM_FILE_PAGE_OFFSET (0x100000000ULL >> PAGE_SHIFT)

				/* smi_mode.c */
void smi_crtc_fb_gamma_set(struct drm_crtc *crtc, u16 red, u16 green,
			     u16 blue, int regno);
void smi_crtc_fb_gamma_get(struct drm_crtc *crtc, u16 *red, u16 *green,
			     u16 *blue, int regno);

int smi_calc_hdmi_ctrl(int m_connector);


				/* smi_main.c */
int smi_device_init(struct smi_device *cdev,
		      struct drm_device *ddev,
		      struct pci_dev *pdev,
		      uint32_t flags);
void smi_device_fini(struct smi_device *cdev);
int smi_gem_init_object(struct drm_gem_object *obj);
void smi_gem_free_object(struct drm_gem_object *obj);
int smi_dumb_mmap_offset(struct drm_file *file,
			    struct drm_device *dev,
			    uint32_t handle,
			    uint64_t *offset);
int smi_gem_create(struct drm_device *dev,
		   u32 size, bool iskernel,
		   struct drm_gem_object **obj);
int smi_dumb_create(struct drm_file *file,
		    struct drm_device *dev,
		    struct drm_mode_create_dumb *args);
int smi_dumb_destroy(struct drm_file *file,
		     struct drm_device *dev,
		     uint32_t handle);

int smi_framebuffer_init(struct drm_device *dev,
			   struct smi_framebuffer *gfb,
			    const struct drm_mode_fb_cmd2 *mode_cmd,
			    struct drm_gem_object *obj);

				/* smi_display.c */
int smi_modeset_init(struct smi_device *cdev);
void smi_modeset_fini(struct smi_device *cdev);

				/* smi_fbdev.c */
int smi_fbdev_init(struct smi_device *cdev);
void smi_fbdev_fini(struct smi_device *cdev);
void smi_fb_zfill(struct drm_device *dev, struct smi_fbdev *gfbdev);


				/* smi_irq.c */
void smi_driver_irq_preinstall(struct drm_device *dev);
int smi_driver_irq_postinstall(struct drm_device *dev);
void smi_driver_irq_uninstall(struct drm_device *dev);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3,14,0))	
irqreturn_t smi_driver_irq_handler(int irq, void *arg);
#else
irqreturn_t smi_driver_irq_handler(DRM_IRQ_ARGS);
#endif
				/* smi_kms.c */
int smi_driver_load(struct drm_device *dev, unsigned long flags);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4,11,0))
void smi_driver_unload(struct drm_device *dev);
#else
int smi_driver_unload(struct drm_device *dev);
#endif
//extern struct drm_ioctl_desc smi_ioctls[];
//extern int smi_max_ioctl;

int smi_mm_init(struct smi_device *smi);
void smi_mm_fini(struct smi_device *smi);
void smi_ttm_placement(struct smi_bo *bo, int domain);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0))
int smi_bo_create(struct drm_device *dev, int size, int align, 
		  uint32_t flags, struct sg_table *sg, struct smi_bo **psmibo);
#else
int smi_bo_create(struct drm_device *dev, int size, int align,
		  uint32_t flags, struct sg_table *sg, struct reservation_object *resv, struct smi_bo **psmibo);
#endif
void smi_bo_ttm_destroy(struct ttm_buffer_object *tbo);

int smi_mmap(struct file *filp, struct vm_area_struct *vma);

void hw750_suspend(struct smi_750_register * pSave);
void hw750_resume(struct smi_750_register * pSave);


#if (LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0))
struct drm_plane *smi_plane_init(struct smi_device *cdev, unsigned int possible_crtcs);
#else
struct drm_plane *smi_plane_init(struct smi_device *cdev, unsigned int possible_crtcs, enum drm_plane_type type);
#endif

static inline int smi_bo_reserve(struct smi_bo *bo, bool no_wait)
{
	int ret;
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0))
	ret = ttm_bo_reserve(&bo->bo, true, no_wait, false, NULL);
#else
	ret = ttm_bo_reserve(&bo->bo, true, no_wait, NULL);
#endif
	if (ret) {
		if (ret != -ERESTARTSYS && ret != -EBUSY)
			DRM_ERROR("reserve failed %p\n", bo);
		return ret;
	}
	return 0;
}

static inline void smi_bo_unreserve(struct smi_bo *bo)
{
	ttm_bo_unreserve(&bo->bo);
}


void smi_fb_output_poll_changed(struct smi_device *sdev);



int smi_bo_pin(struct smi_bo *bo, u32 pl_flag, u64 *gpu_addr);
int smi_bo_unpin(struct smi_bo *bo);



struct sg_table *smi_gem_prime_get_sg_table(struct drm_gem_object *obj);
void *smi_gem_prime_vmap(struct drm_gem_object *obj);
void smi_gem_prime_vunmap(struct drm_gem_object *obj, void *vaddr);


struct drm_gem_object *smi_gem_prime_import_sg_table(struct drm_device *dev,
							struct dma_buf_attachment *attach,
							struct sg_table *sg);

int smi_gem_prime_pin(struct drm_gem_object *obj);
void smi_gem_prime_unpin(struct drm_gem_object *obj);

struct reservation_object *smi_gem_prime_res_obj(struct drm_gem_object *obj);



#if (KERNEL_VERSION(4, 12, 0) > LINUX_VERSION_CODE)
int smi_crtc_page_flip(struct drm_crtc *crtc,struct drm_framebuffer *fb,
    struct drm_pending_vblank_event *event, uint32_t page_flip_flags);
#else
int smi_crtc_page_flip(struct drm_crtc *crtc,struct drm_framebuffer *fb,
    struct drm_pending_vblank_event *event, uint32_t page_flip_flags, struct drm_modeset_acquire_ctx *ctx);
#endif

int smi_calc_hdmi_ctrl(int m_connector);
int smi_encoder_crtc_index_changed(int encoder_index);

int smi_audio_init(struct drm_device *dev);
void smi_audio_remove(struct drm_device *dev);
void smi_audio_suspend(struct smi_device *sdev);
void smi_audio_resume(struct smi_device *sdev);

int smi_pwm_init(struct drm_device *ddev);

#if LINUX_VERSION_CODE < KERNEL_VERSION(5, 14, 0)
void smi_pwm_remove(struct drm_device *ddev);
#endif

int smi_ehci_init(struct drm_device *dev);
void smi_ehci_remove(struct drm_device *dev);
void smi_ehci_shutdown(struct drm_device *dev);
irqreturn_t smi_hdmi0_pnp_handler(int irq, void *dev_id);
irqreturn_t smi_hdmi0_hardirq(int irq, void *dev_id);
irqreturn_t smi_hdmi1_pnp_handler(int irq, void *dev_id);
irqreturn_t smi_hdmi1_hardirq(int irq, void *dev_id);
irqreturn_t smi_hdmi2_pnp_handler(int irq, void *dev_id);
irqreturn_t smi_hdmi2_hardirq(int irq, void *dev_id);

#define smi_LUT_SIZE 256
#define CURSOR_WIDTH 64
#define CURSOR_HEIGHT 64

#define PALETTE_INDEX 0x8
#define PALETTE_DATA 0x9

#define USE_DVI 1
#define USE_VGA (1 << 1)
#define USE_HDMI (1 << 2)
#define USE_DVI_VGA (USE_DVI | USE_VGA)
#define USE_DVI_HDMI (USE_DVI | USE_HDMI)
#define USE_VGA_HDMI (USE_VGA | USE_HDMI)
#define USE_ALL (USE_DVI | USE_VGA | USE_HDMI)

/*  for 770  */
#define USE_HDMI0 	(1 << 3)
#define USE_HDMI1 	(1 << 4)
#define USE_HDMI2 	(1 << 5)
#define USE_DP0 	(1 << 6)
#define USE_DP1 	(1 << 7)

#define HDMI_INT_HPD 1
#define HDMI_INT_NOT_HPD 2
/* please use revision id to distinguish sm750le and sm750*/
#define SPC_SM750 	0
#define SPC_SM712 	1
#define SPC_SM502   2
#define SPC_SM768   3
#define SPC_SM770   4
//#define SPC_SM750LE 8

#define PCI_VENDOR_ID_SMI 	0x126f
#define PCI_DEVID_LYNX_EXP	0x0750
#define PCI_DEVID_SM768		0x0768
#define PCI_DEVID_SM770		0x0770


#define BPP32_RED    0x00ff0000
#define BPP32_GREEN  0x0000ff00
#define BPP32_BLUE   0x000000ff
#define BPP32_WHITE  0x00ffffff
#define BPP32_GRAY   0x00808080
#define BPP32_YELLOW 0x00ffff00
#define BPP32_CYAN   0x0000ffff
#define BPP32_PINK   0x00ff00ff
#define BPP32_BLACK  0x00000000


#define BPP16_RED    0x0000f800
#define BPP16_GREEN  0x000007e0
#define BPP16_BLUE   0x0000001f
#define BPP16_WHITE  0x0000ffff
#define BPP16_GRAY   0x00008410
#define BPP16_YELLOW 0x0000ffe0
#define BPP16_CYAN   0x000007ff
#define BPP16_PINK   0x0000f81f
#define BPP16_BLACK  0x00000000

#endif				/* __SMI_DRV_H__ */
