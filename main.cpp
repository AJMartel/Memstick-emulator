#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h> 
#include <sys/socket.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <linux/usb/ch9.h>
#include <poll.h>
#include <linux/usb/gadgetfs.h>
#include <sys/types.h>
#include <iostream>
#include <math.h>
#include <unistd.h>
#include <sys/stat.h>
#include <fstream>
#include <cstring>
#include <sys/mount.h>
#include "signal.h"

int gadgetFile, outEp, inEp;

int fatFile, fileSize;

static pthread_t gadgetThread, outThread, inThread;

// max lun at ff breaks ps3 - buffer issue
// mess with all command buffers
// mess with device and block size
// ignoring lots of 5a and 1a commands turns ps3 off

#define FILE_SIZE 0xC000000
#define FILE_BLOCK_SIZE 512

// dosfs header
// 00000000  eb 58 90 6d 6b 64 6f 73  66 73 00 00 02 01 20 00  |.X.mkdosfs.... .|
// 00000010  02 00 00 00 80 f8 00 00  20 00 40 00 00 00 00 00  |........ .@.....|
// 00000020  00 00 00 00 fc 00 00 00  00 00 00 00 02 00 00 00  |................|
// 00000030  01 00 06 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
// 00000040  00 00 29 05 a2 5e c6 20  20 20 20 20 20 20 20 20  |..)..^.         |
// 00000050  20 20 46 41 54 33 32 20  20 20 0e 1f be 77 7c ac  |  FAT32   ...w|.|
// 00000060  22 c0 74 0b 56 b4 0e bb  07 00 cd 10 5e eb f0 32  |".t.V.......^..2|
// 00000070  e4 cd 16 cd 19 eb fe 54  68 69 73 20 69 73 20 6e  |.......This is n|
// 00000080  6f 74 20 61 20 62 6f 6f  74 61 62 6c 65 20 64 69  |ot a bootable di|
// 00000090  73 6b 2e 20 20 50 6c 65  61 73 65 20 69 6e 73 65  |sk.  Please inse|
// 000000a0  72 74 20 61 20 62 6f 6f  74 61 62 6c 65 20 66 6c  |rt a bootable fl|
// 000000b0  6f 70 70 79 20 61 6e 64  0d 0a 70 72 65 73 73 20  |oppy and..press |
// 000000c0  61 6e 79 20 6b 65 79 20  74 6f 20 74 72 79 20 61  |any key to try a|
// 000000d0  67 61 69 6e 20 2e 2e 2e  20 0d 0a 00 00 00 00 00  |gain ... .......|
// 000000e0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 00 00  |................|
// *
// 000001f0  00 00 00 00 00 00 00 00  00 00 00 00 00 00 55 aa  |..............U.|

unsigned char mbr[0x200] = {
	0xeb, 0x58, 0x90, 0x6d, 0x6b, 0x64, 0x6f, 0x73, 0x66, 0x73, 0x00, 0x00, 0x02, 0x01, 0x20, 0x00,
	0x02, 0x00, 0x00, 0x00, 0x80, 0xf8, 0x00, 0x00, 0x20, 0x00, 0x40, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0xfc, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x02, 0x00, 0x00, 0x00,
	0x01, 0x00, 0x06, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x29, 0x05, 0xa2, 0x5e, 0xc6, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20, 0x20,
	0x20, 0x20, 0x46, 0x41, 0x54, 0x33, 0x32, 0x20, 0x20, 0x20, 0x0e, 0x1f, 0xbe, 0x77, 0x7c, 0xac,
	0x22, 0xc0, 0x74, 0x0b, 0x56, 0xb4, 0x0e, 0xbb, 0x07, 0x00, 0xcd, 0x10, 0x5e, 0xeb, 0xf0, 0x32,
	0xe4, 0xcd, 0x16, 0xcd, 0x19, 0xeb, 0xfe, 0x54, 0x68, 0x69, 0x73, 0x20, 0x69, 0x73, 0x20, 0x6e,
	0x6f, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x64, 0x69,
	0x73, 0x6b, 0x2e, 0x20, 0x20, 0x50, 0x6c, 0x65, 0x61, 0x73, 0x65, 0x20, 0x69, 0x6e, 0x73, 0x65,
	0x72, 0x74, 0x20, 0x61, 0x20, 0x62, 0x6f, 0x6f, 0x74, 0x61, 0x62, 0x6c, 0x65, 0x20, 0x66, 0x6c,
	0x6f, 0x70, 0x70, 0x79, 0x20, 0x61, 0x6e, 0x64, 0x0d, 0x0a, 0x70, 0x72, 0x65, 0x73, 0x73, 0x20,
	0x61, 0x6e, 0x79, 0x20, 0x6b, 0x65, 0x79, 0x20, 0x74, 0x6f, 0x20, 0x74, 0x72, 0x79, 0x20, 0x61,
	0x67, 0x61, 0x69, 0x6e, 0x20, 0x2e, 0x2e, 0x2e, 0x20, 0x0d, 0x0a, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00
};

void setupMbr() {

	memset(&mbr[0xe0],0x0,0x200-0xe0);
	mbr[510] = 0x55;
	mbr[511] = 0xaa;


}

unsigned char fileBuff[FILE_SIZE];

unsigned char dumpedDescriptor[] = {
	0x00, 0x00, 0x00, 0x00,
	0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0x64,
	0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00,
	0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00,
	0x07, 0x05, 0x02, 0x02, 0x00, 0x02, 0x00,
	0x09, 0x02, 0x20, 0x00, 0x01, 0x01, 0x00, 0x80, 0x64,
	0x09, 0x04, 0x00, 0x00, 0x02, 0x08, 0x06, 0x50, 0x00,
	0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00,
	0x07, 0x05, 0x02, 0x02, 0x00, 0x02, 0x00,
	0x12, 0x01, 0x00, 0x02, 0x00, 0x00, 0x00, 0x40, 0xfe, 0x13, 0x23, 0x1e, 0x10, 0x01, 0x01, 0x02, 0x03, 0x01 
};

unsigned char outEpDesc[] = {0x01,0x00,0x00,0x00,0x07, 0x05, 0x02, 0x02, 0x00, 0x02, 0x00,0x07, 0x05, 0x02, 0x02, 0x00, 0x02, 0x00};
unsigned char inEpDesc[] = {0x01,0x00,0x00,0x00,0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00,0x07, 0x05, 0x81, 0x02, 0x00, 0x02, 0x00};

void setupEpSize() {

	// uint8_t msB = (FILE_BLOCK_SIZE&0xff00)>>8;
	// uint8_t lsB = (FILE_BLOCK_SIZE&0xff);

	// outEpDesc[8] = lsB;
	// outEpDesc[9] = msB;
	// outEpDesc[15] = lsB;
	// outEpDesc[16] = msB;

	// inEpDesc[8] = lsB;
	// inEpDesc[9] = msB;
	// inEpDesc[15] = lsB;
	// inEpDesc[16] = msB;

	// dumpedDescriptor[26] = lsB;
	// dumpedDescriptor[27] = msB;
	// dumpedDescriptor[33] = lsB;
	// dumpedDescriptor[34] = msB;

	// dumpedDescriptor[58] = lsB;
	// dumpedDescriptor[59] = msB;
	// dumpedDescriptor[65] = lsB;
	// dumpedDescriptor[66] = msB;

}

uint8_t borrowedSenseData[] = {
  0x70,			  
  0x00,
  0x02,			  
  0x00, 0x00, 0x00, 0x00,
  0x0a,			  
  0x00, 0x00, 0x00, 0x00,
  0x3a,			 
  0x00,			  
  0x00,
  0x00, 0x00, 0x00,
};

uint8_t scsi_inquiry_data_00[] = { 0, 0, 0, 0, 0 };

uint8_t scsi_inquiry_data_83[] = {
  0x00,
  0x83,   
  0x00,   
  0x00    
};

uint8_t scsi_inquiry_data[] = {
  0x00,				
  0x80,				
  0x00,				
  0x02,				
  0x20,			
  0x00,
  0x00,
  0x00,
  'U', 'n', 'k', 'n', 'o', 'w', 'n', ' ',
  'F', 'a', 'k', 'e', ' ', 'u', 's', 'b',
  ' ', ' ', ' ', ' ', ' ', ' ', ' ', ' ',
  '1', '.', '0', ' '
};

// Offset (Hex)	Type	Description
// 0x00	int32	Signature (0x43425355)
// 0x04	int32	Tag (Transaction Unique Identifier)
// 0x08	int32	Length
// 0x0c	byte	Direction (0x00 = ToDevice, 0x80 = ToHost)
// 0x0d	byte	Logical Unit Number
// 0x0e	byte	Command Length
// 0x0f	byte[16]	Command Data

#define CBW_SIGNATURE 0x43425355
#define CSW_SIGNATURE 0x53425355

struct Cbw {
	uint32_t sig;
	uint32_t tag;
	uint32_t length;
	uint8_t direction;
	uint8_t lun;
	uint8_t cmdlen;
	uint8_t cmd[16];
} __attribute__((packed));;

// 0x00	int32	Signature (0x53425355)
// 0x04	int32	Tag (Copied From CBW)
// 0x08	int32	Residue (Difference Between CBW Length And Actual Length)
// 0x0c	byte	Status (0x00 = Success, 0x01 = Failed, 0x02 = Phase Error)

struct Csw {
	uint32_t sig;
	uint32_t tag;
	uint32_t residue;
	uint8_t status;
} __attribute__((packed));;

#define TEST_UNIT 0x00
#define REQUEST_SENSE 0x03 
#define INQUIRY 0x12
#define READ_CAPACITY 0x25
#define MODE_SENSE 0x1a
#define BLOCK_READ 0x28
#define BLOCK_WRITE 0x2a
#define MODE_SENSE_10 0x5a
#define SYNCHRONIZE_CACHE 0x35
#define ALLOW_REMOVE 0x1e

uint32_t changeEndianness32(uint32_t num) {

	return ((num>>24)&0xff) |
		((num<<8)&0xff0000) |
		((num>>8)&0xff00) |
		((num<<24)&0xff000000);
}

static void* outCheck(void* nothing) {

	struct Cbw cbw;
	struct Csw csw;

	while(1) {

		unsigned char outBuff[512];

		int readVal = read(outEp,(unsigned char*)&cbw,sizeof(struct Cbw));

		// printf("sig %08x tag: %08x dir: %02x length %d cmdlen: %d command: ",cbw.sig,cbw.tag,cbw.direction,cbw.length,cbw.cmdlen);
		// for(int i = 0 ; i < cbw.cmdlen ; i++) {
		// 	printf("%02x ",cbw.cmd[i]);
		// }
		// printf("\n");

		if(cbw.sig == CBW_SIGNATURE) {

			csw.sig = CSW_SIGNATURE;
			csw.tag = cbw.tag;
			csw.status = 0;
			csw.residue = cbw.length;

			unsigned char* cbwBuff = (unsigned char*)malloc(cbw.length);

			switch(cbw.cmd[0]) {

				case REQUEST_SENSE: {

					uint8_t allocationLength = cbw.cmd[4];

					// csw.residue = sizeof(borrowedSenseData);

					write(inEp,borrowedSenseData,cbw.length);

					break;
				}
				case TEST_UNIT:
					printf("Pinging\n");
					break;
				case INQUIRY:

					printf("Got inquiry: %02x %02x\n",cbw.cmd[1],cbw.cmd[2]);

					if(cbw.cmd[1]&0x01) {

						if(cbw.cmd[2] == 0x83) {
							write(inEp,scsi_inquiry_data_83,sizeof(scsi_inquiry_data_83));
						} else {
							scsi_inquiry_data_00[1] = cbw.cmd[2];
							write(inEp,scsi_inquiry_data_00,sizeof(scsi_inquiry_data_00));
						}

					} else {

						write(inEp,scsi_inquiry_data,sizeof(scsi_inquiry_data));
					}

					break;
				case READ_CAPACITY: {

					printf("Capcities: %08x %d = %08x\n",fileSize,FILE_BLOCK_SIZE,fileSize/FILE_BLOCK_SIZE);
					uint32_t numberOfBlocks = changeEndianness32(fileSize/FILE_BLOCK_SIZE);
					uint32_t blockSize = changeEndianness32(FILE_BLOCK_SIZE);
					memcpy(cbwBuff,(unsigned char*)&numberOfBlocks,4);
					memcpy(&cbwBuff[4],(unsigned char*)&blockSize,4);

					write(inEp,cbwBuff,8);
					break;
				}
				case MODE_SENSE:

					memset(cbwBuff,0x00,cbw.length);
					cbwBuff[0] = 0x03;
					write(inEp,cbwBuff,cbw.length);

					break;
				case BLOCK_READ: {

					uint32_t offsetPointer = 0;
					memcpy(&offsetPointer,(unsigned char*)&cbw.cmd[2],4);
					offsetPointer = changeEndianness32(offsetPointer);

					memset(cbwBuff,0xff,cbw.length);

					lseek(fatFile,offsetPointer * FILE_BLOCK_SIZE,SEEK_SET);

					// for(int i = 0 ; i < (cbw.length/FILE_BLOCK_SIZE) ; i++) {

					int fatFileReadRet = read(fatFile,cbwBuff,cbw.length);
					printf("Reading offset: %08x(%08x) ret: %d\n",offsetPointer,offsetPointer*FILE_BLOCK_SIZE,fatFileReadRet);
					write(inEp,cbwBuff,cbw.length);
					csw.residue = FILE_BLOCK_SIZE;
					// }

					break;
				}
				case BLOCK_WRITE: {

					uint32_t offsetPointer = 0;
					memcpy(&offsetPointer,(unsigned char*)&cbw.cmd[2],4);
					offsetPointer = changeEndianness32(offsetPointer);

					memset(cbwBuff,0xff,cbw.length);

					lseek(fatFile,offsetPointer * FILE_BLOCK_SIZE,SEEK_SET);

					// for(int i = 0 ; i < (cbw.length/FILE_BLOCK_SIZE) ; i++) {
					read(outEp,cbwBuff,cbw.length);

					int fatFileReadRet = write(fatFile,cbwBuff,cbw.length);
					printf("Writing offset: %08x ret: %d\n",offsetPointer,fatFileReadRet);
					csw.residue = FILE_BLOCK_SIZE;
					// }

					break;
				}
				case SYNCHRONIZE_CACHE:
				case ALLOW_REMOVE:
					break;
				case MODE_SENSE_10:
					// csw.status = 1;

					// memset(cbwBuff,0x00,cbw.length);
					// write(inEp,cbwBuff,cbw.length);

					memset(cbwBuff,0x00,cbw.length);
					cbwBuff[0] = 0x03;
					write(inEp,cbwBuff,cbw.length);

					break;
				default:
					printf("Unknown cbw command: %02x\n",cbw.cmd[0]);
					csw.status = 1;
					memset(cbwBuff,0x00,cbw.length);
					if(cbw.direction&0x80) {
						write(inEp,cbwBuff,cbw.length);						
					} else {
						read(inEp,cbwBuff,cbw.length);
					}
					break;
			}

			write(inEp,(unsigned char*)&csw,sizeof(struct Csw));

			free(cbwBuff);

		}


	}

}

char* constructStringDesc(char* name) {

	char* newStringDesc = (char*)malloc(strlen(name)*2+2);

	memset(newStringDesc,0x00,strlen(name)*2+2);

	newStringDesc[1] = 0x03; // type constant string

	for(int i = 0 ; i < strlen(name) ; i++) {

		newStringDesc[i*2+2] = name[i];
	}

	newStringDesc[0] = strlen(name)*2+2;

	return newStringDesc;
}

static void handleSetup(struct usb_ctrlrequest *setup) {
	
	uint16_t value = __le16_to_cpu(setup->wValue);
	uint16_t index = __le16_to_cpu(setup->wIndex);
	uint16_t length = __le16_to_cpu(setup->wLength);

	// printf("Got USB bRequest: %d(%02x) with type %d(%02x dir:(%02x)) value: %04x of length %d\n",setup->bRequest,setup->bRequest,setup->bRequestType, setup->bRequestType, setup->bRequestType&0x80, value, setup->wLength);

	// start transactions

	unsigned char* buf = (unsigned char*)malloc(length);

	char dtStringData[4] = {0x04, 0x03, 0x09, 0x04};

	switch(setup->bRequest) {

		case USB_REQ_GET_DESCRIPTOR: {

			char* descriptorPtr;

			switch(value) {
				case 0x0300:
					descriptorPtr = constructStringDesc("VENDOR");
					break;
				case 0x0301:
					descriptorPtr = constructStringDesc("PRODUCT");
					break;
				case 0x0302:
					descriptorPtr = constructStringDesc("SERIAL");
					break;
				default:
					descriptorPtr = (char*)&dtStringData;
					break;
			}

			// printf("String desc: ");
			// for(int i = 0 ; i < length ; i++) {
			// 	printf("%02x ",descriptorPtr[i]);
			// }
			// printf("\n");

			write(gadgetFile,descriptorPtr,length);

			break;		
		}
		case USB_REQ_SET_CONFIGURATION:
			//printf("set Configuration value %d\n",value);
			read(gadgetFile, NULL, 0);
			break;
		case USB_REQ_SET_INTERFACE:
			//printf("set Interface value %d\n",value);
			read(gadgetFile, NULL, 0);
			break;

		case 0xfe: {// max lun
			buf[0] = 0x0; // random value that may not help things

			int ret = write(gadgetFile, buf, 1);

			//printf("Got new buff: %02x length: %d ret: %d\n",buf[0],length,ret);

			break;
		}
		case 0xff: // reset

			read(gadgetFile, buf, length);

			//printf("Got reset buff\n");

			break;
		default:

			printf("Unknown bRequest: %02x\n",setup->bRequest);

			// stall unknown request
			if(setup->bRequestType&0x80) {
				read(gadgetFile,NULL,0);
			} else {
				write(gadgetFile,NULL,0);
			}

			break;
	}	

	free(buf);

}

static void* inCheck(void* nothing) {

	struct pollfd pollRecv;
    pollRecv.fd=inEp;
    pollRecv.events=POLLIN | POLLOUT | POLLHUP;;

	unsigned char inBuff[512];

	while(1) {

		int pollVal = poll(&pollRecv,1,500);

		if(pollVal >= 0) {

//			printf("Got in poll!\n");
			int writeVal = write(inEp,inBuff,512);

			// printf("Poll write val: %d\n",writeVal);

		}

	}

}


static void* gadgetCfgCb(void* nothing) {

	struct usb_gadgetfs_event events[5];

	struct pollfd pollRecv;
    pollRecv.fd=gadgetFile;
    pollRecv.events=POLLIN | POLLOUT | POLLHUP;;

    printf("Starting gadget read\n");

	char recvData[32];
	int readData;
	
	pthread_setcancelstate(PTHREAD_CANCEL_ENABLE, NULL);

	while(true) {
		
		int pollVal = poll(&pollRecv,1,500);

		if(pollVal >= 0) {
		// if(pollVal >= 0 && (pollRecv.revents&POLLIN)) {

			int ret = read(gadgetFile,&events,sizeof(events));

			unsigned char* eventData = (unsigned char*)malloc(sizeof(events[0]));
			
			for(int i = 0 ; i < (ret / sizeof(events[0])) ; i++) {

				switch(events[i].type) {

					case GADGETFS_SETUP:

						handleSetup(&events[i].u.setup);

						break;
					case GADGETFS_NOP:
						break;
					case GADGETFS_CONNECT:
						printf("Connect\n");
						break;
					case GADGETFS_DISCONNECT:
						printf("Disconnect\n");
						break;
					case GADGETFS_SUSPEND:
						printf("Suspend\n");
						break;
					case 5:
						printf("Possibly reset\n");
						break;
					default:
						printf("Unknown type: %d\n",events[i].type);
						exit(0);
						break;
				}

			}
		}

	}
}

int main() {

	setupEpSize();
	setupMbr();

	mkdir("/dev/gadget/",455);
	umount2("/dev/gadget/", MNT_FORCE);
	int mountRet = mount("none", "/dev/gadget/", "gadgetfs", 0, "");

	if(mountRet < 0) {
		printf("Mounting gadget failed\n");
		return 1;
	}

	fatFile = open("file.img",O_RDWR);
	fileSize = lseek(fatFile, 0, SEEK_END);

	// fileSize = FILE_SIZE;

	gadgetFile = open("/dev/gadget/musb-hdrc", O_RDWR);

	if(gadgetFile < 0) {
		printf("Could not open gadget file, got response %d\n", gadgetFile);
		return 1;
	}

	int writeValGadget = write(gadgetFile,dumpedDescriptor,sizeof(dumpedDescriptor)); // make sure length is right
	
	pthread_create(&gadgetThread,0,gadgetCfgCb,NULL);

	outEp = -1;
	
	while(outEp < 0) {
		outEp = open("/dev/gadget/ep2out", O_CLOEXEC | O_RDWR);
	}

	inEp = open("/dev/gadget/ep1in", O_CLOEXEC | O_RDWR);

	int outWritten = -1;

	while(outWritten < 0) {
		outWritten = write(outEp,outEpDesc,sizeof(outEpDesc));
	}

	int inWritten = write(inEp,inEpDesc,sizeof(inEpDesc));

	if(outWritten < 0 || inWritten < 0) {
		printf("Writing endpoint descriptors didn't work\n");
		return 1;
	}

	pthread_create(&outThread,0,outCheck,NULL);
	// pthread_create(&inThread,0,inCheck,NULL);

	while(1);

}