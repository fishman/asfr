#include "asfr.h"
#include "helper.h"

/* Stub routines for non-existent GUI code. */
int gui_start_transmission(char *url, char *filename, int filenamesize, unsigned int totaltime, unsigned int maxtime)
{
	return 1;
}

void gui_progressinfo(int bytes, char *timecode, int progress, int seqno, int currenttime)
{
	//    printf("%5d kB (%2d%%), tc: %s, seq: %d\n", bytes/1024, progress/100, timecode, seqno);
}

void gui_modify_duration(unsigned int totaltime)
{
}

void gui_finished_transmission()
{
}

void gui_prepareasyncsocket(SOCKET s)
{
}

void gui_restoresyncsocket(SOCKET s)
{
}

void gui_return_on_network_activity()
{
}

void gui_return_on_network_connect(int *retval)
{
}

int gui_nonblocking_socket_check(int num)
{
	return 0;
}

void gui_waitforuseraction(void)
{
}

void gui_showstatus(unsigned char StatusType, char *Message, ...)
{
	switch(StatusType)
	{
	default:
		{
			va_list args;
			va_start(args, Message);
			vprintf(Message, args);
			va_end(args);
			break;
		}
	}
}

/* Replace escape sequences in an URL (or a part of an URL) */
/* works like strcpy(), but without return argument */
void unescape_url_string(char *outbuf, char *inbuf)
{
	unsigned char c;
	do
	    {
		c = *inbuf++;
		if (c == '%')
		{
			unsigned char c1 = *inbuf++;
			unsigned char c2 = *inbuf++;
			if (((c1>='0' && c1<='9') || (c1>='A' && c1<='F')) &&
				    ((c2>='0' && c2<='9') || (c2>='A' && c2<='F')) )
			{
				if (c1>='0' && c1<='9') c1-='0';
				else c1-='A';
				if (c2>='0' && c2<='9') c2-='0';
				else c2-='A';
				c = (c1<<4) + c2;
			}
		}
		*outbuf++ = c;
	}
	while (c != '\0');
}

/* generate a valid filename ,NOTE: this currently implements proper rules for Windows only */
void generate_valid_filename(char *buffer)
{
	char *inbuf = buffer, *outbuf = buffer;
	unsigned char c;
	int length = 0;
	do
	    {
		c = *inbuf++;
		if (
	#ifdef __WIN32__
		/* Windows does not allow the following characters in file names */
		/* and filename length (including '\0') is restricted to 255 characters */
		(c!='/' && c!='\\' && c!=':' && c!='*' && c!='?' && c!='"' && c!='<' && c!='>' && c!='|' && length < 254) || c=='\0'
	#else
	    /* Unix disallow forward slash (directory separator)... anything else?!?! */
		c!='/'
	#endif
		    )
		{
			*outbuf++ = c;
			length++;
		}
	}
	while (c != '\0');
}

/* ctrl-c handler */
/* WIN32: Beware! The ctrl-c handler is called in a different thread. */
void ctrlc(int sig)
{
	gui_showstatus(STATUS_ERROR, "\n***CTRL-C\n");
	AbortFlag = 1;
	signal(SIGINT, ctrlc);
}

/* safe fclose() */
int
myfclose(FILE **stream)
{
        int retval = EOF;
        if ( NULL != *stream ) {
                retval = fclose(*stream);
                *stream = NULL;
        }
        return retval;
}

/* Program usage information */
void Usage(char *progname, UsageMode mode)
{
	char Buffer[1024];
	char *bufptr;
	char *nameptr, *tmpptr;

	for (nameptr = progname, tmpptr = progname; (tmpptr = strchr(tmpptr, '\\')) != NULL ; nameptr = ++tmpptr);

	bufptr = Buffer;
	bufptr+=sprintf(bufptr,"ASFR+ b5.21                      http://yaan2.linux.net.cn/\n\n");
	bufptr+=sprintf(bufptr,"%s [-T] [-d] [-a <user:passwd>] [-P <proxy:port>] [-t <threads>]\n", nameptr);
	bufptr+=sprintf(bufptr,"[-m <minutes>] <one or more stream references>\n");
	bufptr+=sprintf(bufptr,"\n");
	bufptr+=sprintf(bufptr,"    -a authorization with username and password, ONLY IN HTTP MODE!\n");
	bufptr+=sprintf(bufptr,"    -P specifies a proxy (e.g. http://proxy:8080), ONLY IN HTTP MODE!\n");
	bufptr+=sprintf(bufptr,"    -m specifies max. recording time in minutes.\n");
	bufptr+=sprintf(bufptr,"    -t specifies threads,default 1,max 3\n");
	bufptr+=sprintf(bufptr,"    -T force download in MMS(TCP) protocol.\n");
	bufptr+=sprintf(bufptr,"    -d show more debug info.\n");
	bufptr+=sprintf(bufptr,"\n");
	bufptr+=sprintf(bufptr,"    Hit Ctrl-C or Ctrl-Break to terminate the download.\n\n");
	gui_showstatus(STATUS_INFORMATION, Buffer);
}
