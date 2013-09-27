#ifndef __NOVENA_EEPROM_H__
#define __NOVENA_EEPROM_H__

#include <stdint.h>

/*
 * For structure documentation, see:
 * http://www.kosagi.com/w/index.php?title=Novena/EEPROM
 */
struct novena_eeprom_data {
	uint8_t		signature[6];	/* 'Novena' */
	uint8_t		version;	/* 1 */
	uint8_t		reserved1;
	uint32_t	serial;
	uint8_t		mac[6];
	uint16_t	features;
} __attribute__((__packed__));

#endif /* __NOVENA_EEPROM_H__ */
