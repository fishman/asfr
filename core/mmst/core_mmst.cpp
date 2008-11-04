/* The core routine for ASF download/extraction. */
#include "core_mmst.h"

#include "../../asfr.h"

#include "../asf_commonh.h"
#include "../asf_helper.h"
#include "../asf_redirection.h"
#include "../asf_net.h"

void core_mmst(struct JOB_PARM *My_Job)
{
	int						MyID = My_Job->ThreadID;
	struct THREAD_INFO 		*My_ti = My_Job->ti[MyID];
	struct HEADER_INFO 		*My_hi = My_Job->hi;
	struct CUSTOM_INFO_ASF 	*My_ci = (struct CUSTOM_INFO_ASF*)My_Job->ci;

	unsigned int i;
	unsigned char Buffer[MAX_CHUNK_SIZE];

	///////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	switch(My_ti->RequestType)
	{
	case REQUEST_INIT:
		My_Job->ciSize = sizeof(struct CUSTOM_INFO_ASF);
		break;

	case REQUEST_RESET:
		memset(My_ci, 0, sizeof(struct CUSTOM_INFO_ASF));
		My_ci->ASFContentType = unknown_content;
		My_ci->PortNum = MMST_DEFAULT_PORT;
		randomize_guid(My_ci->RandomizedGUID);
		break;

	case REQUEST_INFORMATION:
		{
			My_ti->Reply = REPLY_FAIL;

			char *URLPtr;
			char *DashPtr;
			char *ColonPtr;

			char EscapedURL[512];

			/* Firset treat this as a on-disk redirection file */
			parse_redirection(My_Job, NULL, 0);

			gui_showstatus(STATUS_INFORMATION, "Parsing URL: '%s'\n", My_Job->URL);

			strcpy(EscapedURL, My_Job->URL); /* No need to escape, MMST use UNICODE path */

			URLPtr = EscapedURL;
			if (!strnicmp("mms://",  URLPtr, 6)) URLPtr += 6;
			if (!strnicmp("mmst://", URLPtr, 7)) URLPtr += 7;
			DashPtr = strchr(URLPtr, '/');
			if (DashPtr == NULL)
			{
				DashPtr = URLPtr+strlen(URLPtr);
				gui_showstatus(STATUS_CRITICALERROR, "Invalid URL format!\n");
				break;
			}
			else
			    {
				strncpy(My_ci->ServerName, URLPtr, DashPtr-URLPtr);
				My_ci->ServerName[DashPtr-URLPtr] = 0;
				strcpy(My_ci->File, DashPtr+1);

				ColonPtr = strchr(My_ci->ServerName, ':');
				if (ColonPtr != NULL)
				{
					My_ci->PortNum = (unsigned short)atol(ColonPtr+1);
					*ColonPtr = '\0';
					if (My_ci->PortNum == 0)
					{
						gui_showstatus(STATUS_CRITICALERROR, "Invalid URL format!\n");
						strcpy(My_ci->ServerName, "");
						strcpy(My_ci->File, "");
						break;
					}
				}
			} /* url parse finished */
			if(Debug) gui_showstatus(STATUS_INFORMATION, "ServerName: '%s'\nPortNum: %d\nFile: '%s'\n",
				My_ci->ServerName, My_ci->PortNum, My_ci->File);

			/* Attempt to detect if we should call gethostbyname() or gethostbyaddr() */
			if(Debug) gui_showstatus(STATUS_INFORMATION, "Resolving host: '%s'\n", My_ci->ServerName);

			unsigned int Addr = 0;
			struct hostent *hp = NULL;

			if (isalpha(My_ci->ServerName[0]))
			{
				/* Server address is a host name */
				hp = gethostbyname(My_ci->ServerName);
				if (hp == NULL)
				{
					gui_showstatus(STATUS_CRITICALERROR, "Unable to resolve host '%s'!\n", My_ci->ServerName);
					break;
				}
			}
			else
			    {
				/* Server is a ip address. convert nnn.nnn address to a usable one */
				Addr = inet_addr(My_ci->ServerName);
				if ((hp = gethostbyaddr((char *)&Addr,4,AF_INET))!=NULL)
					strcpy(My_ci->ServerName, hp->h_name);
			}	/* Gethost finished */

			/* Copy the resolved information into the sockaddr_in structure */
			if (hp != NULL)
			{
				memcpy(&(My_ci->Server.sin_addr),hp->h_addr,hp->h_length);
				My_ci->Server.sin_family = hp->h_addrtype;
				strcpy(My_ci->ServerName, hp->h_name);
			}
			else
			    {
				memcpy(&(My_ci->Server.sin_addr), &Addr, 4);
				My_ci->Server.sin_family = AF_INET;
			}
			My_ci->Server.sin_port = htons(My_ci->PortNum);
		} /* Fall through */

	case REQUEST_GETDATA:
		{
			My_ti->Message = 0;
			My_ti->Reply = REPLY_FAIL;

			/* Open a socket */
			SOCKET ConnSocket;
			if ((ConnSocket=my_socket(AF_INET, SOCK_STREAM, 0)) <= 0)
			{
				gui_showstatus(STATUS_CRITICALERROR, "Socket() failed: %d\n", errno);
				break;
			}

			gui_showstatus(STATUS_INFORMATION, "Connecting to: %s:%d\n", My_ci->ServerName, My_ci->PortNum);

			if( my_connect(ConnSocket, (struct sockaddr*)&(My_ci->Server), sizeof(My_ci->Server)) == SOCKET_ERROR )
			{
				gui_showstatus(STATUS_CRITICALERROR, "connect() failed: %d\n", errno);
				break;
			}

			/* Initialize a stream */
			struct STREAM_PARM Stream_Parm;
			Stream_Parm.RequestType = REQUEST_INIT;
			Stream_Parm.conn_socket = ConnSocket;
			if( (Stream_Parm.si = (char*)malloc( readfromstream(&Stream_Parm, 0, 0))) == NULL) break;

			Stream_Parm.RequestType = REQUEST_RESET;
			readfromstream(&Stream_Parm, 0, 0);

			Stream_Parm.RequestType = REQUEST_GETDATA;

			unsigned char *BufPtr;
			unsigned int MMSCSize;
			unsigned int MMSCSeq = 0;

			BufPtr = Buffer; /* Send Initial Request */
			BufPtr += my_sprintf(BufPtr, MMSC_HEADER, sizeof(MMSC_HEADER));
			BufPtr += my_sprintf(BufPtr, MMSC_REQUEST_INIT, sizeof(MMSC_REQUEST_INIT));
			BufPtr += my_swprintf(BufPtr, "NSPlayer/4.0.0.3845; ", 0);
			BufPtr += my_swprintf(BufPtr, (const char*)My_ci->RandomizedGUID, 0);
			BufPtr += my_swprintf(BufPtr, "\0", 1);
			MMSCSize=(int)BufPtr-(int)Buffer; if(MMSCSize%8) MMSCSize+=8-MMSCSize%8; put_long(Buffer+MMSC_POS_SIZE, MMSCSize-0x10);
			put_long(Buffer+MMSC_POS_SEQ, MMSCSeq++);
			if ((my_send(ConnSocket, Buffer, MMSCSize, 0)) == SOCKET_ERROR)
			{
				gui_showstatus(STATUS_CRITICALERROR, "send() failed: %d\n", errno);
				my_closesocket(ConnSocket);
				free(Stream_Parm.si);
				break;
			}
                        
			/* The main loop for chunk extraction/ASF generation */
			unsigned long MyFilePointer = 0;
			unsigned char TimeCodeString[64];
			//unused unsigned int StartTimeHi = 0xffffffff;
			unsigned int StartTime    = 0xffffffff;
			unsigned int StartSeqNO   = 0xffffffff;

			for (;;)
			{
				unsigned char Type, SubType;
				unsigned short Length;
				unsigned int BodyLength;
				unsigned long SeqNO;
				unsigned long TimeCode;
				unsigned int Progress; /* scaled from 0 to 10000 */
				unsigned int Got;

				if (eos(&Stream_Parm))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Connection reset\n");
					break;
				}

				if( readfromstream(&Stream_Parm, Buffer, 0x08) != 0x08) break;

				Buffer[3] = 0x00; /* Mask out server version(maybe?) */

				if( !memcmp(MMSC_HEADER, Buffer, 0x08) ) 	/* Process MMSC if found */
				{
					unsigned char ToBreak = 0;
					unsigned long DataSize;
					if( readfromstream(&Stream_Parm, Buffer+0x08, 0x08) != 0x08) break;
					get_long(Buffer+MMSC_POS_SIZE, &DataSize);
					if( (unsigned)readfromstream(&Stream_Parm, Buffer+0x10, DataSize) != DataSize) break;
					unsigned char MMSCFlag = Buffer[MMSC_POS_FLAG];
					//unused unsigned char MMSCSubFlag = Buffer[MMSC_POS_SUBFLAG];

					BufPtr = Buffer; /* Prepare Send Request */
					BufPtr += my_sprintf(BufPtr, MMSC_HEADER, sizeof(MMSC_HEADER));
					switch(MMSCFlag)
					{
						case 0x01: /* Server Info Received */
						{ /* Send local address & port */
							SOCKADDR_IN Client;
#ifdef __MINGW32__
							int ClientLen = sizeof(Client);
#else
							size_t ClientLen = sizeof(Client);
#endif /* __MINGW32__ */
							unsigned char PortNumString[8];
							getsockname(ConnSocket, (struct sockaddr*)&Client, &ClientLen);
                                                        sprintf((char *)PortNumString, "%d", ntohs(Client.sin_port));
							BufPtr += my_sprintf(BufPtr, MMSC_REQUEST_CONNPORT, sizeof(MMSC_REQUEST_CONNPORT));
							BufPtr += my_swprintf(BufPtr, "\\\\", 0);
							BufPtr += my_swprintf(BufPtr, inet_ntoa(Client.sin_addr), 0);
							BufPtr += my_swprintf(BufPtr, "\\TCP\\", 0);
							BufPtr += my_swprintf(BufPtr, (char*)PortNumString, 0);
							BufPtr += my_swprintf(BufPtr, "\0", 1);
							My_ti->Reply = REPLY_RETRY; /* Server is OK, so Retry if connect reset */
						}break;

						case 0x02: /* 'Funnel Of The' Received, whats meaning? */
						{ /* Send file request */
							BufPtr += my_sprintf(BufPtr, MMSC_REQUEST_FILE, sizeof(MMSC_REQUEST_FILE));
							BufPtr += my_swprintf(BufPtr, My_ci->File, 0);
							BufPtr += my_swprintf(BufPtr, "\0", 1);
						}break;

						case 0x05: /* ASF Body Received */
							break;  /* Nothing to do :) */

						case 0x06: /* File Info Received */
						{ /* Send request for ASF Header */
							get_long(Buffer+MMSC_POS_CHUNKLENGTH, &My_ci->ChunkLength2);
							if(My_ci->ChunkLength2) /* detect file existance here */
							{
								BufPtr += my_sprintf(BufPtr, MMSC_REQUEST_RECVHEADER, sizeof(MMSC_REQUEST_RECVHEADER));
								My_hi->Resumeable = TRUE;
							}
							else
							{
								gui_showstatus(STATUS_CRITICALERROR, "File '%s/%s' not found!\n", My_ci->ServerName, My_ci->File);
								My_ti->Reply = REPLY_FAIL;
								ToBreak = 1;
							}
						}break;

						case 0x11: /* ASF Header Received */
						{ /* Send request for ASF Body if needed */
							usleep(1000);
							if (My_ti->RequestType == REQUEST_GETDATA)
							{
								BufPtr += my_sprintf(BufPtr, MMSC_REQUEST_RECVBODY, sizeof(MMSC_REQUEST_RECVBODY));
								unsigned long ReqSeqNO = max((signed)(My_ti->SourOffset - My_ci->EndOfHeaderOffs - DATSEG_HDR_SIZE), 0) / My_ci->ChunkLength;
								put_long(Buffer+MMSC_POS_REQCHUNKSEQ, ReqSeqNO);
							}
						}break;

						case 0x1A: /* File is password protected */
						{
							gui_showstatus(STATUS_CRITICALERROR, "File is password protected!\n");
							My_ti->Reply = REPLY_FAIL;
							ToBreak = 1;
						} break;

						case 0x1B: /* Received every minute */
						{ /* Send a reply */
							BufPtr += my_sprintf(BufPtr, MMSC_REPLY, sizeof(MMSC_REPLY));
						} break;

						case 0x1E: /* Transfer finished */
						{
							gui_showstatus(STATUS_FINISH, "Transfer complete.\n");
							My_ti->Reply = REPLY_FINISH;
							ToBreak = 1;
						} break;

						default:  /* What this? */
						{
							gui_showstatus(STATUS_INFORMATION, "Unknown MMSCFlag:%X\n", MMSCFlag);
							if(Debug) {  /* Dump all contents */
								for ( i=0 ; i<DataSize+0x10 ; ++i ) {
									if( 0 == (i % 0x10) ) printf("\n");
									printf("%02X ", Buffer[i]);
								}
								printf("\n\n");
							}
							return;
						} break;
					}

					if( (MMSCSize=(int)BufPtr-(int)Buffer) > sizeof(MMSC_HEADER) ) /* have something to send */
					{
				    	if(MMSCSize % 8) MMSCSize += 8-MMSCSize%8; put_long(Buffer+MMSC_POS_SIZE, MMSCSize-0x10);
						put_long(Buffer+MMSC_POS_SEQ, MMSCSeq++);

						if ((my_send(ConnSocket, Buffer, MMSCSize, 0)) == SOCKET_ERROR)
						{
							gui_showstatus(STATUS_CRITICALERROR, "send() failed: %d\n", errno);
							my_closesocket(ConnSocket);
							free(Stream_Parm.si);
							break;
						}
					}

					if(ToBreak) break;
					continue;
				}
				else
				{
					get_long(Buffer, &SeqNO);
					Type = Buffer[MMSD_POS_TYPE];
					SubType = Buffer[MMSD_POS_SUBTYPE];
					get_short(Buffer+MMSD_POS_SIZE, &Length);
				}

				if (eos(&Stream_Parm))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Connection reset\n");
					break;
				}

				/* These header types correspond to "H$", "D$" and "E$" (Header, Data and End) */
				if ((Type != MMSD_HEADER_CHUNK) && (Type != MMSD_DATA_CHUNK))
				{
					gui_showstatus(STATUS_ERROR, "Unknown header type: %02X:%02X\n", Type, SubType);
					My_ti->Reply = REPLY_FAIL;
					//break;
				}

				/* calculate length of chunk body. */
				BodyLength = Length-8;

				/* check if the body length exceeds our buffer size */
				if ((BodyLength > MAX_CHUNK_SIZE) || (BodyLength <= 0))
				{;
					gui_showstatus(STATUS_CRITICALERROR, "Illegal ChunkLength. Chunk is %d bytes!\n", Length);
					break;
				}

				/* Read chunk's body data */
				Got = readfromstream(&Stream_Parm, Buffer, BodyLength);

				if (Type == MMSD_HEADER_CHUNK && My_ti->RequestType == REQUEST_INFORMATION)
				{
					int Offs;

					/* finally got the header */
					gui_showstatus(STATUS_INFORMATION, "Received ASF header!\n");

					/* find a specific object in this header */
					Offs = find_id(ASF_HDR_ID,Buffer, Got);
					if (Offs == -1)
					{
						gui_showstatus(STATUS_CRITICALERROR, "Unable to parse this ASF header!\n");
						break;
					}

					/* extract required information */
					My_ci->HeaderOffs = Offs;
					//get_quad(&Buffer[Offs+HDR_TOTAL_SIZE_8], &My_ci->TotalSizeHi, &My_ci->TotalSizeLo);
					get_quad(&Buffer[Offs+HDR_FINE_TOTALTIME_8], &My_ci->TotalTimeHi, &My_ci->TotalTimeLo);
					get_long(&Buffer[Offs+HDR_PLAYTIME_OFFSET_4], &My_ci->Offset);
					get_long(&Buffer[Offs+HDR_FLAGS_4], &My_ci->Flags);
					get_long(&Buffer[Offs+HDR_ASF_CHUNKLENGTH_4], &My_ci->ChunkLength);
					get_long(&Buffer[Offs+HDR_ASF_CHUNKLENGTH_CONFIRM_4], &My_ci->ChunkLength2);

					unsigned long NumOfPacket;
					get_long(&Buffer[Offs+HDR_NUM_PACKETS_8], &NumOfPacket);
					My_ci->TotalSizeLo = BodyLength + NumOfPacket*My_ci->ChunkLength;
					My_ci->TotalSizeHi = 0;

					My_hi->FileSize = My_ci->TotalSizeLo;

					/* check if the extracted chunk length looks good */
					if ((My_ci->ChunkLength > MAX_CHUNK_SIZE) && (My_ci->ChunkLength != My_ci->ChunkLength2))
					{
						gui_showstatus(STATUS_CRITICALERROR, "Illegal chunk sizes, %d, %d\n", My_ci->ChunkLength, My_ci->ChunkLength2);
						break;
					}

					/* calculate playtime in milliseconds (0 for live streams) */
					if (My_ci->TotalTimeHi == 0 && My_ci->TotalTimeLo == 0)
						My_ci->Time = 0; /* live streams */
					else
					    My_ci->Time = (int)((double)429496.7296 * My_ci->TotalTimeHi) + (My_ci->TotalTimeLo / 10000) - My_ci->Offset;

					/* store position where the ASF header segment ends and the chunk data segment starts */
					My_ci->EndOfHeaderOffs = BodyLength - DATSEG_HDR_SIZE;
					My_ti->Reply = REPLY_OK;
					break;
				}

				/* Try to extract a timecode from all known chunk/content types(only applies to data chunks) */
				if (Type == MMSD_DATA_CHUNK)
				{
					int TCStart;
					ustrcpy(TimeCodeString, "?????");
					TimeCode = 0;

					if (My_ti->RequestType == REQUEST_INFORMATION) continue;

					/* save the first seqno available as a reference */
					if (StartSeqNO == 0xffffffff) StartSeqNO = SeqNO;

					/* fix the seqno for live recordings only */
					if (My_ci->Time == 0) SeqNO -= StartSeqNO; /* refer seqno to the point we "zapped in" (for live streams) */

					/* find the location of the time code */
					if ((TCStart = whereis_timecode(Buffer)) > 0)
					{
						/* The timecode is an integer value defining milliseconds enough range for about 50 days! */
						get_long(&Buffer[TCStart], &TimeCode);

						/* save the first timecode available as a reference */
						if (StartTime == 0xffffffff) StartTime = TimeCode;

						/* fix timecode for live recordings only */
						if (My_ci->Time == 0)
						{
							TimeCode -= StartTime; /* refer timecode to the point we "zapped in" (live streams) */
							fix_timecodes(Buffer, BodyLength, StartTime, SeqNO, My_ci);	/* fixes the timecodes in the memory buffer */
						}

						/* save max. timecode value */
						if (TimeCode > My_ci->MaxTimeCode)
							My_ci->MaxTimeCode = TimeCode;

						/* create a string with a human-readable form of the timecode */
						ustrcpy(TimeCodeString, createtimestring(TimeCode));
					}
				}

				/* calculate progress indicator (scale: 0....10000) */
				if (My_ci->Time == 0)		/* this mean a live record */
				{
					if (My_Job->MaxTime == 0)   /* unlimited recording */
						Progress = 0;
					else                /* limited time recording */
					Progress = (int)((double)TimeCode*10000/(My_Job->MaxTime*60*1000));
				}
				else
					Progress = (int)(((double)(SeqNO+1)*My_ci->ChunkLength+My_ci->EndOfHeaderOffs+DATSEG_HDR_SIZE) / My_hi->FileSize * 10000);

				/* Print current position in stream download */
				gui_showstatus(STATUS_INFORMATION, "%d | %7dKB | %3d.%02d%% | HDR:%02X:%02X | %5dBYTES | SEQ:%06X | TC:%s\n",
					MyID,
					max( (signed long)(SeqNO+1)*My_ci->ChunkLength - My_ti->DestOffset, 0 )/1024 + 1,
					Progress / 100,
					Progress % 100,
					Type, SubType,
					BodyLength,
					SeqNO,
					TimeCodeString );

				/* some statistics for data chunks only */
				if (Type == MMSD_DATA_CHUNK)
				{
					/* check chunk body for completeness */
					if (Got < BodyLength && My_Job->OutFile != NULL)
					{
						gui_showstatus(STATUS_ERROR, "Received incomplete chunk... (Chunk is NOT saved)\n");
						continue;
					}
					My_ci->NumDataChunks++; /* count number of chunks */
					My_ci->SizeOfDataChunks += My_ci->ChunkLength; /* count total size of chunks */
				}

				if (My_Job->OutFile != NULL)
				{
					/* lock other threads from writing */
					MUTEX

					char ThisChunkFlag;
					unsigned char *ThisChunkPtr;
					unsigned long ThisChunkPos;
					unsigned long ThisChunkSize;

					switch (Type)
					{
					case MMSD_HEADER_CHUNK:
						ThisChunkPos = 0;
						ThisChunkPtr = Buffer;
						if(My_ti->SourOffset == 0)  /* Is this the request for first segment? */
						{
							unsigned char c1=0, c2=0, c3=0, c4=0;

							/* a dirty hack to find all "MP43" and change to "DIV3" in header */
							for ( i=0 ; i<=Got ; ++i ) {
								c4=c3 ; c3=c2 ; c2=c1;
								c1=Buffer[i];
								if( 'M'==c4 && 'P'==c3 && '4'==c2 && '3'==c1 ) {
									Buffer[i-3]='D';
									Buffer[i-2]='I';
									Buffer[i-1]='V';
									Buffer[i]='3';
								}
							}

							ThisChunkSize = Got;
						}
						else
							ThisChunkSize = 0;    /* Just ignore it */
						break;

					case MMSD_DATA_CHUNK:
						/* calculate appropriate position in file */
						ThisChunkPos = My_ci->EndOfHeaderOffs + DATSEG_HDR_SIZE + SeqNO * My_ci->ChunkLength;
						ThisChunkSize = My_ci->ChunkLength;
						ThisChunkPtr = Buffer;

						/* Get Data chunk's Padding Flags. */
						ThisChunkFlag = Buffer[3];

						/* Convert 0x40 chunk to Non-0x40 form, VirtualDub dont recognize this type */
						if (ThisChunkFlag & 0x40)
						{
							for (i=5 ; i<Got ; i++) Buffer[i]=Buffer[i+2];
							Buffer[3] = ThisChunkFlag - 0x40;
							Got -= 2;
							BodyLength -= 2;
						}

						/* Fix the padding size. */
						if (ThisChunkFlag & 0x18)
						{
							short int PaddingSize = ThisChunkSize-BodyLength;
							Buffer[5] = (char)(PaddingSize & 0xff);
							if (ThisChunkFlag & 0x10) Buffer[6]=(char)(PaddingSize >> 8);
						}

						/* Fill up unused bytes in this chunk. ASF requires equally sized DATA chunks */
						memset((char *)(ThisChunkPtr + Got), 0, (ThisChunkSize - Got));
						break;

					default:
						ThisChunkPos = MyFilePointer;
						ThisChunkSize = Got;
						ThisChunkPtr = Buffer;
					}

					/* Since the server always send a full chunk, We sometimes just need latter part of this chunk */
					if (ThisChunkPos < My_ti->SourOffset)
					{
						if ((ThisChunkPos + ThisChunkSize) < My_ti->SourOffset)
						{
							if(Type != MMSD_HEADER_CHUNK) gui_showstatus(STATUS_INFORMATION, "(%d) Chunk position below range, ignored.\n", MyID);
							ThisChunkPos = 0;
							ThisChunkSize = 0;
						}
						else
						{
							ThisChunkPtr += (My_ti->SourOffset - ThisChunkPos);
							ThisChunkSize -= (My_ti->SourOffset - ThisChunkPos);
							ThisChunkPos = My_ti->SourOffset;
						}
					}

					/* We sometimes just need header part of this chunk */
					if ((ThisChunkPos + ThisChunkSize) > (My_ti->SourOffset + My_ti->DestLength))
					{
						if (ThisChunkPos > (My_ti->SourOffset + My_ti->DestLength))
						{
							gui_showstatus(STATUS_INFORMATION, "(%d) Chunk position over range, ignored.\n", MyID);
							ThisChunkPos = 0;
							ThisChunkSize = 0;
						}
						else
						{
					//printf("ID:%d Offse:%d Length:%d ThisChunkSize:%d ThisChunkPos:%d\n", MyID, My_ti->SourOffset, My_ti->DestLength, ThisChunkSize, ThisChunkPos);
							ThisChunkSize -= ((ThisChunkPos + ThisChunkSize) - (My_ti->SourOffset + My_ti->DestLength));
						}
					}

					/* perform write action */
					fseek(My_Job->OutFile, ThisChunkPos-My_ti->DestOffset, SEEK_SET);
					fwrite(ThisChunkPtr, ThisChunkSize, 1, My_Job->OutFile);

					MyFilePointer = ThisChunkPos + ThisChunkSize;
				    My_ti->SourOffset += ThisChunkSize;
					My_ti->DestLength -= ThisChunkSize;
					//printf("ID:%d Offse:%d Length:%d ThisChunkSize:%d ThisChunkPos:%d\n", MyID, My_ti->SourOffset, My_ti->DestLength, ThisChunkSize, ThisChunkPos);

					/* unlock other threads */
					DEMUTEX
				}

				/* set a new total time for the stream (important for preview and slider functionality)
				if (My_ci->Time == 0)
				{
				if (My_Job->MaxTime == 0)
				gui_modify_duration(TimeCode);
				else
				gui_modify_duration(My_Job->MaxTime*60*1000);
				} */
				/* Check whether dest_length reached */
				if( !My_ti->DestLength )
				{
					gui_showstatus(STATUS_INFORMATION, "Segment finished.\n");
					My_ti->Reply = REPLY_SEGMENTFINISH;
					break;
				}

				/* use recording time limit, if specified */
				if ( (My_Job->MaxTime != 0) && ((int)TimeCode >= (My_Job->MaxTime*60*1000)) )
				{
					My_ti->DestLength = 0;
					gui_showstatus(STATUS_INFORMATION, "maxtime reached.\n");
					My_ti->Reply = REPLY_EXIT;
					break;
				}

				/* Receive Message from Job */
				if(My_ti->Message == MESSAGE_PAUSE)
					while(My_ti->Message != MESSAGE_CONTINUE) usleep(50);

				if(My_ti->Message == MESSAGE_EXIT || My_ti->Message == MESSAGE_CANCEL)
				{
					My_ti->Reply = REPLY_EXIT;
				    break;
				}

			} /* for (;;) */

			/* Cleanup stream */
			my_closesocket(ConnSocket);
			free(Stream_Parm.si);
			break;
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	case REQUEST_FINISH:
		{
			/* fix total file size and for live streams the header information as well */
			if ( My_Job->OutFile != NULL )
			{
				unsigned long FileSizeHi = 0;
				unsigned long FileSizeLo;

				/* Determine file size of .ASF file */
				fseek(My_Job->OutFile, 0, SEEK_END);
				FileSizeLo = ftell(My_Job->OutFile);

				/* write correct file size in ASF header */
				fseek(My_Job->OutFile, My_ci->HeaderOffs + HDR_TOTAL_SIZE_8, SEEK_SET);
				write_quad(My_Job->OutFile, FileSizeHi, FileSizeLo);

				/* enable seeking in file */
				fseek(My_Job->OutFile, My_ci->HeaderOffs + HDR_FLAGS_4, SEEK_SET);
				write_long(My_Job->OutFile, 0x00000002);
			}
			break;
		}
		//////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
	} /* switch(RequestType)*/

	/* Cleanup & exit */
	return;
}
