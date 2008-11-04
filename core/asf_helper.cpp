#include "../asfr.h"

#include "asf_commonh.h"
#include "asf_helper.h"

unsigned char ASF_HDR_ID[16] = {
	0xa1,0xdc,0xab,0x8c,0x47,0xa9,0xcf,0x11,0x8e,0xe4,0x00,0xc0,0x0c,0x20,0x53,0x65};

/* get 64bit integer (Little endian) */
void get_quad(unsigned char *pos, unsigned long *hi, unsigned long *lo)
{
	unsigned char c1 = *pos++, c2 = *pos++, c3 = *pos++, c4 = *pos++;
	unsigned char c5 = *pos++, c6 = *pos++, c7 = *pos++, c8 = *pos++;
	(*hi) = (c8<<24) + (c7<<16) + (c6<<8) + c5;
	(*lo) = (c4<<24) + (c3<<16) + (c2<<8) + c1;
}

/* write 64bit integer to stream (Little endian) */
void write_quad(FILE *outfile, unsigned int hi, unsigned int lo)
{
	unsigned char c1 = (lo & 0xff), c2 = ((lo>>8) & 0xff), c3 = ((lo>>16) & 0xff), c4 = ((lo>>24) & 0xff);
	unsigned char c5 = (hi & 0xff), c6 = ((hi>>8) & 0xff), c7 = ((hi>>16) & 0xff), c8 = ((hi>>24) & 0xff);
	fputc(c1, outfile);
	fputc(c2, outfile);
	fputc(c3, outfile);
	fputc(c4, outfile);
	fputc(c5, outfile);
	fputc(c6, outfile);
	fputc(c7, outfile);
	fputc(c8, outfile);
}

/* get 32bit integer (Little endian) */
void get_long(unsigned char *pos, unsigned long *val)
{
	unsigned char c1 = *pos++, c2 = *pos++, c3 = *pos++, c4 = *pos++;
	(*val) = (c4<<24) + (c3<<16) + (c2<<8) + c1;
}

/* put 32 bit integer (Little endian) */
void put_long(unsigned char *pos, unsigned long val)
{
	*pos++ = (val & 0xff);
	*pos++ = ((val>>8) & 0xff), *pos++ = ((val>>16) & 0xff), *pos++ = ((val>>24) & 0xff);
}

/* write a 32 bit integer to stream (Little endian) */
void write_long(FILE *outfile, unsigned long val)
{
	unsigned char c1 = (val & 0xff), c2 = ((val>>8) & 0xff), c3 = ((val>>16) & 0xff), c4 = ((val>>24) & 0xff);
	fputc(c1, outfile);
	fputc(c2, outfile);
	fputc(c3, outfile);
	fputc(c4, outfile);
}

/* get short integer (Little endian) */
void get_short(unsigned char *pos, unsigned short *val)
{
	unsigned char c1 = *pos++, c2 = *pos++;
	(*val) = (c2<<8) + c1;
}

/* write a 32 bit integer to stream (Little endian) */
void write_short(FILE *outfile, unsigned int val)
{
	unsigned char c1 = (val & 0xff), c2 = ((val>>8) & 0xff);
	fputc(c1, outfile);
	fputc(c2, outfile);
}

/* 64 bit subtraction with unsigned integers */
void quad_subtraction(unsigned int *dst_hi, unsigned int *dst_lo, unsigned int hi, unsigned int lo)
{
	unsigned int tmp;

	*dst_hi -= hi;

	tmp = *dst_lo;
	*dst_lo -= lo;

	/* handle "carry bit" */
	if (*dst_lo > tmp) *dst_hi--;
}

/* timecode helper functions */
/* get position of timecode in chunk (-1 if unknown) */
int whereis_timecode(unsigned char *Buffer)
{
	int tc_start = -1;
	/* timecode position varies with type of chunk */
	switch(Buffer[3])
	{
		case 0x00:	tc_start =  5;	break;
		case 0x01:	tc_start =  5;	break;
		case 0x08:	tc_start =  6;	break;
		case 0x09:	tc_start =  6;	break;
		case 0x10:	tc_start =  7;	break;
		case 0x11:	tc_start =  7;	break;
		case 0x48:	tc_start =  8;	break;
		case 0x49:	tc_start =  8;	break;
		case 0x50:	tc_start =  9;	break;
		case 0x51:	tc_start =  9;	break;
		default:	tc_start = -1;	gui_showstatus(STATUS_ERROR, "unknown format type: $%02x\n", (int)Buffer[3]);
	}
	return tc_start;
}

/* Create a randomized GUID string to avoid filtering ASFRecorder's */
/* stream requests by means of detecting any hardcoded GUIDs */
void randomize_guid(unsigned char *buf)
{
	int digit, dig;
	time_t curtime;

	*buf++='{';
	time(&curtime);
	srand(curtime);
	for (digit=0; digit <32; digit++)
	{
		if (digit==8 || digit == 12 || digit == 16 || digit == 20) *buf++='-';
		dig = rand()%0xf;
		if (dig<10)
			*buf++='0'+dig;
		else
			*buf++='A'+(dig-10);
	}
	*buf++='}';
	*buf++='\0';
}

/* create a timecode string from a timecode in milliseconds */
char *createtimestring(unsigned int timecode)
{
	static char timecodestring[64];

	if (timecode/1000 >= 24*3600)
		sprintf(timecodestring, "%d:%02d:%02d:%02d.%03d", (timecode/1000)/(24*3600), ((timecode/1000)/3600)%24, ((timecode/1000)/60)%60, (timecode/1000)%60, timecode%1000);
	else
		if (timecode/1000 >= 3600)
			sprintf(timecodestring, "%d:%02d:%02d.%03d", (timecode/1000)/3600, ((timecode/1000)/60)%60, (timecode/1000)%60, timecode%1000);
		else
			if (timecode/1000 >= 60)
				sprintf(timecodestring, "%d:%02d.%03d", (timecode/1000)/60, (timecode/1000)%60, timecode%1000);
			else
				sprintf(timecodestring, "%d.%03d", (timecode/1000)%60, timecode%1000);
	return timecodestring;
}

/* subroutine for base64 encoding (used in HTTP Basic authentification) */
int base64enc(char *data, unsigned char *out)
{
	char alphabet[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/=";
	int len = strlen(data);
	int i,index;
	int val;

	for (i=0, index=0; i<len; i+=3, index+=4)
	{
		int quad = 0;
		int trip = 0;

		val = (0xff & data[i]);
		val <<= 8;
		if ((i+1) < len) {
			val |= (0xff & data[i+1]);
			trip = 1;
		}
		val <<= 8;
		if ((i+2) < len) {
			val |= (0xff & data[i+2]);
			quad = 1;
		}
		out[index+3] = alphabet[(quad? (val & 0x3f): 64)];
		val >>= 6;
		out[index+2] = alphabet[(trip? (val & 0x3f): 64)];
		val >>= 6;
		out[index+1] = alphabet[val & 0x3f];
		val >>= 6;
		out[index+0] = alphabet[val & 0x3f];
	}

	out[++index] = 0;
	return index;
}

/* helper routine for ASF header parsing */
int find_id(unsigned char *id, unsigned char *Buffer, int bufsize)
{
	int offs, i, found;
	for (offs = 0; offs < bufsize-16; offs++) {
		found=1;
		for (i=0; i<16; i++) if (Buffer[offs+i] != id[i]) {
			found = 0;
			break;
		}
		if (found) return(offs);
	}
	return -1;
}

/* These routines are used to parse the packet and find */
/* and modify timecodes for live streams */
unsigned int get_data(unsigned char **bp, unsigned int type)
{
	unsigned int res = 0;
	switch (type) {
	case 0:
		break;
	case 1:
		res = *(*bp+0);
		*bp+=1;
		break;
	case 2:
		res = *(*bp+0) + (*(*bp+1) << 8);
		*bp+=2;
		break;
	case 3:
		res = *(*bp+0) + (*(*bp+1) << 8) + (*(*bp+2) << 16) + (*(*bp+3) << 24);
		*bp+=4;
		break;
	default:
		gui_showstatus(STATUS_CRITICALERROR, "unknown format type %d\n", type);
		break;
	}
	return res;
}

/* Replace specific characters in the URL string by an escape sequence */
/* works like strcpy(), but without return argument */
void escape_url_string(char *outbuf, char *inbuf)
{
	unsigned char c;
	do
	    {
		c = *inbuf++;

		if ((c >= 'A' && c <= 'Z') ||
		    (c >= 'a' && c <= 'z') ||
	   		(c >= '0' && c <= '9') ||
			(c >= 0x7f) ||										/* fareast languages(Chinese, Korean, Japanese) */
			c=='-' || c=='_'  || c=='.' || c=='!' || c=='~'  || /* mark characters */
			c=='*' || c=='\'' || c=='(' || c==')' || c=='%'  || /* do not touch escape character */
			c==';' || c=='/'  || c=='?' || c==':' || c=='@'  || /* reserved characters */
			c=='&' || c=='='  || c=='+' || c=='$' || c==','  || /* see RFC 2396 */
			c=='\0' )
		{
			*outbuf++ = c;
		}
		else
		    {
			/* all others will be escaped */
			unsigned char c1 = ((c & 0xf0) >> 4);
			unsigned char c2 =  (c & 0x0f);
			if (c1 < 10) c1+='0';
			else c1+='A';
			if (c2 < 10) c2+='0';
			else c2+='A';
			*outbuf++ = '%';
			*outbuf++ = c1;
			*outbuf++ = c2;
		}
	}
	while (c != '\0');
}

int fix_timecodes(unsigned char *Buffer, int bodylength, unsigned int deltatime, unsigned int in_chunk_no, struct CUSTOM_INFO_ASF *ci)
{
	unsigned char *ptr1,b1;
	unsigned int	f1, flag_more_than_one_segment,	flag_packet_size, flag_padding_size, flag_chunk_no, l4, l5, l6, l7, l8, segments,	n;
	unsigned long 	packet_size, chunk_no, padding_size, send_time,	v5=0, v6, v7, v8, v9, v10;

	if ((Buffer[0]!=0x82) || (Buffer[1]!=0x00) || (Buffer[2]!=0x00))
	{
		gui_showstatus(STATUS_ERROR, "Illegal header bytes in chunk %d!\n",in_chunk_no);
		return 1;
	}

	/* Get flags from Packet Flags */
	ptr1 = &Buffer[3];
	b1 = *ptr1++;						/* Flags */
	f1 = (b1&0x80) >> 7;					/* 0x80 bit */
	if (f1 == 0)
	{
		flag_packet_size = (b1&0x60) >> 5;			/* 0x40, 0x20 bit, presence of packet size*/
		flag_padding_size = (b1&0x18) >> 3;			/* 0x10, 0x08 bit, presence of padding */
		flag_chunk_no = (b1&0x06) >> 1;				/* 0x04, 0x02 bit, chunk No. */
		flag_more_than_one_segment = (b1&0x01);		/* 0x01 bit */
	}
	else
	{
		gui_showstatus(STATUS_ERROR, "Chunks carrying error correction information are not supported yet!\n");
		return 1;
	}

	/* Get flags from Segment Type ID */
	b1 = *ptr1++;							/* Buffer[4],Segment type ID */
	l4 = (b1&0xc0) >> 6;					/* 0x80 , 0x40 bit */
	l5 = (b1&0x30) >> 4;					/* 0x20 , 0x10 bit */
	l6 = (b1&0x0c) >> 2;					/* 0x08 , 0x0c bit */
	l7 = (b1&0x03);							/* 0x02 , 0x01 bit */

	/* Parse info from Packet Flags */
	packet_size = get_data(&ptr1,flag_packet_size);				/* packet size */
	chunk_no = get_data(&ptr1,flag_chunk_no);					/* chunk number */
	padding_size = get_data(&ptr1,flag_padding_size);			/* padding size */
	send_time = get_data(&ptr1,3);								/* send time */

	/* Fix send time if needed */
	send_time = (deltatime < send_time) ? send_time -= deltatime : 0;
	put_long(ptr1 - 4,send_time);

	if (flag_packet_size == 0)
	    packet_size = ci->ChunkLength;
	if (flag_chunk_no == 0)
	    chunk_no = in_chunk_no;
	if (padding_size > 0)
		memset(Buffer + packet_size - padding_size, 0, padding_size);

	ptr1 += 2;													/* Duration,skipped */
	b1 = *ptr1++;												/* Number of Segments & Segment properties */

	if (flag_more_than_one_segment)
	{
		l8 = 2;
		segments = b1 & 0x1f;
	}
	else
	{
		l8 = 0;
		segments = 1;
	}

	for (n=0 ; (n<segments) ; n++)
	{
		if (flag_more_than_one_segment)
			v5 = get_data(&ptr1,l4);
		v6 = get_data(&ptr1,l5);
		v7 = get_data(&ptr1,l6);
		v8 = get_data(&ptr1,l7);

		if (v8 == 8)
		{
			get_long(ptr1+4,&v10);
			v10 = (deltatime < v10) ? v10-=deltatime : 0;
			put_long(ptr1+4,v10);
		}

		if (v8 > 0)
			ptr1 += v8;

		if (flag_more_than_one_segment)
			v9 = get_data(&ptr1,l8);
		else
			v9=(packet_size-padding_size)-(ptr1-Buffer);

		if (v9 > 0)
			ptr1+=v9;

		if ((unsigned int)(ptr1-Buffer) > ci->ChunkLength)
		{
			gui_showstatus(STATUS_ERROR, "WARNING! buffer pointer %d bytes after packet!\n",	(unsigned int)(ptr1-Buffer)-ci->ChunkLength);
			return 1;
		}

		if ((unsigned int)(ptr1-Buffer) > ci->ChunkLength - padding_size)
		{
			gui_showstatus(STATUS_ERROR, "WARNING! buffer pointer %d bytes after packet-padlen!\n", (unsigned int)(ptr1-Buffer)-(ci->ChunkLength - padding_size));
			return 1;
		}
	}

	if ((ptr1-Buffer)<(int)ci->ChunkLength-(int)padding_size)
		gui_showstatus(STATUS_ERROR, "NOTE: %d bytes not covered by payload\n",ci->ChunkLength-padding_size-(ptr1-Buffer));
	return 0;
}

int my_sprintf(unsigned char *pos, unsigned char *CWord, int number)
{
	//if(number==0) number = ustrlen(CWord);
	if(number==0) number = strlen((char *)CWord);
	memcpy(pos, CWord, number);
	return number;
}

int my_swprintf(unsigned char *pos, const char *CWord, int number)
{
#ifdef __WIN32__
	int count;
	if(number==0) number = strlen(CWord);
	count = MultiByteToWideChar(CP_ACP, MB_PRECOMPOSED, CWord, number, (WCHAR*)pos, number);
	return count*2;

#else /* iconv way */
	int i, count;
	unsigned short *SWord;
	if(number==0) number = strlen(CWord);
	SWord = (unsigned short *)malloc(number*2);
	for(i=0, count=0 ; i<number ; i++)
	{
		if(((unsigned char)CWord[i] > 0x7f) && ((unsigned char)CWord[i+1] > 0x7f))
		{
			SWord[count++] = CWord[i+1]*256 + CWord[i];
			i++;
		}
		else
		{
			SWord[count++] = CWord[i];
		}
	}
	memcpy(pos, SWord, count*2);
	free(SWord);
	return count*2;

#endif /* __WIN32__ */
}
