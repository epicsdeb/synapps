/***********************************************************************
 *
 * mar345: marhw.h
 *
 * Copyright by:        Dr. Claudio Klein
 *                      X-ray Research GmbH, Hamburg
 *
 * Version:     1.0
 * Date:        16/01/1997
 *
 ***********************************************************************/

/*
 *      The following parameters are used in the
 *	status block read from the ESD controller
 */

#define	ALL_DONE_BIT	0x01
#define	C_ACTIVE_BIT	0x02
#define	STARTED_BIT	0x04
#define	QUEUED_BIT	0x80

#define	ERROR_BIT	0x01
#define	ABORTED_BIT	0x80

/*
 *	These bits can be written to cause
 *	the scanner to do one of the below.
 */

#define	LOCK_IP_BIT		0x01
#define	LASER_SHUTTER_OUT	0x02
#define	HV_BIT			0x04
#define	MAINS_ACTIVE_BIT	0x10
#define	ION_CHAMBER_SELECT	0x20
#define	XRAY_SHUTTER_OUT	0x40
#define	ERASE_LAMP_ON_BIT	0x80

/*
 * The following bits are status bits from scanner: CIOin-Port B
 */

#define LOWER_STATUS_BIT        0x80
#define UPPER_STATUS_BIT        0x40
#define LASERSHUTTER_STATUS_BIT 0x20
#define LOCK_STATUS_BIT         0x10
#define POSITION_STATUS_BIT     0x08
#define ERASE_STATUS_BIT        0x04
#define LASER_STATUS_BIT        0x02

/*
 * The following bits are status bits from scanner: PITin-Port B
 */

#define TRANSREF_STATUS_BIT     0x01
#define XRAYSHUTTER_STATUS_BIT  0x02

