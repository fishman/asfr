#ifndef __CORE_MMST_H_
#define __CORE_MMST_H_

#define MMST_DEFAULT_PORT 1755

#define MMSC_POS_SIZE			0x08
#define MMSC_POS_SEQ			0x14
#define MMSC_POS_FLAG			0x24
#define MMSC_POS_SUBFLAG		0x26
#define MMSC_POS_REQCHUNKSEQ	0x3c
#define MMSC_POS_CHUNKLENGTH	0x5c

#define MMSD_POS_TYPE			0x04
#define MMSD_POS_SUBTYPE		0x05
#define MMSD_POS_SIZE			0x06

/* MMSD Chunk types */
#define MMSD_HEADER_CHUNK 		0x02
#define MMSD_DATA_CHUNK   		0x04

unsigned char MMSC_HEADER[] = {
	0x01,0x00,0x00,0x00,0xCE,0xFA,0x0B,0xB0, 0x00,0x00,0x00,0x00,0x4D,0x4D,0x53,0x20};

unsigned char MMSC_REQUEST_INIT[] = {
	0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x12,0x00,0x00,0x00,0x01,0x00,0x03,0x00, 0xF0,0xF0,0xF0,0xF0,0x0B,0x00,0x04,0x00,
	0x1C,0x00,0x03,0x00};

unsigned char MMSC_REQUEST_CONNPORT[] = {
	0x0B,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,
	0x09,0x00,0x00,0x00,0x02,0x00,0x03,0x00, 0xF0,0xF0,0xF0,0xF0,0xFF,0xFF,0xFF,0xFF,
    0x00,0x00,0x00,0x00,0x00,0x00,0xA0,0x00, 0x02,0x00,0x00,0x00};

unsigned char MMSC_REQUEST_FILE[] = {
	0x08,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,
	0x06,0x00,0x00,0x00,0x05,0x00,0x03,0x00, 0x01,0x00,0x00,0x00,0xFF,0xFF,0xFF,0xFF,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

unsigned char MMSC_REQUEST_RECVHEADER[] = {
	0x09,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,
	0x07,0x00,0x00,0x00,0x15,0x00,0x03,0x00, 0x01,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0x00,0x00,0x00,0x00,0x00,0x80,0x00,0x00, 0xFF,0xFF,0xFF,0xFF,0x64,0x00,0x79,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x20,0xAC,0x40,
	0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00};

unsigned char MMSC_REQUEST_RECVBODY[] = {
	0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,
	0x05,0x00,0x00,0x00,0x07,0x00,0x03,0x00, 0x01,0x00,0x00,0x00,0xFF,0xFF,0x01,0x00,
	0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
	0xFF,0xFF,0xFF,0x80,0x04,0x00,0x00,0x00};

unsigned char MMSC_REPLY[] = {
	0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00, 0x00,0x00,0x00,0x00,0x00,0x00,0xE0,0x3F,
	0x02,0x00,0x00,0x00,0x1B,0x00,0x03,0x00, 0x01,0x00,0x00,0x00,0xFF,0xFF,0x01,0x00};

#endif
