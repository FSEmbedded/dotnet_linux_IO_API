/*
 * net_i2c_api.h
 *
 *  Created on: Jul 22, 2020
 *      Author: developer
 */

#ifndef NET_I2C_API_H_
#define NET_I2C_API_H_

struct NI2C_MSG_HEADER
{
	uint8_t chDevAddr;
	uint8_t chFlags;
	uint16_t wLen;
};

int I2C_IoControl(int hFile, uint32_t dwIoControlCode,
        struct NI2C_MSG_HEADER * lpInBuffer,
		uint32_t nInBufferSize,
        uint8_t * lpOutBuffer, uint32_t nOutBufferSize,
        uint32_t * lpBytesReturned, void * lpOverlapped);

#endif /* NET_I2C_API_H_ */
