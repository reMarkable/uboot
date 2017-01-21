/*
 * f_mass_storage.c -- Mass Storage USB Composite Function
 *
 * Copyright (C) 2003-2008 Alan Stern
 * Copyright (C) 2009 Samsung Electronics
 *                    Author: Michal Nazarewicz <m.nazarewicz@samsung.com>
 * All rights reserved.
 *
 * SPDX-License-Identifier: GPL-2.0+	BSD-3-Clause
 */

#ifndef __F_MASS_STORAGE_H__
#define __F_MASS_STORAGE_H__

#include <linux/usb/composite.h>

/*-------------------------------------------------------------------------*/

/* SCSI device types */
#define TYPE_DISK	0x00
#define TYPE_CDROM	0x05

/* USB protocol value = the transport method */
#define USB_PR_CBI	0x00		/* Control/Bulk/Interrupt */
#define USB_PR_CB	0x01		/* Control/Bulk w/o interrupt */
#define USB_PR_BULK	0x50		/* Bulk-only */

/* USB subclass value = the protocol encapsulation */
#define USB_SC_RBC	0x01		/* Reduced Block Commands (flash) */
#define USB_SC_8020	0x02		/* SFF-8020i, MMC-2, ATAPI (CD-ROM) */
#define USB_SC_QIC	0x03		/* QIC-157 (tape) */
#define USB_SC_UFI	0x04		/* UFI (floppy) */
#define USB_SC_8070	0x05		/* SFF-8070i (removable) */
#define USB_SC_SCSI	0x06		/* Transparent SCSI */

/* Bulk-only data structures */

/* Command Block Wrapper */
struct fsg_bulk_cb_wrap {
	__le32	Signature;		/* Contains 'USBC' */
	u32	Tag;			/* Unique per command id */
	__le32	DataTransferLength;	/* Size of the data */
	u8	Flags;			/* Direction in bit 7 */
	u8	Lun;			/* LUN (normally 0) */
	u8	Length;			/* Of the CDB, <= MAX_COMMAND_SIZE */
	u8	CDB[16];		/* Command Data Block */
};

#define USB_BULK_CB_WRAP_LEN	31
#define USB_BULK_CB_SIG		0x43425355	/* Spells out USBC */
#define USB_BULK_IN_FLAG	0x80

/* Command Status Wrapper */
struct bulk_cs_wrap {
	__le32	Signature;		/* Should = 'USBS' */
	u32	Tag;			/* Same as original command */
	__le32	Residue;		/* Amount not transferred */
	u8	Status;			/* See below */
};

#define USB_BULK_CS_WRAP_LEN	13
#define USB_BULK_CS_SIG		0x53425355	/* Spells out 'USBS' */
#define USB_STATUS_PASS		0
#define USB_STATUS_FAIL		1
#define USB_STATUS_PHASE_ERROR	2

/* Bulk-only class specific requests */
#define USB_BULK_RESET_REQUEST		0xff
#define USB_BULK_GET_MAX_LUN_REQUEST	0xfe

/* CBI Interrupt data structure */
struct interrupt_data {
	u8	bType;
	u8	bValue;
};

#define CBI_INTERRUPT_DATA_LEN		2

/* CBI Accept Device-Specific Command request */
#define USB_CBI_ADSC_REQUEST		0x00

/* Length of a SCSI Command Data Block */
#define MAX_COMMAND_SIZE	16

/* SCSI commands that we recognize */
#define SC_FORMAT_UNIT			0x04
#define SC_INQUIRY			0x12
#define SC_MODE_SELECT_6		0x15
#define SC_MODE_SELECT_10		0x55
#define SC_MODE_SENSE_6			0x1a
#define SC_MODE_SENSE_10		0x5a
#define SC_PREVENT_ALLOW_MEDIUM_REMOVAL	0x1e
#define SC_READ_6			0x08
#define SC_READ_10			0x28
#define SC_READ_12			0xa8
#define SC_READ_CAPACITY		0x25
#define SC_READ_FORMAT_CAPACITIES	0x23
#define SC_READ_HEADER			0x44
#define SC_READ_TOC			0x43
#define SC_RELEASE			0x17
#define SC_REQUEST_SENSE		0x03
#define SC_RESERVE			0x16
#define SC_SEND_DIAGNOSTIC		0x1d
#define SC_START_STOP_UNIT		0x1b
#define SC_SYNCHRONIZE_CACHE		0x35
#define SC_TEST_UNIT_READY		0x00
#define SC_VERIFY			0x2f
#define SC_WRITE_6			0x0a
#define SC_WRITE_10			0x2a
#define SC_WRITE_12			0xaa

/* Vendor specific */
#ifdef CONFIG_USB_FUNCTION_FSL_UTP
#define SC_FSL_UTP			0xf0
#endif


/* SCSI Sense Key/Additional Sense Code/ASC Qualifier values */
#define SS_NO_SENSE				0
#define SS_COMMUNICATION_FAILURE		0x040800
#define SS_INVALID_COMMAND			0x052000
#define SS_INVALID_FIELD_IN_CDB			0x052400
#define SS_LOGICAL_BLOCK_ADDRESS_OUT_OF_RANGE	0x052100
#define SS_LOGICAL_UNIT_NOT_SUPPORTED		0x052500
#define SS_MEDIUM_NOT_PRESENT			0x023a00
#define SS_MEDIUM_REMOVAL_PREVENTED		0x055302
#define SS_NOT_READY_TO_READY_TRANSITION	0x062800
#define SS_RESET_OCCURRED			0x062900
#define SS_SAVING_PARAMETERS_NOT_SUPPORTED	0x053900
#define SS_UNRECOVERED_READ_ERROR		0x031100
#define SS_WRITE_ERROR				0x030c02
#define SS_WRITE_PROTECTED			0x072700

#define SK(x)		((u8) ((x) >> 16))	/* Sense Key byte, etc. */
#define ASC(x)		((u8) ((x) >> 8))
#define ASCQ(x)		((u8) (x))

struct device_attribute { int i; };
#define ETOOSMALL	525

/*-------------------------------------------------------------------------*/

struct fsg_lun {
	loff_t		file_length;
	loff_t		num_sectors;

	unsigned int	initially_ro:1;
	unsigned int	ro:1;
	unsigned int	removable:1;
	unsigned int	cdrom:1;
	unsigned int	prevent_medium_removal:1;
	unsigned int	registered:1;
	unsigned int	info_valid:1;
	unsigned int	nofua:1;

	u32		sense_data;
	u32		sense_data_info;
	u32		unit_attention_data;

	struct device	dev;
};

#define fsg_lun_is_open(curlun)	((curlun)->filp != NULL)
#if 0
static struct fsg_lun *fsg_lun_from_dev(struct device *dev)
{
	return container_of(dev, struct fsg_lun, dev);
}
#endif

/* Big enough to hold our biggest descriptor */
#define EP0_BUFSIZE	256
#define DELAYED_STATUS	(EP0_BUFSIZE + 999)	/* An impossibly large value */

/* Number of buffers we will use.  2 is enough for double-buffering */
#define FSG_NUM_BUFFERS	2

/* Default size of buffer length. */
#define FSG_BUFLEN	((u32)16384)

/* Maximal number of LUNs supported in mass storage function */
#define FSG_MAX_LUNS	8

enum fsg_buffer_state {
	BUF_STATE_EMPTY = 0,
	BUF_STATE_FULL,
	BUF_STATE_BUSY
};

struct fsg_buffhd {
#ifdef FSG_BUFFHD_STATIC_BUFFER
	char				buf[FSG_BUFLEN];
#else
	void				*buf;
#endif
	enum fsg_buffer_state		state;
	struct fsg_buffhd		*next;

	/*
	 * The NetChip 2280 is faster, and handles some protocol faults
	 * better, if we don't submit any short bulk-out read requests.
	 * So we will record the intended request length here.
	 */
	unsigned int			bulk_out_intended_length;

	struct usb_request		*inreq;
	int				inreq_busy;
	struct usb_request		*outreq;
	int				outreq_busy;
};

enum fsg_state {
	/* This one isn't used anywhere */
	FSG_STATE_COMMAND_PHASE = -10,
	FSG_STATE_DATA_PHASE,
	FSG_STATE_STATUS_PHASE,

	FSG_STATE_IDLE = 0,
	FSG_STATE_ABORT_BULK_OUT,
	FSG_STATE_RESET,
	FSG_STATE_INTERFACE_CHANGE,
	FSG_STATE_CONFIG_CHANGE,
	FSG_STATE_DISCONNECT,
	FSG_STATE_EXIT,
	FSG_STATE_TERMINATED
};

enum data_direction {
	DATA_DIR_UNKNOWN = 0,
	DATA_DIR_FROM_HOST,
	DATA_DIR_TO_HOST,
	DATA_DIR_NONE
};

/*-------------------------------------------------------------------------*/

#define GFP_ATOMIC ((gfp_t) 0)
#define PAGE_CACHE_SHIFT	12
#define PAGE_CACHE_SIZE		(1 << PAGE_CACHE_SHIFT)
#define kthread_create(...)	__builtin_return_address(0)
#define wait_for_completion(...) do {} while (0)

struct kref {int x; };
struct completion {int x; };

struct fsg_dev;

/* Data shared by all the FSG instances. */
struct fsg_common {
	struct usb_gadget	*gadget;
	struct fsg_dev		*fsg, *new_fsg;

	struct usb_ep		*ep0;		/* Copy of gadget->ep0 */
	struct usb_request	*ep0req;	/* Copy of cdev->req */
	unsigned int		ep0_req_tag;

	struct fsg_buffhd	*next_buffhd_to_fill;
	struct fsg_buffhd	*next_buffhd_to_drain;
	struct fsg_buffhd	buffhds[FSG_NUM_BUFFERS];

	int			cmnd_size;
	u8			cmnd[MAX_COMMAND_SIZE];

	unsigned int		nluns;
	unsigned int		lun;
	struct fsg_lun          luns[FSG_MAX_LUNS];

	unsigned int		bulk_out_maxpacket;
	enum fsg_state		state;		/* For exception handling */
	unsigned int		exception_req_tag;

	enum data_direction	data_dir;
	u32			data_size;
	u32			data_size_from_cmnd;
	u32			tag;
	u32			residue;
	u32			usb_amount_left;

	unsigned int		can_stall:1;
	unsigned int		free_storage_on_release:1;
	unsigned int		phase_error:1;
	unsigned int		short_packet_received:1;
	unsigned int		bad_lun_okay:1;
	unsigned int		running:1;

	int			thread_wakeup_needed;
	struct completion	thread_notifier;
	struct task_struct	*thread_task;

	/* Callback functions. */
	const struct fsg_operations	*ops;
	/* Gadget's private data. */
	void			*private_data;

	const char *vendor_name;		/*  8 characters or less */
	const char *product_name;		/* 16 characters or less */
	u16 release;

	/* Vendor (8 chars), product (16 chars), release (4
	 * hexadecimal digits) and NUL byte */
	char inquiry_string[8 + 16 + 4 + 1];

	struct kref		ref;
};


struct fsg_dev {
	struct usb_function	function;
	struct usb_gadget	*gadget;	/* Copy of cdev->gadget */
	struct fsg_common	*common;

	u16			interface_number;

	unsigned int		bulk_in_enabled:1;
	unsigned int		bulk_out_enabled:1;

	unsigned long		atomic_bitflags;
#define IGNORE_BULK_OUT		0

	struct usb_ep		*bulk_in;
	struct usb_ep		*bulk_out;

#ifdef CONFIG_USB_FUNCTION_FSL_UTP
	void			*utp;
#endif
};

#endif

