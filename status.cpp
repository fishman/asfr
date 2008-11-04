#include "asfr.h"
#include "status.h"

#define MIN_INSERT_SIZE 204800	/* 200k */
#define MAX_SAVE_COUNT	60
#define MAX_RESPAWN_COUNT	60



int status(struct JOB_PARM *My_Job)
{
	static int Counter = MAX_SAVE_COUNT;
	static int ThreadCounter[MAX_THREADS];
	static unsigned long LastLength[MAX_THREADS];

	int i, ReturnCode = 0; /* Do nothing for default */
	int MyID = My_Job->ThreadID;
	int MaxThreads = My_Job->MaxThreads;

	if( ThreadCounter[MyID] == -1 ) return ReturnCode;	/* This thread has finished */

	if( My_Job->ti[MyID]->DestLength ) /* Current thread have something to do */
	{
		if( Counter++ >= MAX_SAVE_COUNT ) 	/* Save status */
		{
			Counter = 0;	/* Reset counter */
			savestatus(My_Job);
		}

		if( (ThreadCounter[MyID] > MAX_RESPAWN_COUNT) || (ThreadCounter[MyID] == 0) )
		{
			if(ThreadCounter[MyID] == 0) /* This is the initial loop, the thread has not started yet */
				ReturnCode = 1;  /* Start it */

			ThreadCounter[MyID] = 1; /* Reset counter */

			if( (LastLength[MyID] == 0) || (LastLength[MyID] > My_Job->ti[MyID]->DestLength) ) /* Update? */
			{
			    LastLength[MyID] = My_Job->ti[MyID]->DestLength;
			}
			else /* This thread seems to be frozen, restart it */
			{
				printf("Thread frozen, respawn......\n");
				return 3;	/* Kill me & Start a new thread */
			}
		}
		ThreadCounter[MyID]++;

		//printf("MyID:%d ThreadCounter[MyID]:%d LastLength[MyID]:%d SourOffset:%d DestLength:%d\n", MyID, ThreadCounter[MyID], LastLength[MyID],
					//My_Job->ti[MyID]->SourOffset, My_Job->ti[MyID]->DestLength);

		if( My_Job->ti[MyID]->Message == MESSAGE_EXIT ) /* The main loop called for a exit */
		{
			//printf("Someone send a MESSAGE_EXIT! ID:%d\n", MyID);
			savestatus(My_Job);
			My_Job->ti[MyID]->Message = 0;
			ThreadCounter[MyID] = -1;
			ReturnCode = 2;		/* Mark this thread has finished */
		}
	}
	else
    {
		int ToInit = 0;
		int ToGive = 0;
		int MaxID = 0;
		unsigned int MaxLength = 0;

		for(i=0 ; i<MaxThreads ; i++)	/* Count empty threads */
		{
			if( !My_Job->ti[i]->DestLength )
			{
				ToGive++;
				if( !My_Job->ti[i]->SourOffset ) ToInit++;  /* Both zero mean this job've not been initialized  */
			}

			if( My_Job->ti[i]->DestLength > MaxLength ) /* Find the thread that have most heavy duty */
			{
				MaxID = i;
				MaxLength = My_Job->ti[i]->DestLength;
			}
		}

		if(ToInit == MaxThreads)  /* All threads uninitialized. give the whole file to this thread */
		{
			//printf("%d %d\n", My_Job->SpecOffset, My_Job->SpecLength);
			My_Job->ti[MyID]->SourOffset = My_Job->SpecOffset;
			My_Job->ti[MyID]->DestLength = My_Job->SpecLength;
			ReturnCode = 255;	   /* Ask for execute on this thread again (to really start it) */
		}
		else /* Give current thread something to do */
		{
			unsigned int Length = My_Job->ti[MaxID]->DestLength / (ToGive+1); /* +1 is MaxID's owner itself */
			if(Length > MIN_INSERT_SIZE) /* No need to spawn thread if no much work */
			{
				MUTEX /* Will change other threads's working parameters */

				My_Job->ti[MyID]->DestLength 	= My_Job->ti[MaxID]->DestLength - Length;
				My_Job->ti[MyID]->SourOffset 	= My_Job->ti[MaxID]->SourOffset + Length;
				My_Job->ti[MaxID]->DestLength 	= Length;

				DEMUTEX

				ReturnCode = 255;     /* Ask for execute on this thread again (to really start it) */
			}
			else
			{
				//printf("I go here!\nLength:%d ToGive:%d ToInit:%d\n", Length, ToGive, ToInit);
				ThreadCounter[MyID] = -1;
			    ReturnCode = 2; /* Nothing to do, current thread finished */
			}
		}
		savestatus(My_Job);
	}
	return ReturnCode;
}

void fgetline(FILE * fpin, char * dst)
{
	char * oridst = dst;
	while( '{' != fgetc(fpin) ) ;  /* Skip anything before the leading { */
	while( 1 ) {
		*dst=fgetc(fpin);
		if( 0x0a == *dst ) {  /* EOL got */
                        while( '}' != *dst ) {
                                *dst = 0x00;
                                dst--;
                        }
                        *dst = 0x00;
			break;
		}
		dst++;
	}
	if (!strcmp(oridst, "null")) *oridst = 0x00;  /* "null" means null string */
}

int initstatus(struct JOB_PARM *My_Job)
{
	int i;
	char *DashPtr = strrchr(My_Job->URL, '/');
	if( DashPtr == NULL ) DashPtr = My_Job->URL;
	else DashPtr += 1;

	strcpy(My_Job->StaFileName, DashPtr);
	strcat(My_Job->StaFileName, ".resume");

	My_Job->StaFile = fopen(My_Job->StaFileName, "r+");
	if(My_Job->StaFile != NULL)	/* A sta file exists */
	{
		fgetline(My_Job->StaFile, My_Job->URL);
		fgetline(My_Job->StaFile, My_Job->Proxy);
		fgetline(My_Job->StaFile, My_Job->Username);
		fgetline(My_Job->StaFile, My_Job->Password);
		fscanf(My_Job->StaFile, "{%d %d %ld %ld}\n", &My_Job->MaxThreads, &My_Job->MaxTime, &My_Job->SpecOffset, &My_Job->SpecLength);
		if(My_Job->MaxThreads > MAX_THREADS) My_Job->MaxThreads=MAX_THREADS;
		if(My_Job->MaxThreads < 1) My_Job->MaxThreads=1;
		for(i=0 ; i<My_Job->MaxThreads ; i++)
		{
			if( My_Job->ti[i] != NULL ) free(My_Job->ti[i]);
			if( (My_Job->ti[i] = (struct THREAD_INFO *)malloc(sizeof(struct THREAD_INFO))) == NULL) return 1;
			memset(My_Job->ti[i], 0, sizeof(struct THREAD_INFO));
		}

		for(i=0 ; i<My_Job->MaxThreads ; i++)
			fscanf(My_Job->StaFile, "{%ld %ld}\n", &My_Job->ti[i]->SourOffset, &My_Job->ti[i]->DestLength);

		myfclose( & My_Job->StaFile);
	}
	else /* Create one */
	{
		if(My_Job->MaxThreads > MAX_THREADS) My_Job->MaxThreads=MAX_THREADS;
		if(My_Job->MaxThreads < 1) My_Job->MaxThreads=1;
		for(i=0 ; i<My_Job->MaxThreads ; i++)
		{
			if( My_Job->ti[i] != NULL ) free(My_Job->ti[i]);
			if( (My_Job->ti[i] = (struct THREAD_INFO *)malloc(sizeof(struct THREAD_INFO))) == NULL) return -1;
			memset(My_Job->ti[i], 0, sizeof(struct THREAD_INFO));
		}
		savestatus(My_Job);
	}
	return 0;
}


int savestatus(struct JOB_PARM *My_Job)
{
	int i;

	char Proxy[256], Username[128], Password[128];
	if(strlen(My_Job->Proxy)) strcpy(Proxy, My_Job->Proxy);
	else strcpy(Proxy, "null");
	if(strlen(My_Job->Username)) strcpy(Username, My_Job->Username);
	else strcpy(Username, "null");
	if(strlen(My_Job->Password)) strcpy(Password, My_Job->Password);
	else strcpy(Password, "null");

	if( (My_Job->StaFile = fopen(My_Job->StaFileName, "wb")) == NULL ) return 1;
	fprintf(My_Job->StaFile, "{%s}\n{%s}\n{%s}\n{%s}\n{%d %d %ld %ld}", My_Job->URL, Proxy, Username, Password,
		My_Job->MaxThreads, My_Job->MaxTime, My_Job->SpecOffset, My_Job->SpecLength);
	My_Job->StaDataPtr = ftell(My_Job->StaFile);

	for(i=0 ; i<My_Job->MaxThreads ; i++)
		fprintf(My_Job->StaFile, "\n{%ld %ld}", My_Job->ti[i]->SourOffset, My_Job->ti[i]->DestLength);

	myfclose( & My_Job->StaFile);
	return 0;
}
