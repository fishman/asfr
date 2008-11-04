/*

	helper.h

	Definitions for the helper.cxx modules

*/

#ifndef HELPER_HPP
#define HELPER_HPP

typedef enum
{
	ShowUsage,
	NoArguments,
	BadOption,
}
UsageMode;

void gui_showstatus(unsigned char StatusType, char *Message, ...);
void ctrlc(int sig);
int myfclose(FILE **stream);
void Usage(char *progname, UsageMode mode);
void gui_not_idle(int flag);
void unescape_url_string(char *outbuf, char *inbuf);
void generate_valid_filename(char *buffer);

#endif // HELPER_HPP
