#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <string.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/i2c-dev.h>
#include <ctype.h>

#include "novena-eeprom.h"

#define EEPROM_ADDRESS (0xac>>1)
#define I2C_BUS "/dev/i2c-2"

union novena_eeprom_data {
	struct novena_eeprom_data_v1	v1;
	struct novena_eeprom_data_v2	v2;
};

struct eeprom_dev {
	/* File handle to I2C bus */
	int				fd;

	/* I2C address of the EEPROM */
	int				addr;

	/* True, if we've read the contents of eeprom */
	int				cached;

	/* Contents of the EEPROM */
	union novena_eeprom_data 	data;
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
	char set_addr_buf[2];

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

int eeprom_write_i2c(struct eeprom_dev *dev, int addr, const void *data, int count) {
	struct i2c_rdwr_ioctl_data session;
	struct i2c_msg messages[1];
	char data_buf[2+count];

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
	const char *buffer = (const char *)&dev->data;
	unsigned int buffer_offset = 0;
	int page_size = dev->data.v2.page_size;
	int ret;

	dev->cached = 1;
	while (buffer_offset < sizeof(dev->data)) {
		if ((buffer_offset + page_size) > sizeof(dev->data))
			page_size = sizeof(dev->data) - buffer_offset;

		ret = eeprom_write_i2c(dev, buffer_offset,
				       buffer + buffer_offset, page_size);
		if (ret)
			break;

		buffer_offset += page_size;
		usleep(10000);
	}

	return ret;
}

static int eeprom_export(struct eeprom_dev *dev, const char *filename) {
	FILE *f;
	int ret;

	/* Ensure we have a cached copy */
	if (eeprom_read(dev) != 0)
		return 1;

	f = fopen(filename, "w");
	if (NULL == f) {
		perror("Unable to open file for exporting");
		return 1;
	}

	ret = fwrite(&dev->data, sizeof(dev->data), 1, f);
	if (ret != 1 {
		perror("Unable to export");
		fclose(f);
		return 1;
	}

	fclose(f);
	return 0;
}

static int eeprom_import(struct eeprom_dev *dev, const char *filename) {
	FILE *f;
	int ret;

	f = fopen(filename, "r");
	if (NULL == f) {
		perror("Unable to open file for importing");
		return 1;
	}

	ret = fread(&dev->data, sizeof(dev->data), 1, f);
	if (ret != 1 {
		perror("Unable to import");
		fclose(f);
		return 1;
	}

	fclose(f);

	/* Mark the copy as cached, so it won't get re-read */
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

static void eeprom_get_defaults(struct eeprom_dev *dev) {
	memset(&dev->data.v2, 0, sizeof(dev->data.v2));

	memcpy(dev->data.v2.signature, NOVENA_SIGNATURE, sizeof(dev->data.v2.signature));

	memset(&dev->data.v2.mac, 0xff, sizeof(dev->data.v2.mac));

	dev->data.v2.version = 2;

	dev->data.v2.features = feature_es8328 | feature_pcie | feature_gbit |
		     feature_hdmi | feature_retina | feature_eepromoops;

	dev->data.v2.eepromoops_offset = 4096;
	dev->data.v2.eepromoops_length = 61440;

	dev->data.v2.eeprom_size = 65536;
	dev->data.v2.page_size = 128;

	dev->data.v2.lvds1.frequency = 148500000;
	dev->data.v2.lvds1.hactive = 1920;
	dev->data.v2.lvds1.vactive = 1080;
	dev->data.v2.lvds1.hback_porch = 148;
	dev->data.v2.lvds1.hfront_porch = 88;
	dev->data.v2.lvds1.hsync_len = 44;
	dev->data.v2.lvds1.vback_porch = 36;
	dev->data.v2.lvds1.vfront_porch = 4;
	dev->data.v2.lvds1.vsync_len = 5;
	dev->data.v2.lvds1.flags = vsync_polarity | hsync_polarity | data_width_8bit
		      | mapping_jeida | dual_channel | channel_present;

	dev->data.v2.lvds2.flags = channel_present;

	/* Pull HDMI settings from e.g. EDID */
	dev->data.v2.hdmi.flags = channel_present | ignore_settings | data_width_8bit;
}

static void eeprom_upgrade_v1_to_v2(struct eeprom_dev *dev) {
	if (dev->data.v2.version != 1)
		return;

        dev->data.v2.features |= feature_eepromoops;

        dev->data.v2.eepromoops_offset = 4096;
        dev->data.v2.eepromoops_length = 61440;

        dev->data.v2.eeprom_size = 65536;
        dev->data.v2.page_size = 128;

        if (dev->data.v2.features & feature_retina) {
            dev->data.v2.lvds1.frequency = 148500000;
            dev->data.v2.lvds1.hactive = 1920;
            dev->data.v2.lvds1.vactive = 1080;
            dev->data.v2.lvds1.hback_porch = 148;
            dev->data.v2.lvds1.hfront_porch = 88;
            dev->data.v2.lvds1.hsync_len = 44;
            dev->data.v2.lvds1.vback_porch = 36;
            dev->data.v2.lvds1.vfront_porch = 4;
            dev->data.v2.lvds1.vsync_len = 5;
            dev->data.v2.lvds1.flags = vsync_polarity | hsync_polarity
				     | data_width_8bit | mapping_jeida
				     | dual_channel | channel_present;

            dev->data.v2.lvds2.flags = channel_present;
        }

        if (dev->data.v2.features & feature_hdmi) {
		/* Pull HDMI settings from e.g. EDID */
		dev->data.v2.hdmi.flags = channel_present | ignore_settings
					| data_width_8bit;
        }

        dev->data.v2.version = 2;
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
	"    -o    Specify the EEPROM Oops start and size (e.g. -o 1000,3000)\n"
	"    -p    EEPROM page size\n"
	"    -l    EEPROM total size (length)\n"
	"    -1    LVDS channel 1 modeline\n"
	"    -2    LVDS channel 2 modeline\n"
	"    -d    HDMI modeline\n"
	"    -w    Actually write the value to the EEPROM\n"
	"    -e    Export EEPROM to file\n"
	"    -i    Import EEPROM from file\n"
	"    -h    Print this help message\n"
	"\n", name);

	printf("Valid features:\n");
	struct feature *feature = features;
	while (feature->name) {
		printf("    %-12s%s\n", feature->name, feature->descr);
		feature++;
	}
	printf("\n");

	printf("Modelines should be specified entirely in quotes.  Flags come at the end. You\n");
	printf("may specify positive polarity with either the modesetting convention,\n");
	printf("or as a flag.  E.g. either +HSync or hsync_polarity.  For example:\n");
	printf("\n");
	printf("    -1 'Modeline \"lvds1\" 148.500  1920 2068 2156 2200   1080 1116 1120 1125 +HSync +VSync channel_present dual_channel mapping_jeida data_width_8bit'\n");
	printf("\n");

	printf("Valid modeline flags:\n");
	struct available_modesetting_flags *ms = available_modesetting_flags;
	while (ms->name) {
		printf("    %-25s%s\n", ms->name, ms->descr);
		ms++;
	}
	printf("\n");

	printf("Example:\n"
		"  %s -f es8328,retina -s 12345 -w\n"
		"", name);
	return 0;
}

static void print_modesetting(struct modesetting *m, const char *name) {
	printf("\t\tModeline \"%s\" %0.3f  %d %d %d %d   %d %d %d %d %cHSync %cVSync\n",
		name,
		m->frequency / 1000000.0,
		m->hactive,
		m->hactive + m->hback_porch,
		m->hactive + m->hback_porch + m->hfront_porch,
		m->hactive + m->hback_porch + m->hfront_porch + m->hsync_len,
		m->vactive,
		m->vactive + m->vback_porch,
		m->vactive + m->vback_porch + m->vfront_porch,
		m->vactive + m->vback_porch + m->vfront_porch + m->vsync_len,
		(m->flags & hsync_polarity) ? '+' : '-',
		(m->flags & vsync_polarity) ? '+' : '-');

	printf("\t\tFlags: 0x%x", m->flags);
	if (m->flags) {
		int matched = 0;
		int flags = m->flags;
		struct available_modesetting_flags *feature = available_modesetting_flags;
		while (feature->name) {
			if (feature->flags & flags) {
				if (!matched)
					printf(" (%s", feature->name);
				else
					printf(",%s", feature->name);
				matched++;
				flags &= ~feature->flags;
			}
			feature++;
		}
		if (matched)
			printf(")");
		if (flags)
			printf(" Unrecognized flags: 0x%02x", flags);
	}
	printf("\n");
}

int print_eeprom_data(struct eeprom_dev *dev) {
	int ret;
	ret = eeprom_read(dev);
	if (ret)
		return ret;

	printf("\tSignature:        %c%c%c%c%c%c\n",
			dev->data.v1.signature[0],
			dev->data.v1.signature[1],
			dev->data.v1.signature[2],
			dev->data.v1.signature[3],
			dev->data.v1.signature[4],
			dev->data.v1.signature[5]);
	printf("\tVersion:          %d\n", dev->data.v1.version);
	printf("\tSerial:           %d\n", dev->data.v1.serial);
	printf("\tMAC:              %02x:%02x:%02x:%02x:%02x:%02x\n",
			dev->data.v1.mac[0], dev->data.v1.mac[1],
			dev->data.v1.mac[2], dev->data.v1.mac[3],
			dev->data.v1.mac[4], dev->data.v1.mac[5]);
	printf("\tFeatures:         0x%x", dev->data.v1.features);
	if (dev->data.v1.features) {
		int matched = 0;
		int flags = dev->data.v1.features;
		struct feature *feature = features;
		while (feature->name) {
			if (feature->flags & flags) {
				if (!matched)
					printf(" (%s", feature->name);
				else
					printf(",%s", feature->name);
				matched++;
				flags &= ~feature->flags;
			}
			feature++;
		}
		if (matched)
			printf(")");
		if (flags)
			printf(" Unrecognized flags: 0x%02x", flags);
	}
	printf("\n");

	if (dev->data.v2.version == 2) {
		struct novena_eeprom_data_v2 *v2 = &dev->data.v2;

		printf("\tEEPROM size:      %d\n", dev->data.v2.eeprom_size);
		printf("\tEEPROM page size: %d\n", dev->data.v2.page_size);
		printf("\tOops offset:      %d\n",
					dev->data.v2.eepromoops_offset);
		printf("\tOops length:      %d\n",
					dev->data.v2.eepromoops_length);

		printf("\tLVDS channel 1:\n");
		print_modesetting(&v2->lvds1, "lvds1");
		printf("\tLVDS channel 2:\n");
		print_modesetting(&v2->lvds2, "lvds2");
		printf("\tHDMI channel:\n");
		print_modesetting(&v2->hdmi, "hdmi");
	}
	return 0;
}

static int parse_modesetting(struct modesetting *m, const char *arg) {
	int len;
	float mhz;
	uint32_t h1, h2, h3, h4;
	uint32_t v1, v2, v3, v4;
	
	sscanf(arg, "%*s %*s %f %u %u %u %u %u %u %u %d %n",
			&mhz, &h1, &h2, &h3, &h4, &v1, &v2, &v3, &v4, &len);

	m->frequency = mhz * 1000000;

	m->hactive = h1;
	m->hback_porch = h2 - m->hactive;
	m->hfront_porch = h3 - m->hactive - m->hback_porch;
	m->hsync_len = h4 - m->hactive - m->hback_porch - m->hfront_porch;

	m->vactive = v1;
	m->vback_porch = v2 - m->vactive;
	m->vfront_porch = v3 - m->vactive - m->vback_porch;
	m->vsync_len = v4 - m->vactive - m->vback_porch - m->vfront_porch;

	m->flags = 0;

	char flagstr[strlen(arg + len) + 1];
	char *flag;
	char *tmp = flagstr;

	memcpy(flagstr, arg + len, sizeof(flagstr));

	while ((flag = strtok(tmp, " ")) != NULL) {
		tmp = NULL;

		if (!strcasecmp(flag, "+hsync"))
			m->flags |= hsync_polarity;
		else if (!strcasecmp(flag, "-hsync"))
			m->flags &= ~hsync_polarity;
		if (!strcasecmp(flag, "+vsync"))
			m->flags |= vsync_polarity;
		else if (!strcasecmp(flag, "-vsync"))
			m->flags &= ~vsync_polarity;
		else {
			struct available_modesetting_flags *feature = available_modesetting_flags;
			while (feature->name) {
				if (!strcasecmp(feature->name, flag)) {
					m->flags |= feature->flags;
					break;
				}
				feature++;
			}
		}

	}

	return 0;
}

int main(int argc, char **argv) {
	struct eeprom_dev *dev;
	int ch;
	int writing = 0;
	char *tmp;

	struct novena_eeprom_data_v2 newrom;

	int newdata = 0;
	int update_mac = 0;
	int update_features = 0;
	int update_serial = 0;
	int update_oops_start = 0;
	int update_oops_length = 0;
	int update_page_size = 0;
	int update_total_size = 0;
	int update_lvds1 = 0;
	int update_lvds2 = 0;
	int update_hdmi = 0;

	dev = eeprom_open(I2C_BUS, EEPROM_ADDRESS);
	if (!dev)
		return 1;

	while ((ch = getopt(argc, argv, "hm:s:f:wo:p:l:1:2:d:e:i:")) != -1) {
		switch(ch) {

		/* MAC address */
		case 'm':
			if (parse_mac(optarg, newrom.mac))
				return 1;
			update_mac = 1;
			break;

		/* Serial number */
		case 's':
			newrom.serial = strtoul(optarg, NULL, 0);
			update_serial = 1;
			break;

		/* Featuresset */
		case 'f':
			newrom.features = parse_features(optarg);
			if (newrom.features == -1)
				return 1;
			update_features = 1;
			break;

		case 'o':
			newrom.eepromoops_offset = strtoul(optarg, &tmp, 0);
			update_oops_start = 1;
			if (tmp && *tmp) {
				newrom.eepromoops_length = strtoul(tmp + 1,
								    NULL, 0);
				update_oops_length = 1;
			}
			break;

		case 'p':
			newrom.page_size = strtoul(optarg, NULL, 0);
			update_page_size = 1;
			break;

		case 'l':
			newrom.eeprom_size = strtoul(optarg, NULL, 0);
			update_total_size = 1;
			break;

		case '1':
			if (parse_modesetting(&newrom.lvds1, optarg))
				return 1;
			update_lvds1 = 1;
			break;

		case '2':
			if (parse_modesetting(&newrom.lvds2, optarg))
				return 1;
			update_lvds2 = 1;
			break;

		case 'd':
			if (parse_modesetting(&newrom.hdmi, optarg))
				return 1;
			update_hdmi = 1;
			break;

		case 'e':
			return eeprom_export(dev, optarg);

		case 'i':
			if (eeprom_import(dev, optarg))
				return 1;
			newdata = 1;
			break;

		/* Write data */
		case 'w':
			writing = 1;
			break;

		case 'h':
			print_usage(argv[0]);
			return 1;

		default:
			printf("Unrecognized option: %c\n", ch);
			print_usage(argv[0]);
			return 1;
		}
	}

	argc -= optind;
	argv += optind;

	if (update_mac || update_serial || update_features ||
		update_oops_start || update_oops_length ||
		update_page_size || update_total_size ||
		update_lvds1 || update_lvds2 || update_hdmi)
		newdata = 1;

	if (argc)
		print_usage(argv[0]);
	else if (!writing) {
		if (newdata)
			printf("Not writing data, as -w was not specified\n");
		printf("Current EEPROM settings:\n");
		print_eeprom_data(dev);
	}
	else {
		int ret;
		ret = eeprom_read(dev);
		if (ret)
			return 1;
		if (dev->data.v1.version == 1) {
			printf("Updating v1 EEPROM to v2...\n");
			eeprom_upgrade_v1_to_v2(dev);
		}
		else if (dev->data.v1.version == 2) {
			/* Ignore v2 */;
		}
		else {
			if (memcmp(dev->data.v2.signature, NOVENA_SIGNATURE,
					sizeof(dev->data.v2.signature)))
				printf("Blank EEPROM found, "
					"setting defaults...\n");
			else
				fprintf(stderr,
					"Unrecognized EEPROM version found "
					"(v%d), overwriting with v2\n",
					dev->data.v1.version);
			eeprom_get_defaults(dev);
		}

		if (update_mac)
			memcpy(&dev->data.v2.mac, newrom.mac, sizeof(newrom.mac));
		if (update_serial)
			dev->data.v2.serial = newrom.serial;
		if (update_features)
			dev->data.v2.features = newrom.features;
		if (update_oops_start)
			dev->data.v2.eepromoops_offset = newrom.eepromoops_offset;
		if (update_oops_length)
			dev->data.v2.eepromoops_length = newrom.eepromoops_length;
		if (update_page_size)
			dev->data.v2.page_size = newrom.page_size;
		if (update_total_size)
			dev->data.v2.eeprom_size = newrom.eeprom_size;
		if (update_lvds1)
			memcpy(&dev->data.v2.lvds1, &newrom.lvds1,
				sizeof(dev->data.v2.lvds1));
		if (update_lvds2)
			memcpy(&dev->data.v2.lvds2, &newrom.lvds2,
				sizeof(dev->data.v2.lvds2));
		if (update_hdmi)
			memcpy(&dev->data.v2.hdmi, &newrom.hdmi,
				sizeof(dev->data.v2.hdmi));
		memcpy(&dev->data.v2.signature,
				NOVENA_SIGNATURE,
				sizeof(dev->data.v2.signature));

		dev->data.v2.version = 2;

		ret = eeprom_write(dev);
		if (ret) {
			printf("EEPROM write failed\n");
			return 1;
		}

		printf("Updated EEPROM.  New values:\n");
		print_eeprom_data(dev);
	}

	eeprom_close(&dev);

	return 0;
}
