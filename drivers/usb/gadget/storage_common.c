/*
 * storage_common.c -- Common definitions for mass storage functionality
 *
 * Copyright (C) 2003-2008 Alan Stern
 * Copyeight (C) 2009 Samsung Electronics
 * Author: Michal Nazarewicz (m.nazarewicz@samsung.com)
 *
 * Ported to u-boot:
 * Andrzej Pietrasiewicz <andrzej.p@samsung.com>
 *
 * Code refactoring & cleanup:
 * Łukasz Majewski <l.majewski@samsung.com>
 *
 * SPDX-License-Identifier:	GPL-2.0+
 */


/*
 * This file requires the following identifiers used in USB strings to
 * be defined (each of type pointer to char):
 *  - fsg_string_manufacturer -- name of the manufacturer
 *  - fsg_string_product      -- name of the product
 *  - fsg_string_serial       -- product's serial
 *  - fsg_string_config       -- name of the configuration
 *  - fsg_string_interface    -- name of the interface
 * The first four are only needed when FSG_DESCRIPTORS_DEVICE_STRINGS
 * macro is defined prior to including this file.
 */

/*
 * When FSG_NO_INTR_EP is defined fsg_fs_intr_in_desc and
 * fsg_hs_intr_in_desc objects as well as
 * FSG_FS_FUNCTION_PRE_EP_ENTRIES and FSG_HS_FUNCTION_PRE_EP_ENTRIES
 * macros are not defined.
 *
 * When FSG_NO_DEVICE_STRINGS is defined FSG_STRING_MANUFACTURER,
 * FSG_STRING_PRODUCT, FSG_STRING_SERIAL and FSG_STRING_CONFIG are not
 * defined (as well as corresponding entries in string tables are
 * missing) and FSG_STRING_INTERFACE has value of zero.
 *
 * When FSG_NO_OTG is defined fsg_otg_desc won't be defined.
 */

/*
 * When FSG_BUFFHD_STATIC_BUFFER is defined when this file is included
 * the fsg_buffhd structure's buf field will be an array of FSG_BUFLEN
 * characters rather then a pointer to void.
 */


/* #include <asm/unaligned.h> */


/*
 * Thanks to NetChip Technologies for donating this product ID.
 *
 * DO NOT REUSE THESE IDs with any other driver!!  Ever!!
 * Instead:  allocate your own, using normal USB-IF procedures.
 */
#define FSG_VENDOR_ID	0x0525	/* NetChip */
#define FSG_PRODUCT_ID	0xa4a5	/* Linux-USB File-backed Storage Gadget */

/*-------------------------------------------------------------------------*/

#ifndef DEBUG
#undef VERBOSE_DEBUG
#undef DUMP_MSGS
#endif /* !DEBUG */

#ifdef VERBOSE_DEBUG
#define VLDBG	LDBG
#else
#define VLDBG(lun, fmt, args...) do { } while (0)
#endif /* VERBOSE_DEBUG */

/*
#define LDBG(lun, fmt, args...)   dev_dbg (&(lun)->dev, fmt, ## args)
#define LERROR(lun, fmt, args...) dev_err (&(lun)->dev, fmt, ## args)
#define LWARN(lun, fmt, args...)  dev_warn(&(lun)->dev, fmt, ## args)
#define LINFO(lun, fmt, args...)  dev_info(&(lun)->dev, fmt, ## args)
*/

#define LDBG(lun, fmt, args...) do { } while (0)
#define LERROR(lun, fmt, args...) do { } while (0)
#define LWARN(lun, fmt, args...) do { } while (0)
#define LINFO(lun, fmt, args...) do { } while (0)

/*
 * Keep those macros in sync with those in
 * include/linux/usb/composite.h or else GCC will complain.  If they
 * are identical (the same names of arguments, white spaces in the
 * same places) GCC will allow redefinition otherwise (even if some
 * white space is removed or added) warning will be issued.
 *
 * Those macros are needed here because File Storage Gadget does not
 * include the composite.h header.  For composite gadgets those macros
 * are redundant since composite.h is included any way.
 *
 * One could check whether those macros are already defined (which
 * would indicate composite.h had been included) or not (which would
 * indicate we were in FSG) but this is not done because a warning is
 * desired if definitions here differ from the ones in composite.h.
 *
 * We want the definitions to match and be the same in File Storage
 * Gadget as well as Mass Storage Function (and so composite gadgets
 * using MSF).  If someone changes them in composite.h it will produce
 * a warning in this file when building MSF.
 */

#define DBG(d, fmt, args...)     debug(fmt , ## args)
#define VDBG(d, fmt, args...)    debug(fmt , ## args)
/* #define ERROR(d, fmt, args...)   printf(fmt , ## args) */
/* #define WARNING(d, fmt, args...) printf(fmt , ## args) */
/* #define INFO(d, fmt, args...)    printf(fmt , ## args) */

/* #define DBG(d, fmt, args...)     do { } while (0) */
/* #define VDBG(d, fmt, args...)    do { } while (0) */
#define ERROR(d, fmt, args...)   do { } while (0)
#define WARNING(d, fmt, args...) do { } while (0)
#define INFO(d, fmt, args...)    do { } while (0)

#ifdef DUMP_MSGS

/* dump_msg(fsg, const char * label, const u8 * buf, unsigned length); */
# define dump_msg(fsg, label, buf, length) do {                         \
	if (length < 512) {						\
		DBG(fsg, "%s, length %u:\n", label, length);		\
		print_hex_dump(KERN_DEBUG, "", DUMP_PREFIX_OFFSET,	\
			       16, 1, buf, length, 0);			\
	}								\
} while (0)

#  define dump_cdb(fsg) do { } while (0)

#else

#  define dump_msg(fsg, /* const char * */ label, \
		   /* const u8 * */ buf, /* unsigned */ length) do { } while (0)

#  ifdef VERBOSE_DEBUG

#    define dump_cdb(fsg)						\
	print_hex_dump(KERN_DEBUG, "SCSI CDB: ", DUMP_PREFIX_NONE,	\
		       16, 1, (fsg)->cmnd, (fsg)->cmnd_size, 0)		\

#  else

#    define dump_cdb(fsg) do { } while (0)

#  endif /* VERBOSE_DEBUG */

#endif /* DUMP_MSGS */

#include <usb_mass_storage.h>

/*-------------------------------------------------------------------------*/

static inline u32 get_unaligned_be24(u8 *buf)
{
	return 0xffffff & (u32) get_unaligned_be32(buf - 1);
}

/*-------------------------------------------------------------------------*/

enum {
#ifndef FSG_NO_DEVICE_STRINGS
	FSG_STRING_MANUFACTURER	= 1,
	FSG_STRING_PRODUCT,
	FSG_STRING_SERIAL,
	FSG_STRING_CONFIG,
#endif
	FSG_STRING_INTERFACE
};

#ifndef FSG_NO_OTG
static struct usb_otg_descriptor
fsg_otg_desc = {
	.bLength =		sizeof fsg_otg_desc,
	.bDescriptorType =	USB_DT_OTG,

	.bmAttributes =		USB_OTG_SRP,
};
#endif

/* There is only one interface. */

static struct usb_interface_descriptor
fsg_intf_desc = {
	.bLength =		sizeof fsg_intf_desc,
	.bDescriptorType =	USB_DT_INTERFACE,

	.bNumEndpoints =	2,		/* Adjusted during fsg_bind() */
	.bInterfaceClass =	USB_CLASS_MASS_STORAGE,
	.bInterfaceSubClass =	USB_SC_SCSI,	/* Adjusted during fsg_bind() */
	.bInterfaceProtocol =	USB_PR_BULK,	/* Adjusted during fsg_bind() */
	.iInterface =		FSG_STRING_INTERFACE,
};

/*
 * Three full-speed endpoint descriptors: bulk-in, bulk-out, and
 * interrupt-in.
 */

static struct usb_endpoint_descriptor
fsg_fs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
};

static struct usb_endpoint_descriptor
fsg_fs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_OUT,
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	/* wMaxPacketSize set by autoconfiguration */
};

#ifndef FSG_NO_INTR_EP

static struct usb_endpoint_descriptor
fsg_fs_intr_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	.bEndpointAddress =	USB_DIR_IN,
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(2),
	.bInterval =		32,	/* frames -> 32 ms */
};

#ifndef FSG_NO_OTG
#  define FSG_FS_FUNCTION_PRE_EP_ENTRIES	2
#else
#  define FSG_FS_FUNCTION_PRE_EP_ENTRIES	1
#endif

#endif

static struct usb_descriptor_header *fsg_fs_function[] = {
#ifndef FSG_NO_OTG
	(struct usb_descriptor_header *) &fsg_otg_desc,
#endif
	(struct usb_descriptor_header *) &fsg_intf_desc,
	(struct usb_descriptor_header *) &fsg_fs_bulk_in_desc,
	(struct usb_descriptor_header *) &fsg_fs_bulk_out_desc,
#ifndef FSG_NO_INTR_EP
	(struct usb_descriptor_header *) &fsg_fs_intr_in_desc,
#endif
	NULL,
};

/*
 * USB 2.0 devices need to expose both high speed and full speed
 * descriptors, unless they only run at full speed.
 *
 * That means alternate endpoint descriptors (bigger packets)
 * and a "device qualifier" ... plus more construction options
 * for the configuration descriptor.
 */
static struct usb_endpoint_descriptor
fsg_hs_bulk_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_in_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
};

static struct usb_endpoint_descriptor
fsg_hs_bulk_out_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_bulk_out_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_BULK,
	.wMaxPacketSize =	cpu_to_le16(512),
	.bInterval =		1,	/* NAK every 1 uframe */
};

#ifndef FSG_NO_INTR_EP

static struct usb_endpoint_descriptor
fsg_hs_intr_in_desc = {
	.bLength =		USB_DT_ENDPOINT_SIZE,
	.bDescriptorType =	USB_DT_ENDPOINT,

	/* bEndpointAddress copied from fs_intr_in_desc during fsg_bind() */
	.bmAttributes =		USB_ENDPOINT_XFER_INT,
	.wMaxPacketSize =	cpu_to_le16(2),
	.bInterval =		9,	/* 2**(9-1) = 256 uframes -> 32 ms */
};

#ifndef FSG_NO_OTG
#  define FSG_HS_FUNCTION_PRE_EP_ENTRIES	2
#else
#  define FSG_HS_FUNCTION_PRE_EP_ENTRIES	1
#endif

#endif

static struct usb_descriptor_header *fsg_hs_function[] = {
#ifndef FSG_NO_OTG
	(struct usb_descriptor_header *) &fsg_otg_desc,
#endif
	(struct usb_descriptor_header *) &fsg_intf_desc,
	(struct usb_descriptor_header *) &fsg_hs_bulk_in_desc,
	(struct usb_descriptor_header *) &fsg_hs_bulk_out_desc,
#ifndef FSG_NO_INTR_EP
	(struct usb_descriptor_header *) &fsg_hs_intr_in_desc,
#endif
	NULL,
};

/* Maxpacket and other transfer characteristics vary by speed. */
static struct usb_endpoint_descriptor *
fsg_ep_desc(struct usb_gadget *g, struct usb_endpoint_descriptor *fs,
		struct usb_endpoint_descriptor *hs)
{
	if (gadget_is_dualspeed(g) && g->speed == USB_SPEED_HIGH)
		return hs;
	return fs;
}

/* Static strings, in UTF-8 (for simplicity we use only ASCII characters) */
static struct usb_string		fsg_strings[] = {
#ifndef FSG_NO_DEVICE_STRINGS
	{FSG_STRING_MANUFACTURER,	fsg_string_manufacturer},
	{FSG_STRING_PRODUCT,		fsg_string_product},
	{FSG_STRING_SERIAL,		fsg_string_serial},
	{FSG_STRING_CONFIG,		fsg_string_config},
#endif
	{FSG_STRING_INTERFACE,		fsg_string_interface},
	{}
};

static struct usb_gadget_strings	fsg_stringtab = {
	.language	= 0x0409,		/* en-us */
	.strings	= fsg_strings,
};

/*-------------------------------------------------------------------------*/

/*
 * If the next two routines are called while the gadget is registered,
 * the caller must own fsg->filesem for writing.
 */

static int fsg_lun_open(struct fsg_lun *curlun, unsigned int num_sectors,
			const char *filename)
{
	int				ro;

	/* R/W if we can, R/O if we must */
	ro = curlun->initially_ro;

	curlun->ro = ro;
	curlun->file_length = num_sectors << 9;
	curlun->num_sectors = num_sectors;
	debug("open backing file: %s\n", filename);

	return 0;
}

static void fsg_lun_close(struct fsg_lun *curlun)
{
}

/*-------------------------------------------------------------------------*/

/*
 * Sync the file data, don't bother with the metadata.
 * This code was copied from fs/buffer.c:sys_fdatasync().
 */
static int fsg_lun_fsync_sub(struct fsg_lun *curlun)
{
	return 0;
}

static void store_cdrom_address(u8 *dest, int msf, u32 addr)
{
	if (msf) {
		/* Convert to Minutes-Seconds-Frames */
		addr >>= 2;		/* Convert to 2048-byte frames */
		addr += 2*75;		/* Lead-in occupies 2 seconds */
		dest[3] = addr % 75;	/* Frames */
		addr /= 75;
		dest[2] = addr % 60;	/* Seconds */
		addr /= 60;
		dest[1] = addr;		/* Minutes */
		dest[0] = 0;		/* Reserved */
	} else {
		/* Absolute sector */
		put_unaligned_be32(addr, dest);
	}
}

/*-------------------------------------------------------------------------*/
