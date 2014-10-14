#ifndef __NOVENA_EEPROM_H__
#define __NOVENA_EEPROM_H__

#include <stdint.h>

#define NOVENA_SIGNATURE "Novena"

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

	/* Channel is present, but ignore EEPROM values and auto-detect them */
	ignore_settings	= 0x40,
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

#ifndef __cplusplus	/* C++ doesn't support named assignment */
struct available_modesetting_flags {
	uint32_t	flags;
	char		*name;
	char		*descr;
} available_modesetting_flags[] = {
	{
		.name	= "channel_present",
		.flags	= channel_present,
		.descr	= "This channel is present",
	},
	{
		.name	= "dual_channel",
		.flags	= dual_channel,
		.descr	= "Channel is dual-lane",
	},
	{
		.name	= "vsync_polarity",
		.flags	= vsync_polarity,
		.descr	= "VSync polarity is positive",
	},
	{
		.name	= "hsync_polarity",
		.flags	= hsync_polarity,
		.descr	= "HSync polarity is positive",
	},
	{
		.name	= "mapping_jeida",
		.flags	= mapping_jeida,
		.descr	= "Use JEIDA (as opposed to PSWG) mapping",
	},
	{
		.name	= "data_width_8bit",
		.flags	= data_width_8bit,
		.descr	= "Use 8-bit (as opposed to 6 [LVDS] or 10 [HDMI] bit)",
	},
	{
		.name	= "ignore_settings",
		.flags	= ignore_settings,
		.descr	= "Ignore settings and attempt to auto-detect",
	},
	{} /* Sentinal */
};
#endif /* ! __cplusplus */

enum feature_flags {
	feature_es8328 		= 0x0001,
	feature_senoko		= 0x0002,
	feature_retina		= 0x0004,
	feature_pixelqi		= 0x0008,
	feature_pcie		= 0x0010,
	feature_gbit		= 0x0020,
	feature_hdmi		= 0x0040,
	feature_eepromoops	= 0x0080,
	feature_rootsrc_sata	= 0x0100,
};

#ifndef __cplusplus	/* C++ doesn't support named assignment */
struct feature {
	uint32_t	flags;
	char		*name;
	char		*descr;
} features[] = {
	{
		.name	= "es8328",
		.flags	= feature_es8328,
		.descr	= "ES8328 audio codec",
	},
	{
		.name	= "senoko",
		.flags	= feature_senoko,
		.descr	= "Senoko battery board",
	},
	{
		.name	= "edp",
		.flags	= feature_retina,
		.descr	= "eDP bridge chip",
	},
	{
		.name	= "pixelqi",
		.flags	= feature_pixelqi,
		.descr	= "PixelQi LVDS display (deprecated)",
	},
	{
		.name	= "pcie",
		.flags	= feature_pcie,
		.descr	= "PCI Express support",
	},
	{
		.name	= "gbit",
		.flags	= feature_gbit,
		.descr	= "Gigabit Ethernet",
	},
	{
		.name	= "hdmi",
		.flags	= feature_hdmi,
		.descr	= "HDMI Output (deprecated)",
	},
	{
		.name	= "eepromoops",
		.flags	= feature_eepromoops,
		.descr	= "EEPROM Oops storage",
	},
	{
		.name	= "sataroot",
		.flags	= feature_rootsrc_sata,
		.descr	= "Root device is SATA",
	},
	{} /* Sentinal */
};
#endif /* ! __cplusplus */

/*
 * For structure documentation, see:
 * http://www.kosagi.com/w/index.php?title=Novena/EEPROM
 */
struct novena_eeprom_data_v1 {
	uint8_t		signature[6];	/* 'Novena' */
	uint8_t		version;	/* always 1 */
	uint8_t		reserved;
	uint32_t	serial;		/* 32-bit serial number */
	uint8_t		mac[6];		/* Gigabit MAC address */

	/* Features present, from struct feature features[] below */
	uint16_t	features;	/* Native byte order */
} __attribute__((__packed__));

/* V1 is mostly a superset of v2, except the page size is a "reserved" field */
struct novena_eeprom_data_v2 {
	uint8_t		signature[6];	/* 'Novena' */
	uint8_t		version;	/* always 2 */
	uint8_t		page_size;	/* Size of EEPROM read/write page */
	uint32_t	serial;		/* 32-bit serial number */
	uint8_t		mac[6];		/* Gigabit MAC address */

	/* Features present, from struct feature features[] below */
	uint16_t	features;	/* Native byte order */

	/* Describes default resolutions of various output devices */
	struct modesetting	lvds1;	/* LVDS channel 1 settings */
	struct modesetting	lvds2;	/* LVDS channel 2 settings */
	struct modesetting	hdmi;	/* HDMI settings */

	/* An indicator of how large this particular EEPROM is */
	uint32_t	eeprom_size;

	/* If eepromoops is present, describes eepromoops storage */
	uint32_t	eepromoops_offset;
	uint32_t	eepromoops_length;
} __attribute__((__packed__));

#endif /* __NOVENA_EEPROM_H__ */
