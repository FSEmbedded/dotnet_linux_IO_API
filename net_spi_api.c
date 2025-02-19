#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h> // string function definitions
#include <stdint.h>
#include <stdlib.h>
#include <linux/spi/spidev.h>

#include "net_spi_api.h"

/* Windows IOCTL definitions */
#define METHOD_BUFFERED  0
#define METHOD_IN_DIRECT  1
#define METHOD_OUT_DIRECT  2
#define METHOD_NEITHER  3

#define FILE_ANY_ACCESS  (0 << 14)
#define FILE_READ_ACCESS  (1 << 14)   // file & pipe
#define FILE_WRITE_ACCESS  (2 << 14)   // file & pipe

#define DEVICE_NSPI (0x0000800AU << 16)

#define IOCTL_NSPI_SEND     (DEVICE_NSPI | FILE_ANY_ACCESS | (0x800 << 2) | METHOD_BUFFERED)
#define IOCTL_NSPI_RECEIVE  (DEVICE_NSPI | FILE_ANY_ACCESS | (0x801 << 2) | METHOD_BUFFERED)
#define IOCTL_NSPI_TRANSFER (DEVICE_NSPI | FILE_ANY_ACCESS | (0x802 << 2) | METHOD_BUFFERED)
#define IOCTL_NSPI_EXCHANGE (DEVICE_NSPI | FILE_ANY_ACCESS | (0x803 << 2) | METHOD_BUFFERED)

#define NSPI_DEFAULT_BITS  "8"
#define NSPI_DEFAULT_SPEED "100000"
#define NSPI_DEFAULT_DELAY  "0"
#define NSPI_DEFAULT_MODE   "0"

static long speed;
static long bits;
static long delay;

/*****************************************************************************
 *** Function:    void transfer(int fd)                                     ***
 ***                                                                        ***
 *** Parameters:  fd: filedescriptor                                        ***
 ***                                                                        ***
 *** Return:      0 if sending and receive are the same, -1 if sending or   ***
 ***              receive failed                                            ***
 ***                                                                        ***
 *** Description                                                            ***
 *** -----------                                                            ***
 *** Write and read function of the spi device                              ***
 *****************************************************************************/
static int transfer(int fd, uint8_t bits, uint32_t speed, uint16_t delay,
		uint8_t *tx, int tx_len, uint8_t *rx, int rx_len) {
	int ret = 1;
	uint8_t *rx_buf;

	int len;

	len = tx_len;

	/* initialize buffer with zero */
	memset(rx, 0, len);

	struct spi_ioc_transfer tr = { .tx_buf = (unsigned long) tx, .rx_buf =
			(unsigned long) rx, .len = len, .delay_usecs = delay, .speed_hz =
			speed, .bits_per_word = bits, };

	/* case tx = cmd + send_data  */

	if (tx_len > rx_len) {
		rx_buf = malloc(len);
		tr.rx_buf = (unsigned long) rx_buf;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1) {
		printf("\ncan't send spi message\n");
	}

	if (tx_len > rx_len) {
		int offset = tx_len - rx_len;
		memcpy(rx, rx_buf + offset, rx_len);
		free(rx_buf);
	}
	return ret;
}

/*****************************************************************************
 *** Function:    int receive(int fd)                                                                        ***
 ***                                                                        ***
 *****************************************************************************/
static int receive(int fd, uint8_t bits, uint32_t speed, uint16_t delay,
		uint8_t *cmd, int cmd_len, uint8_t *rx, int rx_len) {

	int ret = 1;
	uint8_t *cmddata_buf;
	uint8_t *rxdata_buf;
	int len;
	len = rx_len;

	struct spi_ioc_transfer tr = { .tx_buf = (unsigned long) rx, .rx_buf =
			(unsigned long) NULL, .len = len, .delay_usecs = delay, .speed_hz =
			speed, .bits_per_word = bits, };

	if (cmd_len > 0) {
		/* initialize buffer with zero */
		memset(rx, 0, len);

		cmddata_buf = malloc(rx_len + cmd_len);
		memset(cmddata_buf, 0, rx_len + cmd_len);
		memcpy(cmddata_buf, cmd, cmd_len);

		rxdata_buf = malloc(rx_len + cmd_len);
		memset(rxdata_buf, 0, rx_len + cmd_len);

		tr.tx_buf = (unsigned long) cmddata_buf;
		tr.rx_buf = (unsigned long) rxdata_buf;
		tr.len = cmd_len + rx_len;
	}
	else{
		//get size of cmd
		int size = sizeof(rx);
		int cmdSize = size - len;

		memset(rx+cmdSize, 0, len);

		cmddata_buf = malloc(size);
		memset(cmddata_buf, 0, size);
		memcpy(cmddata_buf, rx, cmdSize);

		rxdata_buf = malloc(size);
		memset(rxdata_buf, 0, size);

		tr.tx_buf = (unsigned long) cmddata_buf;
		tr.rx_buf = (unsigned long) rxdata_buf;
		tr.len = size;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);

	if (ret < 1) {
		printf("\ncan't send spi message\n");
	}
	if (cmd_len > 0) {
		if(ret >= 0){
			int offset = cmd_len;
			memcpy(rx, rxdata_buf + offset, rx_len);
		}
		free(cmddata_buf);
		free(rxdata_buf);
	}
	else{
		if(ret >= 0){
			int size = sizeof(rx);
			int cmdSize = size - len;
			memcpy(rx + cmdSize, rxdata_buf + cmdSize, rx_len);
		}
		free(cmddata_buf);
		free(rxdata_buf);
	}
	return ret;
}

/*****************************************************************************
 *** Function:    int send(int fd)                                                                        ***
 ***                                                                        ***
 *****************************************************************************/
static int send(int fd, uint8_t bits, uint32_t speed, uint16_t delay,
		uint8_t *cmd, int cmd_len, uint8_t *data, int data_len) {

	int ret = 1;
	uint8_t *cmddata_buf;
	int len;
	len = data_len;

	struct spi_ioc_transfer tr = { .tx_buf = (unsigned long) data, .rx_buf =
			(unsigned long) NULL, .len = len, .delay_usecs = delay, .speed_hz =
			speed, .bits_per_word = bits, };

	if (cmd_len > 0) {
		cmddata_buf = malloc(data_len + cmd_len);
		memcpy(cmddata_buf, cmd, cmd_len);
		memcpy(cmddata_buf + cmd_len, data, data_len);
		tr.tx_buf = (unsigned long) cmddata_buf; //Not Tested!!
		tr.rx_buf = (unsigned long) NULL;
		tr.len = cmd_len + data_len;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1) {
		printf("\ncan't send spi message\n");
	}
	if (cmd_len > 0) {
		free(cmddata_buf);
	}

	return ret;
}

/*****************************************************************************
 *** Function:    int exchange(int fd)                                                                        ***
 ***                                                                        ***
 *****************************************************************************/

static int exchange(int fd, uint8_t bits, uint32_t speed, uint16_t delay,
		uint8_t *cmd, int cmd_len, uint8_t *data, int data_len) {

	int ret = 1;
	uint8_t *cmddata_buf;

	int len;
	len = data_len;

	struct spi_ioc_transfer tr = {
			.tx_buf = (unsigned long) data, //Not Tested!!
			.rx_buf = (unsigned long) data, .len = len, .delay_usecs = delay,
			.speed_hz = speed, .bits_per_word = bits, };

	/* If cmd is given */
	if (cmd_len > 0) {
		cmddata_buf = malloc(data_len + cmd_len);
		memcpy(cmddata_buf, cmd, cmd_len);
		memcpy(cmddata_buf + cmd_len, data, data_len);
		tr.tx_buf = (unsigned long) cmddata_buf; //Not Tested!!
		tr.rx_buf = (unsigned long) cmddata_buf;
		tr.len = cmd_len + data_len;
	}

	ret = ioctl(fd, SPI_IOC_MESSAGE(1), &tr);
	if (ret < 1) {
		printf("\ncan't send spi message\n");
	}
	/* If cmd is given */
	if (cmd_len > 0) {
		if(ret >= 0){
			int offset = cmd_len;
			memcpy(data, cmddata_buf + offset, data_len);
		}
	free(cmddata_buf);
	}

	return ret;
}

/*****************************************************************************
 *** Function:    int configure_port(int fd)                                ***
 ***                                                                        ***
 *** Parameters:  fd: Name of the filedescriptor                            ***
 ***                                                                        ***
 *** Return:      new filedescriptor                                        ***
 ***                                                                        ***
 *** Description                                                            ***
 *** -----------                                                            ***
 *** Configure the UART port.                                               ***
 *****************************************************************************/
int configure_port(int fd, uint8_t mode, uint8_t bits, uint32_t speed) {
	int ret;
	uint8_t spi_mode;

	if (mode) {
		/* spi mode */
		if (mode == 1)
			spi_mode = SPI_MODE_0;
		else if (mode == 2)
			spi_mode = SPI_MODE_1;
		else if (mode == 3)
			spi_mode = SPI_MODE_2;
		else
			spi_mode = SPI_MODE_3;

		ret = ioctl(fd, SPI_IOC_WR_MODE, &spi_mode);
		if (ret == -1) {
			goto failure;
		}

		ret = ioctl(fd, SPI_IOC_RD_MODE, &spi_mode);
		if (ret == -1) {
			goto failure;
		}
	}

	if (bits) {
		/* bits per word */
		ret = ioctl(fd, SPI_IOC_WR_BITS_PER_WORD, &bits);
		if (ret == -1) {
			goto failure;
		}

		ret = ioctl(fd, SPI_IOC_RD_BITS_PER_WORD, &bits);
		if (ret == -1) {
			goto failure;
		}

	}

	if (speed) {
		/* max speed hz */
		ret = ioctl(fd, SPI_IOC_WR_MAX_SPEED_HZ, &speed);
		if (ret == -1) {
			goto failure;
		}

		ret = ioctl(fd, SPI_IOC_RD_MAX_SPEED_HZ, &speed);
		if (ret == -1) {
			goto failure;
		}

	}

	return fd;
	failure: return -1;
}


int Nspi_getEnviromentVariables(int hFile)
{
	char *nspi_speed;
	char *nspi_bits;
	char *nspi_delay;
	char *nspi_mode;
	long mode;
	char *ptr;


	nspi_speed = getenv("NSPI_SPEED");
	if(nspi_speed == NULL){
		nspi_speed = NSPI_DEFAULT_SPEED;
		printf("Environment variable NSPI_SPEED was not set. Setting to default value %s\n", NSPI_DEFAULT_SPEED);
	}
	speed = strtol(nspi_speed, &ptr, 10);

	nspi_bits = getenv("NSPI_BITS");
	if(nspi_bits == NULL){
		nspi_bits = NSPI_DEFAULT_BITS;
		printf("Environment variable NSPI_BITS was not set. Setting to default value %s\n", NSPI_DEFAULT_BITS);
	}
	bits = strtol(nspi_bits, &ptr, 10);

	nspi_delay = getenv("NSPI_DELAY");
	if(nspi_delay == NULL){
		nspi_delay = NSPI_DEFAULT_DELAY;
		printf("Environment variable NSPI_DELAY was not set. Setting to default value %s\n", NSPI_DEFAULT_DELAY);
	}
	delay = strtol(nspi_delay, &ptr, 10);

	nspi_mode = getenv("NSPI_MODE");
	if(nspi_mode == NULL){
		nspi_mode = NSPI_DEFAULT_MODE;
		printf("Environment variable NSPI_MODE was not set. Setting to default value %s\n", NSPI_DEFAULT_MODE);
	}
	mode = strtol(nspi_mode, &ptr, 10);

	hFile = configure_port(hFile, mode, bits, speed);

	return 0;
}

int Nspi_IoControl(int hFile, uint32_t dwIoControlCode, uint8_t * lpInBuffer,
		uint32_t nInBufferSize, uint8_t * lpOutBuffer, uint32_t nOutBufferSize,
		uint32_t * lpBytesReturned, void * lpOverlapped) {

	int ret = 0;

	switch (dwIoControlCode) {
	case IOCTL_NSPI_TRANSFER:
		ret = transfer(hFile, bits, speed, delay, lpInBuffer, nInBufferSize,
				lpOutBuffer, nOutBufferSize);
		break;
	case IOCTL_NSPI_RECEIVE:
		ret = receive(hFile, bits, speed, delay, lpInBuffer, nInBufferSize,
				lpOutBuffer, nOutBufferSize);
		break;
	case IOCTL_NSPI_SEND:
		ret = send(hFile, bits, speed, delay, lpInBuffer, nInBufferSize,
				lpOutBuffer, nOutBufferSize);
		break;
	case IOCTL_NSPI_EXCHANGE:
		ret = exchange(hFile, bits, speed, delay, lpInBuffer, nInBufferSize,
				lpOutBuffer, nOutBufferSize);
		break;
	default:
		printf("Unknown operation\n");
		break;
	}

	/* IOCTL returns the amount of bytes transfered but
	 * according to documentation Windows expects 0.
	 */
	if (ret >= 0)
		ret = 0;

	return ret;
}

