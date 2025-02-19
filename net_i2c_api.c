#include <stdio.h>
#include <unistd.h>

#include <sys/ioctl.h>
#include <fcntl.h>
#include <string.h> // string function definitions
#include <stdint.h>
#include <stdlib.h>
#include <linux/i2c-dev.h>
#include <linux/i2c.h>
#include <pthread.h>
#include <unistd.h>
#include <errno.h>

#include "net_i2c_api.h"

/* Windows IOCTL definitions */
#define METHOD_BUFFERED  0
#define METHOD_IN_DIRECT  1
#define METHOD_OUT_DIRECT  2
#define METHOD_NEITHER  3

#define FILE_ANY_ACCESS  (0 << 14)
#define FILE_READ_ACCESS  (1 << 14)   // file & pipe
#define FILE_WRITE_ACCESS  (2 << 14)   // file & pipe

#define DEVICE_NI2C  (0x00008037U << 16)

#define IOCTL_NI2C_SCHEDULE \
	(DEVICE_NI2C | FILE_ANY_ACCESS | (0x800 << 2) | METHOD_BUFFERED)
#define IOCTL_NI2C_GET_RESULT \
	(DEVICE_NI2C | FILE_ANY_ACCESS | (0x801 << 2) | METHOD_BUFFERED)
#define IOCTL_NI2C_SKIP_RESULT \
	(DEVICE_NI2C | FILE_ANY_ACCESS | (0x802 << 2) | METHOD_BUFFERED)

#define IOCTL_NI2C_CHECK_RESULT \
	(DEVICE_NI2C | FILE_ANY_ACCESS | (0x803 << 2) | METHOD_BUFFERED)

#define IOCTL_NI2C_GET_CLKFREQ \
	(DEVICE_NI2C | FILE_ANY_ACCESS | (0x804 << 2) | METHOD_BUFFERED)
#define NI2C_MSG_HEADER_SIZE sizeof(NI2C_MSG_HEADER)

pthread_mutex_t i2cMutex = PTHREAD_MUTEX_INITIALIZER;
//thread that handles i2c calls
pthread_t i2cThread;
//0 if no background thread is running, otherwise 1
int threadInWork = 0;

//holds the data send by the dotnet application
struct windowsData{
	//File descriptor of the device in use
	int hFile;
	//The list of message headers including flags and size
	struct NI2C_MSG_HEADER * lpInBuffer;
	//size of the header in byte (4 byte per header)
	uint32_t nInBufferSize;
	//Contains the Data including registers, and dummy data
	uint8_t * lpOutBuffer;
	//size of the data in byte
	uint32_t nOutBufferSize;
};

struct ringBufferElement{
	//the message send by the dotnet application
	struct windowsData originalMsg;
	//the send message parsed to fit the ioctl format
	struct i2c_rdwr_ioctl_data *ioctlMsg;
	//amount of ioctl messages in ioctlMsg
	int ioctlMsgSize;
	//result after completion
	int result;
};
struct ringBufferElement ringBuffer[512];

int scheduleIndex = 0;	//Index of where the next Schedule call will be saved
int processIndex = 0;	//Index of the next message to be processed by ioctl
int resultIndex = 0;	//Index of the next result to be returned to the caller

/*
 * Name			: deallocation
 *
 * Input		: struct ringBufferElement
 * Output		: void
 *
 * Description	: Deallocates all the memory of one ringBufferElement, called
 * 		  after the result is retrieved or skipped by the caller
 */
void deallocation(struct ringBufferElement toFree){
	int size = toFree.ioctlMsgSize;
	for(int i = 0; i < size; i++){
		free(toFree.ioctlMsg[i].msgs[0].buf);
		free(toFree.ioctlMsg[i].msgs);
	}
	free(toFree.ioctlMsg);
	free(toFree.originalMsg.lpInBuffer);
}

/*
 * Name			: setupMemoryForMessage
 *
 * Input		: struct ringBufferElement
 * Output		: void
 *
 * Description	: Allocate space for the IOCTL massage that is parsed from the
 * 		  dotnet message.
 */
void setupMemoryForMessage(struct ringBufferElement *element){
	struct windowsData *win = &element->originalMsg;

	int nMsgHeaders = win->nInBufferSize/sizeof(struct NI2C_MSG_HEADER);
	int nIoctlMsgs = 0;

	for(int  i = 0; i < nMsgHeaders ; i++){
		if((i < nMsgHeaders-1) \
		    && ((win->lpInBuffer[i+1].chDevAddr & 0x1) == 0x1)){
			//read
			nIoctlMsgs += win->lpInBuffer[++i].wLen;
		}
		else{
			//write
			nIoctlMsgs += win->lpInBuffer[i].wLen - 1;
		}
	}
	element->ioctlMsgSize = nIoctlMsgs;
	element->ioctlMsg = malloc(sizeof(struct i2c_rdwr_ioctl_data) * \
				   nIoctlMsgs);
}

/*
 * Name		: convertDotnetToIoctl
 *
 * Input	: struct ringBufferElement
 * 		  int nmsgs
 * 		  int message_index
 * 		  int headerIndex
 * 		  int reg
 * 		  int dataIndex
 * Output	: void
 *
 * Description	: Takes a dotnet api call and some Information about its
 * 		  location and converts it into an ioctl call.
 */
void convertDotnetToIoctl(struct ringBufferElement *element, int nmsgs,
			  int message_index, int headerIndex, int reg,
			  int dataIndex){
	struct windowsData *win = &element->originalMsg;
	struct i2c_rdwr_ioctl_data *ioctlMsg =&element-> \
					      ioctlMsg[message_index];

	//how many ioctl meassage are needed by this one windows message
	ioctlMsg->nmsgs = nmsgs;
	ioctlMsg->msgs = malloc(sizeof(struct i2c_msg)*nmsgs);

	ioctlMsg->msgs[0].addr = win->lpInBuffer[headerIndex].chDevAddr >> 1;
	ioctlMsg->msgs[0].buf = malloc(sizeof(__u8)*(3-nmsgs));
	ioctlMsg->msgs[0].flags = win->lpInBuffer[headerIndex].chFlags;
	ioctlMsg->msgs[0].len = 3 - nmsgs;

	ioctlMsg->msgs[0].buf[0] = reg;
	if(nmsgs == 1){
		//write
		ioctlMsg->msgs[0].buf[1] = win->lpOutBuffer[dataIndex];
	}
	else{
		//read
		ioctlMsg->msgs[1].addr = win->lpInBuffer[headerIndex + 1]. \
				         chDevAddr >> 1;
		ioctlMsg->msgs[1].buf = &win->lpOutBuffer[dataIndex];
		ioctlMsg->msgs[1].flags = (I2C_M_RD | I2C_M_NOSTART);
		ioctlMsg->msgs[1].len = 1;
	}
}

/*
 * Name		: parseMessage
 *
 * Input	: Data that needs to be parsed
 * Output	: void
 *
 * Description	: analyzes every single dotnet command and call methods to
 * 		  convert them into IOCTL calls.
 */
void parseMessage(struct windowsData data){
	//Iterator for the Windows Style Array
	int outBufferIndex = 0;
	//Saves the register of the i2c controller to be interacted with next
	int reg = 0;

	setupMemoryForMessage(&ringBuffer[processIndex]);

	int parsePos = 0;
	for(int headerPos = 0; headerPos < data.nInBufferSize/4; headerPos++){
		reg = data.lpOutBuffer[outBufferIndex++];
		//read
		if((headerPos < data.nInBufferSize/4 - 1) &&
		   ((data.lpInBuffer[headerPos+1].chDevAddr & 0x1) == 0x1)){
			for(int dataPos = 0; dataPos < data.lpInBuffer[headerPos+1].wLen; dataPos++){
				convertDotnetToIoctl(&ringBuffer[processIndex],
						     2, parsePos, headerPos,
						     reg, outBufferIndex);
				reg++;
				outBufferIndex++;
				parsePos++;
			}
			headerPos++;
		}
		//write
		else{
			for(int dataPos = 0; dataPos < data.lpInBuffer[headerPos].wLen - 1; dataPos++){
				convertDotnetToIoctl(&ringBuffer[processIndex],
						     1, parsePos, headerPos,
						     reg, outBufferIndex);
				reg++;
				outBufferIndex++;
				parsePos++;
			}
		}
	}
}

/*
 * Name		: i2cBackgroundTask
 *
 * Input	: void pointer (not used but necassary)
 * Output	: void * (also not used)
 *
 * Description	: calls the parser method and ioctl as long as there is work to
 * 		  be done, afterwards the task waits for new work
 */
void * i2cBackgroundTask(void * arg){
	while(1) {
		//wait for next call
		while(scheduleIndex == processIndex){ }

		pthread_mutex_lock(&i2cMutex);
		//if 1 the calling function will not create another task
		threadInWork = 1;
		if(processIndex >= 512) processIndex = 0;
		parseMessage(ringBuffer[processIndex].originalMsg);

		int result = 0;
		for(int i = 0; i < ringBuffer[processIndex].ioctlMsgSize; i++){
			int j = ioctl(ringBuffer[processIndex].originalMsg.hFile,
				      I2C_RDWR, &ringBuffer[processIndex]. \
				      ioctlMsg[i]);
			if(j == -1) result = -1;

		}
		ringBuffer[processIndex].result = result;

		processIndex++;
		pthread_mutex_unlock(&i2cMutex);
	}
}

void queueWindowsMsg(struct windowsData *win, int hFile, uint32_t nInBufferSize,
		     struct NI2C_MSG_HEADER * lpInBuffer,
		     uint32_t nOutBufferSize, uint8_t* lpOutBuffer) {
	win->hFile = hFile;		
	win->nInBufferSize = nInBufferSize;
	win->nOutBufferSize = nOutBufferSize;
	win->lpOutBuffer = malloc(win->nOutBufferSize);
	memcpy(win->lpOutBuffer, lpOutBuffer, nOutBufferSize);
	win->lpInBuffer = malloc(nInBufferSize);
	for(int i = 0; i < ringBuffer[scheduleIndex].originalMsg. \
			nInBufferSize/sizeof(struct NI2C_MSG_HEADER); i++){
		win->lpInBuffer[i].chDevAddr = lpInBuffer[i].chDevAddr;
		win->lpInBuffer[i].chFlags = lpInBuffer[i].chFlags;
		win->lpInBuffer[i].wLen = lpInBuffer[i].wLen;
	}
}

/*
 * Name		: I2C_IoControl
 *
 * Input	: i2c message in windows style
 * Output	: int, 0 if successful
 *
 * Description	: handles the basic windows calls like schedule or getresult.
 * 		  Creates Task if neccasery.
 */
int I2C_IoControl(int hFile, uint32_t dwIoControlCode,
        struct NI2C_MSG_HEADER * lpInBuffer,
		uint32_t nInBufferSize,
        uint8_t * lpOutBuffer, uint32_t nOutBufferSize,
        uint32_t * lpBytesReturned, void * lpOverlapped){

	//Used when the user is requesting to send a i2c message
	if (dwIoControlCode == IOCTL_NI2C_SCHEDULE){

		pthread_mutex_lock(&i2cMutex);
		if(scheduleIndex >= 512) scheduleIndex = 0;

		queueWindowsMsg(&ringBuffer[scheduleIndex].originalMsg, hFile,
				nInBufferSize, lpInBuffer, nOutBufferSize,
				lpOutBuffer);

		scheduleIndex++;
		pthread_mutex_unlock(&i2cMutex);

		if(threadInWork == 0) {
			if(pthread_create(&i2cThread, NULL,
					  &i2cBackgroundTask, NULL) != 0){
				printf("thread creation failed\n");
				return 2;
			}
		}
		//printf("leaks sched: %x\n", leaks);
		return 0;
	}
	//Used if the User wants the result of the next message
	else if (dwIoControlCode == IOCTL_NI2C_GET_RESULT){
		int res;
		while(processIndex == resultIndex) { }
		pthread_mutex_lock(&i2cMutex);
		if(resultIndex >= 512) resultIndex = 0;

		if(ringBuffer[resultIndex].originalMsg.nInBufferSize
							!= nInBufferSize){
			pthread_mutex_unlock(&i2cMutex);
			errno = EINVAL;
			return -1;
		}
		if(ringBuffer[resultIndex].originalMsg.nOutBufferSize
							!= nOutBufferSize){
			pthread_mutex_unlock(&i2cMutex);
			errno = EINVAL;
			return -1;
		}

		memcpy(lpOutBuffer, ringBuffer[resultIndex].originalMsg.lpOutBuffer,
		       nOutBufferSize);

		//get the result and free the allocated space
		res = ringBuffer[resultIndex].result;
		deallocation(ringBuffer[resultIndex]);
		resultIndex++;

		pthread_mutex_unlock(&i2cMutex);
		//printf("leaks getr: %x\n", leaks);
		return res;
	}
	//Used if the User wants to skip a return of a call
	else if (dwIoControlCode == IOCTL_NI2C_SKIP_RESULT) {
		if (processIndex == resultIndex) { }
		pthread_mutex_lock(&i2cMutex);
		if(resultIndex >= 512) resultIndex = 0;

		//free the allocated space without saving the result
		deallocation(ringBuffer[resultIndex]);
		resultIndex++;
		pthread_mutex_unlock(&i2cMutex);
		//printf("leaks skip: %x\n", leaks);
		return 0;
	}
	//Used to check if there are Results to retrieve
	else if (dwIoControlCode == IOCTL_NI2C_CHECK_RESULT){
		return processIndex - resultIndex;
	}
	//Used to check clock frequency. At the moment only returns clock
	//frequency of i2c0
	else if (dwIoControlCode == IOCTL_NI2C_GET_CLKFREQ){
		//possible path would be:
		//proc/device-tree/soc/aips-bus@02100000/i2c@021a0000//clock-frequency
		//but the middle part comes from an alias

		char* pathStart = "/proc/device-tree";
		char* pathEnd = "/clock-frequency";

		uint32_t fequency = -1;
		int success = -1;

		//path will be 67 characters long including the null terminator
		char* path = malloc(sizeof(char)*67);

		//create the path string
		for(int i = 0; i < 17; i++) path[i] = pathStart[i];
		FILE* aliasFile = fopen("/proc/device-tree/aliases/i2c0", "r");
		fread(&path[17], sizeof(char), 35, aliasFile);
		for(int i = 0; i < 16; i++) path[i + 52] = pathEnd[i];
		path[68] = 0;

		//read file if it exists
		FILE* freqFile = fopen(path, "r");
		if (freqFile != 0) {
			success = fread(&fequency, 4, 1, freqFile);
			fclose(freqFile);
		}
		free(path);
		//if there was a successful read return the read alue, wlse -1
		if (success >= 0) return fequency;
		else return -1;
	}
	else
		return -1;
	return -1;
}
