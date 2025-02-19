#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h> // string function definitions
#include <stdint.h>
#include <stdlib.h>

#include "net_i2c_api.h"
#include "net_spi_api.h"

#define WIN_INVALID_HANDLE_VALUE -1

		/* Windows APIError definitions*/
#define	WIN_ERROR_FILE_NOT_FOUND  2     // Port not found
#define	WIN_ERROR_ACCESS_DENIED  5      // Access denied
#define	WIN_ERROR_INVALID_HANDLE  6     // Invalid handle
#define	WIN_ERROR_NOT_READY  21         // Device not ready
#define	WIN_ERROR_WRITE_FAULT  27       // Write fault
#define	WIN_ERROR_DEV_NOT_EXIST  55     // Device does not exist
#define	WIN_ERROR_INVALID_PARAMETER  87 // Bad parameters
#define	WIN_ERROR_INVALID_NAME  123     // Invalid port name

        /* Windows IOCTL definitions */
#define METHOD_BUFFERED  0
#define METHOD_IN_DIRECT  1
#define METHOD_OUT_DIRECT  2
#define METHOD_NEITHER  3

#define FILE_ANY_ACCESS  (0 << 14)
#define FILE_READ_ACCESS  (1 << 14)   // file & pipe
#define FILE_WRITE_ACCESS  (2 << 14)   // file & pipe

#define DEVICE_NI2C  (0x00008037U << 16)
#define DEVICE_NSPI  (0x0000800AU << 16)

#define WIN_QUERY  0x0
#define WIN_WRTIE  0x40000000
#define WIN_READ   0x80000000
#define WIN_READ_WRITE (WIN_WRTIE | WIN_READ)

int isDeviceType(char * filename, char * devicename)
{
	int i;
	int dif;

	long int filename_len = strlen(filename);
	long int devicename_len = strlen(devicename);

	dif = filename_len - devicename_len;
	if(dif < 0)
	{
		return 0;
	}

	for(i = 0; i < dif; i++)
	{
		int ret;
		ret = strncmp(filename + i ,devicename, devicename_len);

		if (ret == 0)
		{
		    return 1;
		}
	}


	return 0;
}

long long CreateFileW(char* lpFileName, uint32_t dwDesiredAccess,
                             uint32_t dwShareMode, void *  lpSecurityAttributes,
                             uint32_t dwCreationDisposition,
                             uint32_t dwFlagsAndAttributes,
                             void * hTemplateFile)
{
	long long fd;
	int mode;
	int ret;

	switch(dwDesiredAccess)
	{
		case WIN_WRTIE: mode = O_WRONLY; break;
		case WIN_READ:  mode = O_RDONLY; break;
		case WIN_READ_WRITE: mode = O_RDWR; break;
		case WIN_QUERY:;
		default: printf("Unknown Mode\n"); return WIN_INVALID_HANDLE_VALUE;
	}

	fd = open(lpFileName, mode);

	if(fd < 1)
	{

		return WIN_INVALID_HANDLE_VALUE;
	}


	ret = isDeviceType(lpFileName, "spidev");

	if(ret == 1)
	{
		Nspi_getEnviromentVariables(fd);
	}

	return(fd);
}



int CloseHandle( void * hObject){
	int filePtr = (uintptr_t)hObject;
	close(filePtr);
	return 0;
}


int DeviceIoControl(int hFile, uint32_t dwIoControlCode,
        void * lpInBuffer,
		uint32_t nInBufferSize,
        uint8_t * lpOutBuffer, uint32_t nOutBufferSize,
        uint32_t * lpBytesReturned, void * lpOverlapped){

	int ret;

	switch(dwIoControlCode & (0xFFFF<<16)) {
		case DEVICE_NI2C:
			ret = I2C_IoControl(hFile, dwIoControlCode, (struct NI2C_MSG_HEADER *) lpInBuffer, nInBufferSize,lpOutBuffer,
						  nOutBufferSize, lpBytesReturned, lpOverlapped);
			break;
		case DEVICE_NSPI:
			ret = Nspi_IoControl(hFile, dwIoControlCode, (uint8_t *) lpInBuffer, nInBufferSize,lpOutBuffer,
				  nOutBufferSize, lpBytesReturned, lpOverlapped);
			break;
		default: printf("Unkown device\n"); break;
	}

	return ret;

}


