#ifndef __ASF_COMMONH_
#define __ASF_COMMONH_

/* Generic ASF info */
#define MAX_CHUNK_SIZE 65535

//extern unsigned char ASF_HDR_ID[16];
extern unsigned char ASF_HDR_ID[];

/* Offsets of some critical information in ASF header */
#define HDR_TOTAL_SIZE_8               	0x28
#define HDR_NUM_PACKETS_8              	0x38
#define HDR_FINE_TOTALTIME_8           	0x40
#define HDR_FINE_PLAYTIME_8            	0x48
#define HDR_PLAYTIME_OFFSET_4          	0x50
#define HDR_FLAGS_4                    	0x58
#define HDR_ASF_CHUNKLENGTH_4          	0x5c
#define HDR_ASF_CHUNKLENGTH_CONFIRM_4  	0x60
#define DATSEG_HDR_SIZE 		0x32
#define DATSEG_NUMCHUNKS_4 		0x28

typedef enum  /* The type of content on the server */
{
	connect_failed = 0,
	server_error,
	password_required,
	unknown_content,
	live_content,
	prerecorded_content,
	redirector_content,
}
CONTENTTYPE;

struct CUSTOM_INFO_ASF /* ASF specific info (Unique in each job) */
{
	CONTENTTYPE ASFContentType;

	unsigned long MaxTimeCode;
	unsigned long NumDataChunks;
	unsigned long SizeOfDataChunks;
	unsigned int Time;
	unsigned int TotalSizeHi, TotalSizeLo;
	unsigned long TotalTimeHi, TotalTimeLo;
	unsigned long Offset;
	unsigned long ChunkLength;
	unsigned long ChunkLength2;
	unsigned long Flags;
	unsigned int HeaderOffs;
	unsigned int EndOfHeaderOffs;

	SOCKADDR_IN Server;
	char File[512];
	char ServerName[512];
	unsigned char RandomizedGUID[40];
	unsigned char Base64Buf[512];
	unsigned short PortNum;

};

struct STREAM_PARM /* Stream parameters (Unique in each stream) */
{
	int RequestType;
	char *si;
	SOCKET conn_socket;
};

#endif /* __ASF_COMMONH_ */
