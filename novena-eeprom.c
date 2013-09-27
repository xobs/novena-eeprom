#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>

#include "novena-eeprom.h"

#define EEPROM_ADDRESS (0xac>>1)
#define I2C_BUS "/dev/i2c-2"

struct eeprom_dev {
	/* File handle to I2C bus */
	int				fd;

	/* I2C address of the EEPROM */
	int				addr;

	/* True, if we've read the contents of eeprom */
	int				cached;

	/* Contents of the EEPROM */
	struct novena_eeprom_data 	data;
};

int eeprom_read_i2c(struct eeprom_dev *dev, int addr, void *data, int count) {
	struct i2c_rdwr_ioctl_data session;
	struct i2c_msg messages[2];
	uint8_t set_addr_buf[2];

	memset(set_addr_buf, 0, sizeof(set_addr_buf));
	memset(data, 0, count);

	set_addr_buf[0] = addr>>8;
	set_addr_buf[1] = addr;

	messages[0].addr = dev->addr;
	messages[0].flags = 0;
	messages[0].len = sizeof(set_addr_buf);
	messages[0].buf = set_addr_buf;

	messages[1].addr = dev->addr;
	messages[1].flags = I2C_M_RD;
	messages[1].len = count;
	messages[1].buf = data;

	session.msgs = messages;
	session.nmsgs = 2;

	if(ioctl(dev->fd, I2C_RDWR, &session) < 0) {
		perror("Unable to communicate with i2c device");
		return 1;
	}

	return 0;
}

int eeprom_read(struct eeprom_dev *dev) {
	int ret;

	if (dev->cached)
		return 0;

	ret = eeprom_read_i2c(dev, 0, &dev->data, sizeof(dev->data));
	if (ret)
		return ret;

	dev->cached = 1;
	return 0;
}

struct eeprom_dev *eeprom_open(char *path, int addr) {
	struct eeprom_dev *dev;

	dev = malloc(sizeof(*dev));
	if (!dev) {
		perror("Unable to alloc data");
		goto malloc_err;
	}

	memset(dev, 0, sizeof(*dev));

	dev->fd = open(path, O_RDWR);
	if (dev->fd == -1) {
		perror("Unable to open i2c device");
		goto open_err;
	}

	dev->addr = addr;

	return dev;

open_err:
	free(dev);
malloc_err:
	return NULL;
}

int eeprom_close(struct eeprom_dev **dev) {
	if (!dev || !*dev)
		return 0;
	close((*dev)->fd);
	free(*dev);
	*dev = NULL;
	return 0;
}

int main(int argc, char **argv) {
	struct eeprom_dev *dev;
	int ret;

	dev = eeprom_open(I2C_BUS, EEPROM_ADDRESS);
	if (!dev)
		return 1;

	if (argc == 1) {
		ret = eeprom_read(dev);
		if (ret)
			goto quit;

		printf("Current EEPROM settings:\n");
		printf("\tSignature:   %c%c%c%c%c%c\n",
				dev->data.signature[0],
				dev->data.signature[1],
				dev->data.signature[2],
				dev->data.signature[3],
				dev->data.signature[4],
				dev->data.signature[5]);
		printf("\tVersion:     %d\n", dev->data.version);
		printf("\tSerial:      %d\n", dev->data.serial);
		printf("\tMAC:         %02x:%02x:%02x:%02x:%02x:%02x\n",
				dev->data.mac[0], dev->data.mac[1],
				dev->data.mac[2], dev->data.mac[3],
				dev->data.mac[4], dev->data.mac[5]);
		printf("\tFeatures:    0x%x\n", dev->data.features);
	}

quit:
	eeprom_close(&dev);

	return 0;
}
