#ifndef __ASFR_H_
#define __ASFR_H_

/* Standard ANSI-C headers */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <stdarg.h>
#include <time.h>

#ifdef __WIN32__
#include "win32.h"
#endif

#ifdef __MINGW32__
#include "mingw32.h"
#endif

#ifdef __LINUX__
#include "linux.h"
#endif

#include "helper.h"
#include "status.h"

//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////

/* maximum number of threads */
#define MAX_THREADS 3
#define MAX_URLS 100
#define RESUME_REWIND 10240

/* Status types */
#define STATUS_INFORMATION 		1
#define STATUS_ERROR 			2
#define STATUS_CRITICALERROR 	3
#define STATUS_PROGRESS 		4
#define STATUS_FINISH			5
#define STATUS_RESET 			100

#define EXIT_ERROR 				1
#define EXIT_FINISH 			2
#define EXIT_RETRY 				3
#define EXIT_INFORM 			4

#define REQUEST_INIT			1
#define REQUEST_RESET			2
#define REQUEST_INFORMATION 	3
#define REQUEST_GETDATA 		4
#define REQUEST_FINISH 			5

#define MESSAGE_EXIT			1	/* Send by ctrl-c */
#define MESSAGE_PAUSE			2	/* Not used yet */
#define MESSAGE_CONTINUE		3	/* Not used yet */
#define MESSAGE_CANCEL			4	/* Send by status() to respawn thread */

#define REPLY_OK				1
#define REPLY_FAIL				2
#define REPLY_RETRY				3
#define REPLY_FINISH			4
#define REPLY_SEGMENTFINISH		5
#define REPLY_EXIT				6
#define REPLY_HTTP				100
#define REPLY_MMST				101
#define REPLY_MMSH				102

struct JOB_PARM  /* Job parameters (Unique in each job) */
{
	FILE                	*OutFile;
	FILE			*StaFile;
	unsigned long			StaDataPtr;
	int			Mutex;

	char			URL[512];
	char			Proxy[256];
	char			Username[128];
	char			Password[128];
	char			StaFileName[256];
	int 			MaxThreads;
	int 			MaxTime;
	long 			SpecOffset;
	long 			SpecLength;

	unsigned int   	        ThreadID;
	unsigned short			ciSize;
	struct THREAD_INFO		*ti[MAX_THREADS];
	int						NoUse;		/* padding for data alignment */
	struct HEADER_INFO   	*hi;
	char					*ci;
};

struct HEADER_INFO	/* Basic file info (Unique in each job) */
{
	int					RequirePassword;
	int					Resumeable;
	unsigned long 			FileSize;
};

struct THREAD_INFO /* Thread parameters (Unique in each thread) */
{
	unsigned long 			SourOffset;
	unsigned long 			SourLength;
	unsigned long 			DestOffset;
	unsigned long 			DestLength;

	unsigned char			RequestType;
	unsigned char			Message;
	unsigned char			Reply;

};

// Some global flags.
extern unsigned char AbortFlag;
extern unsigned char Debug;

/* Prototypes for this source code module */
int main_function(int argc, char **argv);

#endif
