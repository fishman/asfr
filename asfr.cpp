/******************************************************************************\
asfr.c - by Yet Another ANonymous^2, so no one is to blame too.
\******************************************************************************/

/* global flag for exiting the download */
unsigned char AbortFlag = 0;
unsigned char Debug = 0;

#include "asfr.h"

void core_mmsh(struct JOB_PARM *My_Job);
void core_mmst(struct JOB_PARM *My_Job);
void createjob(struct JOB_PARM *My_Parm);

/* A simple main() code that works for both console and GUI application */
int main(int argc, char **argv)
{
#ifdef WIN32
	WSADATA wsaData;
#endif

#ifdef WIN32

	/* Open Winsock */
#define WS_MAJOR 2
#define WS_MINOR 0
	if (WSAStartup(((WS_MINOR<<8)+WS_MAJOR),&wsaData) != 0)
	{
		gui_showstatus(STATUS_CRITICALERROR, "WSAStartup failed: %d\n", errno);
		WSACleanup();
		return -1;
	}
#endif
	main_function(argc, argv);

#ifdef WIN32
	WSACleanup();
#endif

	return 0;
}

int main_function(int argc, char **argv)
{
	struct JOB_PARM Job_Parm;
	int i;
	int err = 0;

	/* Setup CTRL-C handler */
	signal(SIGINT, &ctrlc);

	/* set some defaults */
	memset(&Job_Parm, 0, sizeof(Job_Parm));

	/* Argument parsing */
	if (argc > 1)
	{
		for( i=1 ; !(err%1000) && (i < argc) ; i++)
		{
			if ( (argv[i][0] == '-') || (argv[i][0] == '/') )
			{
				switch(argv[i][1])
				{
				case 'a':
					if (argc > i+1)
					{
						char *colonptr;
						colonptr = strchr(argv[++i],':');
						if (colonptr == NULL)
						{
							gui_showstatus(STATUS_CRITICALERROR, "Invalid syntax for username:password! Forgot colon?\n");
							err++;
						}
						else
						{
							*colonptr = 0;
							strcpy(Job_Parm.Username, argv[i]);
							strcpy(Job_Parm.Password, colonptr+1);
						}
					}
					else {
						gui_showstatus(STATUS_CRITICALERROR, "insufficient args\n");
						err++;
					}
					break;
				case 'P':
					if (argc > i+1)
						strcpy(Job_Parm.Proxy, argv[++i]);
					else {
						gui_showstatus(STATUS_CRITICALERROR, "insufficient args\n");
						err++;
					}
					break;
				case 'm':
					if (argc > i+1)
						Job_Parm.MaxTime = atoi(argv[++i]);
					else {
						gui_showstatus(STATUS_CRITICALERROR, "insufficient args\n");
						err++;
					}
					break;
				case 't':
					if (argc > i+1) {
						Job_Parm.MaxThreads = atol(argv[++i]);
					}
					else {
						gui_showstatus(STATUS_CRITICALERROR, "insufficient args\n");
						err++;
					}
					break;
				case 'T':
					Job_Parm.ThreadID = 1;
					break;
				case 'd':
					Debug = 1;
					break;
				default:
					err++;
					break;
				}
			}
			else
			    {
				strcpy(Job_Parm.URL, argv[i]);
				err+=10000;
			}
		}
	}

	/* Print program usage if no url found */
	if (err != 10000)
	{
		Usage(argv[0], ShowUsage);
		return 0;
	}

	/* Create a Job */
	createjob(&Job_Parm);

	return 0;
}

void createjob(struct JOB_PARM *My_Job)
{
	int i, Thread;
	void (*core)(void*) = NULL;
	struct JOB_PARM My_RJob;
	struct HEADER_INFO hi;
	char FileName[256];
	char *FilePtr, *TmpPtr;

	memcpy(&My_RJob, My_Job, sizeof(My_RJob));
	My_Job = &My_RJob;

	/* Initialize status file */
	initstatus(My_Job);

	/* Initialize data structure */
	My_Job->OutFile = NULL;
	My_Job->hi = &hi;
	My_Job->ci = NULL;
	memset(&hi, 0, sizeof(hi));

	/* get file information (Try all core to find one that can handle the file) */
	My_Job->ti[MAX_THREADS] = (struct THREAD_INFO *)malloc(sizeof(struct THREAD_INFO));

	if(My_Job->ThreadID == 1) /* temperary use ThreadID */
	{
		if(strlen(My_Job->Proxy))
		{
			gui_showstatus(STATUS_CRITICALERROR, "Proxy is invalid in MMST mode!\n");
			return;
		}
		else My_Job->ti[MAX_THREADS]->Reply = REPLY_MMST;
	}
	else My_Job->ti[MAX_THREADS]->Reply = REPLY_MMSH;

	while( My_Job->ti[MAX_THREADS]->Reply != REPLY_OK )
	{
		unsigned char ToTry = 0;

		switch(My_Job->ti[MAX_THREADS]->Reply)
		{
			case REPLY_HTTP:
                                gui_showstatus(STATUS_INFORMATION, "This is a pure HTTP content, use any HTTP downloader to download it!\n");
				return;	/* No core_http yet */

			case REPLY_MMSH:
				core = (void(*)(void*))core_mmsh;
				ToTry = 1;
				break;

			case REPLY_MMST:
				core = (void(*)(void*))core_mmst;
				ToTry = 1;
				break;

			case REPLY_RETRY:
				ToTry = 1;
				break;

			case REPLY_FAIL:
				return;

			default:
				My_Job->ti[MAX_THREADS]->Reply = REPLY_FAIL;
				break;
		}
		if(ToTry)
		{	/* Use the MAX_THREAD+1 thread to avoid data overwrite */
			My_Job->ThreadID = MAX_THREADS;
			memset(My_Job->ti[MAX_THREADS], 0, sizeof(struct THREAD_INFO));

			My_Job->ti[MAX_THREADS]->RequestType = REQUEST_INIT;
			core(My_Job);
			if(My_Job->ci != NULL) free(My_Job->ci);	/* Free allocated memory */
			My_Job->ci = (char*)malloc(My_Job->ciSize);

			(My_Job->ti[MAX_THREADS])->RequestType = REQUEST_RESET;
			core(My_Job);

			(My_Job->ti[MAX_THREADS])->RequestType = REQUEST_INFORMATION;		/* get file information */
			core(My_Job);
		}

		if(AbortFlag) return;
	}
	free(My_Job->ti[MAX_THREADS]);

	if ( ((My_Job->hi)->RequirePassword == TRUE) && (My_Job->Username != NULL || My_Job->Password != NULL) )
	{
		gui_showstatus(STATUS_CRITICALERROR, "Authorization failed!\n");
		return;
	}

	/* Cut away leading sub directories from the URL and extract the file name */
	for (FilePtr = My_Job->URL, TmpPtr = FilePtr; (TmpPtr = strchr(TmpPtr, '/')) != NULL ; FilePtr = ++TmpPtr);

	unescape_url_string(FileName, FilePtr);

	generate_valid_filename(FileName);

	/* Open output file */
	if(Debug) gui_showstatus(STATUS_INFORMATION, "Opening file '%s' for output\n", FileName);

	/* try to open existing file for binary reading and writing */
	if (hi.Resumeable == TRUE) My_Job->OutFile = fopen(FileName, "rb+");

	if (My_Job->OutFile == NULL) My_Job->OutFile = fopen(FileName, "wb");

	if (My_Job->OutFile == NULL) /* still failed, something wrong */
	{
		gui_showstatus(STATUS_CRITICALERROR, "Failed to open '%s'.\n", FileName);
		return;
	}

	{ /* Determine file size of existing .ASF file */
		unsigned long OldSize;
		fseek(My_Job->OutFile, 0, SEEK_END);
		OldSize = ftell(My_Job->OutFile);
		if (OldSize) gui_showstatus(STATUS_INFORMATION, "Appending to existing file!\n");

		My_Job->SpecOffset = max((signed)OldSize-RESUME_REWIND, 0);
		My_Job->SpecLength = hi.FileSize - My_Job->SpecOffset;
	}

	int StatusReturn = 0;
	int Finished = 0;
#ifdef __MINGW32__  /* Win32 threads */
	HANDLE ThreadHandle[MAX_THREADS];
	DWORD ThreadID;
#else
	pthread_t ThreadHandle[MAX_THREADS];
#endif
	for(Thread=0 ; 1 ; Thread++)
	{
		if(Thread == My_Job->MaxThreads) Thread = 0;  /* Loop through all threads */
		My_Job->ThreadID = Thread;						/* ID number for this thread */

		/* Call collectdata that does all the work */
		StatusReturn = status(My_Job);
		if(Debug) printf("StatusReturn: ID:%d Return:%d\n", Thread, StatusReturn);

		switch(StatusReturn)
		{
		    	case 0: /* Nothing */
				break;
			case 255: /* status() asked for execute this thread again ( to really start or kill it ) */
				Thread--;
				break;

			case 3: /* Restart me */
#ifdef __MINGW32__
                TerminateThread(ThreadHandle[Thread], 0);
#else
                pthread_kill( ThreadHandle[Thread], SIG_TO_KILL_THREAD );
#endif
				/* Fall through */

			case 1:
				My_Job->ti[Thread]->RequestType = REQUEST_GETDATA;			/* request for get data */
#ifdef __MINGW32__
				ThreadHandle[Thread] = CreateThread(NULL, 0, (DWORD (*)(void *))core, My_Job, 0, &ThreadID);
#else
				pthread_create( &ThreadHandle[Thread], NULL, (void * (*)(void *))core, My_Job );
#endif
				if(Debug) gui_showstatus(STATUS_INFORMATION, "Thread Spawned! ID:%d sour_off:%ld dest_len:%ld\n",
					Thread,My_Job->ti[Thread]->SourOffset,My_Job->ti[Thread]->DestLength);
				break;

			case 2: /* Kill me */
#ifdef __MINGW32__
				TerminateThread(ThreadHandle[Thread], 0);
#else
                pthread_kill( ThreadHandle[Thread], SIG_TO_KILL_THREAD );
#endif
			   	Finished++;
				break;
		}

		if(Finished >= My_Job->MaxThreads)
		{
			break;
		}

		if(AbortFlag)
		{
			for(i=0 ; i<My_Job->MaxThreads ; i++) {
				My_Job->ti[i]->Message = MESSAGE_EXIT;
#ifdef __MINGW32__
				TerminateThread(ThreadHandle[i], 0);
#else
                pthread_kill( ThreadHandle[Thread], SIG_TO_KILL_THREAD );
#endif
			}
            savestatus(My_Job);
			AbortFlag = 0;
			break;
		}
		usleep(500);
	}

	My_Job->ti[0]->RequestType = REQUEST_FINISH; 	/* Fix some data */
	My_Job->ThreadID = 0;
	core(My_Job);

	/* Close files, cleanup and exit */
        myfclose( & My_Job->OutFile);
	free(My_Job->ci);
}
