#ifndef __NOVENA_EEPROM_H__
#define __NOVENA_EEPROM_H__

#include <stdint.h>

#define NOVENA_SIGNATURE "Novena"
#define NOVENA_VERSION 2


/* Bitmask polarities used as flag */
enum modesetting_flags {

	/* If 0, this display device is not present */
	channel_present	= 0x01,

	/* If this is lvds1 and this bit is set, use dual-channel LVDS */
	dual_channel	= 0x02,

	vsync_polarity	= 0x04,
	hsync_polarity	= 0x08,

	/* For LVDS, a value of 0 means PSWG, a value of 1 means JEIDA */
	mapping_jeida	= 0x10,

	/* If 0, channel is either 6-bit (LVDS) or 10-bit (HDMI) */
	data_width_8bit	= 0x20,
};

struct modesetting {
	uint32_t	frequency;
	uint16_t	hactive;
	uint16_t	vactive;
	uint16_t	hback_porch;
	uint16_t	hfront_porch;
	uint16_t	hsync_len;
	uint16_t	vback_porch;
	uint16_t	vfront_porch;
	uint16_t	vsync_len;
	uint32_t	flags;		/* enum modesetting_flags mask */
} __attribute__((__packed__));

/*
 * For structure documentation, see:
 * http://www.kosagi.com/w/index.php?title=Novena/EEPROM
 */
struct novena_eeprom_data {
	uint8_t			signature[6];	/* 'Novena' */
	uint8_t			version;	/* always 2 */
	uint8_t			reserved1;
	uint32_t		serial;		/* 32-bit serial number */
	uint8_t			mac[6];		/* Gigabit MAC address */

	/* Features present, from struct feature features[] below */
	uint16_t		features;	/* Native byte order */

	/* Describes default resolutions of various output devices */
	struct modesetting	lvds1;
	struct modesetting	lvds2;
	struct modesetting	hdmi;

	/* An indicator of how large this particular EEPROM is */
	uint32_t		eeprom_size;

	/* If eepromoops is present, describes eepromoops storage */
	uint32_t		eepromoops_offset;
	uint32_t		eepromoops_length;
} __attribute__((__packed__));



struct feature {
        uint32_t        flags;
        char            *name;
	char		*descr;
} features[] = {
	{
		.name  = "es8328",
		.flags = 0x0001,
		.descr = "ES8328 audio codec",
	},
	{
		.name  = "senoko",
		.flags = 0x0002,
		.descr = "Senoko battery board",
	},
	{
		.name  = "retina",
		.flags = 0x0004,
		.descr = "Retina-class dual-LVDS display (deprecated)",
	},
	{
		.name  = "pixelqi",
		.flags = 0x0008,
		.descr = "PixelQi LVDS display (deprecated)",
	},
	{
		.name  = "pcie",
		.flags = 0x0010,
		.descr = "PCI Express support",
	},
	{
		.name  = "gbit",
		.flags = 0x0020,
		.descr = "Gigabit Ethernet",
	},
	{
		.name  = "hdmi",
		.flags = 0x0040,
		.descr = "HDMI Output (deprecated)",
	},
	{
		.name  = "eepromoops",
		.flags = 0x0080,
		.descr = "EEPROM Oops storage",
	},
	{} /* Sentinal */
};


#endif /* __NOVENA_EEPROM_H__ */
