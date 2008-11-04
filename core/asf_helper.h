/*
	Header file for module asf_helper.cxx


*/

#ifndef ASF_HELPER_HPP
#define ASF_HELPER_HPP

void randomize_guid(unsigned char *buf);
void escape_url_string(char *outbuf, char *inbuf);
int base64enc(char *data, unsigned char *out);

void get_long(unsigned char *pos, unsigned long *val);
void write_long(FILE *outfile, unsigned long val);
void put_long(unsigned char *pos, unsigned long val);

void get_short(unsigned char *pos, unsigned short *val);

int find_id(unsigned char *id, unsigned char *Buffer, int bufsize);

void get_quad(unsigned char *pos, unsigned long *hi, unsigned long *lo);
void write_quad(FILE *outfile, unsigned int hi, unsigned int lo);

int whereis_timecode(unsigned char *Buffer);
int fix_timecodes(unsigned char *Buffer, int bodylength, unsigned int deltatime, unsigned int in_chunk_no, struct CUSTOM_INFO_ASF *ci);
char *createtimestring(unsigned int timecode);

int my_sprintf(unsigned char *pos, unsigned char *CWord, int number);
int my_swprintf(unsigned char *pos, const char *CWord, int number);

#endif // ASF_HELPER_HPP
