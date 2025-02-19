/*
 * net_spi_api.h
 *
 *  Created on: Jul 22, 2020
 *      Author: developer
 */

#ifndef NET_SPI_API_H_
#define NET_SPI_API_H_

int Nspi_IoControl(int hFile, uint32_t dwIoControlCode,
        uint8_t * lpInBuffer,
		uint32_t nInBufferSize,
        uint8_t * lpOutBuffer, uint32_t nOutBufferSize,
        uint32_t * lpBytesReturned, void * lpOverlapped);

int Nspi_getEnviromentVariables(int hFile);

#endif /* NET_SPI_API_H_ */
