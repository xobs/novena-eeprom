#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <ctype.h>

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

int parse_features(char *str) {
	char *ctx;
	char *sep = ",";
	char *word;
	uint16_t flags = 0;

	for (word = strtok_r(str, sep, &ctx);
	     word;
	     word = strtok_r(NULL, sep, &ctx)) {

		struct feature *feature = features;
		while (feature->name) {
			if (!strcmp(feature->name, word)) {
				flags |= feature->flags;
				break;
			}
			feature++;
		}
		if (!feature->name) {
			fprintf(stderr, "Unrecognized feature \"%s\"\n", word);
			return -1;
		}
	}
	return flags;
}

int parse_mac(char *str, void *out) {
	int i;
	char *mac = out;
	for (i=0; i<6; i++) {
		if (!isdigit(str[0]) &&
			(tolower(str[0]) < 'a' || tolower(str[0]) > 'f')) {
			printf("Unable to parse MAC address\n");
			return 1;
		}
		if (!isdigit(str[1]) &&
			(tolower(str[1]) < 'a' || tolower(str[1]) > 'f')) {
			printf("Unable to parse MAC address\n");
			return 1;
		}

		*mac = strtoul(str, NULL, 16);
		mac++;
		str+=2;
		if (*str == '-' || *str == ':' || *str == '.')
			str++;
	}
	if (*str) {
		printf("Unable to parse MAC address\n");
		return 1;
	}
	return 0;
}

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

int eeprom_write_i2c(struct eeprom_dev *dev, int addr, void *data, int count) {
	struct i2c_rdwr_ioctl_data session;
	struct i2c_msg messages[1];
	uint8_t data_buf[2+count];

	data_buf[0] = addr>>8;
	data_buf[1] = addr;
	memcpy(&data_buf[2], data, count);

	messages[0].addr = dev->addr;
	messages[0].flags = 0;
	messages[0].len = sizeof(data_buf);
	messages[0].buf = data_buf;

	session.msgs = messages;
	session.nmsgs = 1;

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

int eeprom_write(struct eeprom_dev *dev) {
	dev->cached = 1;
	return eeprom_write_i2c(dev, 0, &dev->data, sizeof(dev->data));
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

int print_usage(char *name) {
	printf("Usage:\n"
	"  %s [-m 'xx:xx:xx:xx:xx:xx'] [-s 'serial'] [-f features] [-w]\n"
	"\n"
	"If no arguments are specified, then the current EEPROM contents\n"
	"will be read and printed.\n"
	"\n"
	"Specify -w to write a new EEPROM value.  Any unspecified fields\n"
	"will be set to 0.\n"
	"\n"
	"    -m    Specify a MAC address for the Gigabit Ethernet\n"
	"    -s    Specify the device's serial number\n"
	"    -f    A comma-delimited list of features present\n"
	"    -w    Actually write the value to the EEPROM\n"
	"\n", name);

	printf("Valid features:\n");
	struct feature *feature = features;
	while (feature->name) {
		printf("    %-12s%s\n", feature->name, feature->descr);
		feature++;
	}
	printf("\n");

	printf("Example:\n"
		"  %s -f es8328,retina -s 12345 -w\n"
		"", name);
	return 0;
}

int print_eeprom_data(struct eeprom_dev *dev) {
	int ret;
	ret = eeprom_read(dev);
	if (ret)
		return ret;

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
	printf("\tFeatures:    0x%x", dev->data.features);
	if (dev->data.features) {
		int matched = 0;
		int flags = dev->data.features;
		struct feature *feature = features;
		while (feature->name) {
			if (feature->flags & flags) {
				if (!matched)
					printf(" (%s", feature->name);
				else
					printf(", %s", feature->name);
				matched++;
				flags &= ~feature->flags;
			}
			feature++;
		}
		if (matched)
			printf(")");
		if (flags)
			printf(" Unrecognized flags: 0x%02x", flags);
		printf("\n");
	}
	return 0;
}

int main(int argc, char **argv) {
	struct eeprom_dev *dev;
	int ch;
	int writing = 0;
	int features;
	int32_t serial;
	uint8_t new_mac[6];

	int update_mac = 0;
	int update_features = 0;
	int update_serial = 0;

	dev = eeprom_open(I2C_BUS, EEPROM_ADDRESS);
	if (!dev)
		return 1;

	while ((ch = getopt(argc, argv, "m:s:f:w")) != -1) {
		switch(ch) {

		/* MAC address */
		case 'm':
			if (parse_mac(optarg, new_mac))
				return 1;
			update_mac = 1;
			break;

		/* Serial number */
		case 's':
			serial = strtoul(optarg, NULL, 0);
			update_serial = 1;
			break;

		/* Featuresset */
		case 'f':
			features = parse_features(optarg);
			if (features == -1)
				return 1;
			update_features = 1;
			break;

		/* Write data */
		case 'w':
			writing = 1;
			break;

		default:
			printf("Unrecognized option: %c\n", ch);
			print_usage(argv[0]);
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	if (argc)
		print_usage(argv[0]);
	else if (!writing) {
		if (update_mac || update_serial || update_features)
			printf("Not writing data, as -w was not specified\n");
		print_eeprom_data(dev);
	}
	else {
		int ret;
		ret = eeprom_read(dev);
		if (ret)
			return 1;
		if (update_mac)
			memcpy(&dev->data.mac, new_mac, sizeof(new_mac));
		if (update_serial)
			dev->data.serial = serial;
		if (update_features)
			dev->data.features = features;
		memcpy(&dev->data.signature, NOVENA_SIGNATURE, sizeof(dev->data.signature));
		dev->data.version = NOVENA_VERSION;

		ret = eeprom_write(dev);
		if (ret) {
			printf("EEPROM write failed\n");
			return 1;
		}

		printf("Updated EEPROM\n");
	}

	eeprom_close(&dev);

	return 0;
}
