#ifndef __NOVENA_EEPROM_H__
#define __NOVENA_EEPROM_H__

#include <stdint.h>

#define NOVENA_SIGNATURE "Novena"
#define NOVENA_VERSION 1


/* Bitmask polarities used as flag */
#define CHANNEL_PRESENT (1 << 0)
#define DUAL_CHANNEL    (1 << 1)
#define VSYNC_POLARITY  (1 << 2)
#define HSYNC_POLARITY  (1 << 3)
#define MAPPING_JEIDA   (1 << 4) /* If not JEIDA, then PSWG/SPWG */
#define DATA_WIDTH_8    (1 << 5) /* If not 8-bit (24 bpp), then 6-bit (18 bpp) */

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
	uint32_t	flags;
} __attribute__((__packed__));

/*
 * For structure documentation, see:
 * http://www.kosagi.com/w/index.php?title=Novena/EEPROM
 */
struct novena_eeprom_data {
	uint8_t			signature[6];	/* 'Novena' */
	uint8_t			version;	/* always 1 */
	uint8_t			reserved1;
	uint32_t		serial;
	uint8_t			mac[6];
	uint16_t		features;
	struct modesetting	lvds1;
	struct modesetting	lvds2;
	struct modesetting	hdmi;
} __attribute__((__packed__));

struct feature {
        uint32_t        flags;
        char            *name;
	char		*descr;
};

struct feature features[] = {
	{
		.name  = "es8328",
		.flags = 0x01,
		.descr = "ES8328 audio codec",
	},
	{
		.name  = "senoko",
		.flags = 0x02,
		.descr = "Senoko battery board",
	},
	{
		.name  = "retina",
		.flags = 0x04,
		.descr = "Retina-class dual-LVDS display",
	},
	{
		.name  = "pixelqi",
		.flags = 0x08,
		.descr = "PixelQi LVDS display",
	},
	{
		.name  = "pcie",
		.flags = 0x10,
		.descr = "PCI Express support",
	},
	{
		.name  = "gbit",
		.flags = 0x20,
		.descr = "Gigabit Ethernet",
	},
	{
		.name  = "hdmi",
		.flags = 0x40,
		.descr = "HDMI Output",
	},
	{} /* Sentinal */
};


#endif /* __NOVENA_EEPROM_H__ */
