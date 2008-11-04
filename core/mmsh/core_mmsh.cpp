/*
  ModuleL: core_mmsh.cxx
  ASF donwload and extraction
*/

/* The core routine for ASF download/extraction. */
#include "core_mmsh.h"

#include "../../asfr.h"

#include "../asf_commonh.h"
#include "../asf_helper.h"
#include "../asf_redirection.h"
#include "../asf_net.h"

void core_mmsh(struct JOB_PARM *My_Job)
{
	int				MyID = My_Job->ThreadID;
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
		My_ci->PortNum = MMSH_DEFAULT_PORT;
		randomize_guid(My_ci->RandomizedGUID);
		break;

	case REQUEST_INFORMATION:
		{

			char *URLPtr;
			char *DashPtr;
			char *ColonPtr;
			char EscapedURL[512];

			My_ti->Reply = REPLY_FAIL;

			/* Firset treat this as a on-disk redirection file */
			parse_redirection(My_Job, NULL, 0);

			gui_showstatus(STATUS_INFORMATION, "Parsing URL: '%s'\n", My_Job->URL);

			/* replace unescaped characters (e.g. spaces) by escape sequences */
			escape_url_string(EscapedURL, My_Job->URL);

			URLPtr = EscapedURL;
			if (!strnicmp("http://", URLPtr, 7)) URLPtr += 7;
			if (!strnicmp("mms://",  URLPtr, 6)) URLPtr += 6;
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
				strcpy(My_ci->File, DashPtr);

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

			/* parse proxy URL */
			if ( strcmp( My_Job->Proxy, "") )
			{
				char FullPath[512];
				if (My_ci->PortNum == MMSH_DEFAULT_PORT)
					sprintf(FullPath, "http://%s%s", My_ci->ServerName, My_ci->File);
				else
				    sprintf(FullPath, "http://%s:%d%s", My_ci->ServerName, My_ci->PortNum, My_ci->File);
				strcpy(My_ci->File, FullPath);

				escape_url_string(EscapedURL, My_Job->Proxy);
				URLPtr = EscapedURL;
				if (!strnicmp("http://", URLPtr, 7)) URLPtr+=7;

				DashPtr = strchr(URLPtr, '/'); /* There should be no '/' in proxy string */
				if (DashPtr != NULL)
					gui_showstatus(STATUS_CRITICALERROR, "Invalid proxy URL format!\n");
				else
				    {
					ColonPtr = strchr(URLPtr, ':');
					if (ColonPtr != NULL)
					{
						unsigned short ProxyPortNum;
						ProxyPortNum = (unsigned short)atol(ColonPtr+1);
						if (ProxyPortNum == 0)
						{
							gui_showstatus(STATUS_CRITICALERROR, "Invalid proxy URL format!\n");
							strcpy(My_ci->ServerName, "");
							strcpy(My_ci->File, "");
						}
						else
						    My_ci->PortNum = ProxyPortNum;
					}
					else
					    ColonPtr = URLPtr+strlen(URLPtr);

					strncpy(My_ci->ServerName, URLPtr, ColonPtr-URLPtr);
					My_ci->ServerName[ColonPtr-URLPtr] = 0;
				}
			} /* proxy parse finished */

			if ( strcmp(My_Job->Username, "") || strcmp(My_Job->Password, "") )
			{
				char PasswordCombo[128];
				sprintf(PasswordCombo, "%s:%s", My_Job->Username, My_Job->Password);
				base64enc(PasswordCombo,My_ci->Base64Buf);
			}

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
				if(My_ci->PortNum == MMSH_DEFAULT_PORT) /* maybe this is a dedicated mmst server, mmsh port(80) is not opened */
					My_ti->Reply = REPLY_MMST;	/* Try mmst next time */
				gui_showstatus(STATUS_CRITICALERROR, "connect() failed: %d\n", errno);
				break;
			}

			/* Prepare a stream to read incoming datas */
			struct STREAM_PARM Stream_Parm;
			Stream_Parm.RequestType = REQUEST_INIT;
			Stream_Parm.conn_socket = ConnSocket;
			if( (Stream_Parm.si = (char*)malloc( readfromstream(&Stream_Parm, 0, 0))) == NULL) break;

			Stream_Parm.RequestType = REQUEST_RESET;
			readfromstream(&Stream_Parm, 0, 0);

			Stream_Parm.RequestType = REQUEST_GETDATA;

			/* Generate a HTTP request */
			unsigned long OffsetHi = 0;
			unsigned long OffsetLo = My_ti->SourOffset;
			char *BufPtr = (char*)Buffer;
			if(OffsetLo == 0) OffsetHi=0xffffffff,OffsetLo=0xffffffff;

			if (My_ti->RequestType == REQUEST_GETDATA)
			{	/* Generate information for server */
				BufPtr+=sprintf(BufPtr,"GET %s HTTP/1.0\r\n", My_ci->File);
				if ( ustrcmp(My_ci->Base64Buf, "") ) BufPtr += sprintf(BufPtr,"Authorization: Basic %s\r\n", My_ci->Base64Buf);
				BufPtr+=sprintf(BufPtr,"Accept: */*\r\n");
				BufPtr+=sprintf(BufPtr,"User-Agent: NSPlayer/4.1.0.3856\r\n");
				if ( strcmp(My_Job->Proxy, "") ) BufPtr+=sprintf(BufPtr,"Host: %s\r\n", My_ci->ServerName);
				if (My_ci->ASFContentType == prerecorded_content)
					BufPtr+=sprintf(BufPtr,"Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=%u:%u,request-context=2,max-duration=0\r\n",(unsigned)OffsetHi,(unsigned)OffsetLo);
				if (My_ci->ASFContentType == live_content)
					BufPtr+=sprintf(BufPtr,"Pragma: no-cache,rate=1.000000,request-context=2\r\n");
				BufPtr+=sprintf(BufPtr,"Pragma: xPlayStrm=1\r\n");
				BufPtr+=sprintf(BufPtr,"Pragma: xClientGUID=%s\r\n", My_ci->RandomizedGUID);
				BufPtr+=sprintf(BufPtr,"Pragma: stream-switch-count=1\r\n");
				BufPtr+=sprintf(BufPtr,"Pragma: stream-switch-entry=ffff:1:0\r\n");
				BufPtr+=sprintf(BufPtr,"Connection: Close\r\n\r\n");
			}
			if (My_ti->RequestType == REQUEST_INFORMATION)
			{
				BufPtr+=sprintf(BufPtr,"GET %s HTTP/1.0\r\n", My_ci->File);
				if ( ustrcmp(My_ci->Base64Buf, "") ) BufPtr += sprintf(BufPtr,"Authorization: Basic %s\r\n", My_ci->Base64Buf);
				BufPtr+=sprintf(BufPtr,"Accept: */*\r\n");
				BufPtr+=sprintf(BufPtr,"User-Agent: NSPlayer/4.1.0.3856\r\n");
				if ( strcmp(My_Job->Proxy, "") ) BufPtr+=sprintf(BufPtr,"Host: %s\r\n", My_ci->ServerName);
				BufPtr+=sprintf(BufPtr,"Pragma: no-cache,rate=1.000000,stream-time=0,stream-offset=0:0,request-context=1,max-duration=0\r\n");
				BufPtr+=sprintf(BufPtr,"Pragma: xClientGUID=%s\r\n", My_ci->RandomizedGUID);
				BufPtr+=sprintf(BufPtr,"Connection: Close\r\n\r\n");
			}

			if (DUMP_HEADERS) 			/* Dump the outgoing HTTP request */
				gui_showstatus(STATUS_INFORMATION, "\nHTTP-Request to send:\n%s", Buffer);

			gui_showstatus(STATUS_INFORMATION, "sending request [%d bytes]\n",(int)BufPtr-(int)Buffer);

			if ((my_send(ConnSocket,Buffer,((int)BufPtr-(int)Buffer),0)) == SOCKET_ERROR)
			{
				gui_showstatus(STATUS_CRITICALERROR, "send() failed: %d\n", errno);
				my_closesocket(ConnSocket);
				free(Stream_Parm.si);
				break;
			}

			gui_showstatus(STATUS_INFORMATION, "Waiting for reply...\n");

			char HTTPLine[512];
			char HTTPMessage[128] = "<none>";
			char Features[256] = "";
			char ContentType[256] = "";
			char ServerType[256] = "";

			char *HDRPtr;
			unsigned int ErrorCode = 0;
			unsigned int EOL = 0;
			unsigned int HDRPos = 0;
			unsigned int LinePos = 0;
			unsigned int LineNum = 0;
			unsigned char ToBreak = 0;

			for (;;)
			{
				unsigned char c;

				if (readfromstream(&Stream_Parm, &c, 1) == 1)
				{
					/* Server is OK, so retry if connect reset */
					My_ti->Reply = REPLY_RETRY;

					if ((c != '\r') && (c != '\n'))
					{
						EOL = 0;
						HTTPLine[LinePos++] = c;
					}
					else
						HTTPLine[LinePos++] = 0;

					if (c == '\n')	/* finish of a line */
					{
						if (EOL == 0) /* if this is first '/n'? 2 '/n' will make this FALSE ,which mean end of HTTP header */
						{
							LinePos = 0;
							EOL = 1;
							LineNum++;
							HDRPtr = HTTPLine;

							if (LineNum == 1) /* Parse first line of HTTP reply */
							{
								if ((!strnicmp(HDRPtr, "HTTP/1.0 ", 9)) || (!strnicmp(HDRPtr, "HTTP/1.1 ", 9)))
								{
									HDRPtr += 9;
									sscanf(HDRPtr, "%d", &ErrorCode);
									HDRPtr += 4;
									strcpy(HTTPMessage, HDRPtr);
								}
								else
								    gui_showstatus(STATUS_CRITICALERROR, "Illegal server reply! Expected HTTP/1.0 or HTTP/1.1\n");
							}
							else
							    {
								/* Parse all other lines of HTTP reply , find "Content-Type:" */
								if (!strnicmp(HDRPtr, "Content-Type: ", 14))
								{
									HDRPtr += 14;
									strncpy(ContentType, HDRPtr, sizeof(ContentType));
								}
								if (!strnicmp(HDRPtr, "Pragma: ", 8))
								{
									HDRPtr += 8;
									if (!strnicmp(HDRPtr, "features=", 9))
									{
										HDRPtr += 9;
										strncpy(Features, HDRPtr, sizeof(Features));
									}
								}
								if (!strnicmp(HDRPtr, "Server: ", 8))
								{
									HDRPtr += 8;
									strncpy(ServerType, HDRPtr, sizeof(ServerType));
								}
							}
						}
						else
						    {
							Buffer[HDRPos++] = 0;
							break;
						}
					}
					Buffer[HDRPos++] = c;
				}
				else
				{
					if( !HDRPos ) /* Nothing Received, this is probably a mmst stream */
						My_ti->Reply = REPLY_MMST;
				    gui_showstatus(STATUS_CRITICALERROR, "readfromstream() returned other than 1!\n");
					ToBreak = 1;
					break;
				}
			}

			if(ToBreak) break;
			ToBreak = 0;

			gui_showstatus(STATUS_INFORMATION, "reply: %d - %s\n", ErrorCode, HTTPMessage);

			/* Dump the incoming HTTP reply */
			if (DUMP_HEADERS)
				gui_showstatus(STATUS_INFORMATION, "\nHTTP-Reply:\n%s\n", Buffer);

			/* Detect reply's content, apply only once */
			if(My_ti->RequestType == REQUEST_INFORMATION)
			{
				if ( !stricmp(ContentType, "application/octet-stream") )
				{
					if (strstr(Features, "broadcast"))
					{
						My_ci->ASFContentType = live_content;
						My_hi->Resumeable = FALSE;
					}
					else
					    {
						My_ci->ASFContentType = prerecorded_content;
						My_hi->Resumeable = TRUE;
					}
				}
				else /* Then what's it */
				    {
					if (!eos(&Stream_Parm))				/* try to read as much data as fits into the buffer */
					{
						unsigned char c = 0x00;
						unsigned int RedirSize = 0;
						unsigned int PrintableCount = 0;

						RedirSize = readfromstream(&Stream_Parm, Buffer, MAX_CHUNK_SIZE);
						for(i=0 ; i<RedirSize ; c=Buffer[i++]) /* Count all printable chars */
						{
							if( (c == ' ') || (c == '\t') || (c == '\r') || (c == '\n')) continue;
							if(isprint(c)) PrintableCount++;
						}
						if(PrintableCount >= RedirSize*3/4) /* If printable chars > 3/4, assume it as redirector */
						{
							gui_showstatus(0, "\n%s", Buffer);
							parse_redirection(My_Job, Buffer, RedirSize);
							My_ti->Reply = REPLY_RETRY;
						}
						else
						{
							gui_showstatus(STATUS_CRITICALERROR, "This is a HTTP content\n");
							My_ti->Reply = REPLY_HTTP;
						}
					}
					else
					    {
						gui_showstatus(STATUS_CRITICALERROR, "Unknown content-type: %s\n", ContentType);
					}
				}

				switch(ErrorCode)
				{
				case 200: /* ok */
					break;

				case 401:
					My_hi->RequirePassword = TRUE;
					ToBreak = 1;
					break;

				case 400: /* Bad Request */
				case 404: /* The famous 404 error */
					My_ti->Reply = REPLY_MMST;
					ToBreak = 1;
					break;

				case 453: /* "Not enough bandwidth" */
					My_ti->Reply = REPLY_RETRY;
					ToBreak = 1;
					break;

				default:  /* We dont know what it is */
					My_ci->ASFContentType = server_error;
					ToBreak = 1;
					break;
				}
			}

			if(ToBreak)
			{
				my_closesocket(ConnSocket);
				free(Stream_Parm.si);
				break;
			}

			/* Return if contenttype unknown */
			if ( eos(&Stream_Parm) || ((My_ci->ASFContentType != live_content) && (My_ci->ASFContentType != prerecorded_content)) )
			{
				my_closesocket(ConnSocket);
				free(Stream_Parm.si);
				break;
			}

			/* Handle live or prerecorded content */
			unsigned long MyFilePointer = 0;
			unsigned char TimeCodeString[64]={""};
			//unsigned int StartTimeHi = 0xffffffff;
			unsigned int StartTime    = 0xffffffff;
			unsigned int StartSeqNO   = 0xffffffff;
			//unsigned int EndOfHeaderPosition = 0;
			unsigned long HeaderLength = 0;
			unsigned int HeaderOffset = 0;

			/* The main loop for chunk extraction/ASF generation */
			for (;;)
			{
				unsigned short Type;
				unsigned int Length, Length2;
				unsigned int BodyLength;
				unsigned int SeqNO;
				unsigned short PartFlag;
				unsigned long TimeCode=0;
				unsigned int Progress; /* scaled from 0 to 10000 */
				unsigned int Got;

				/* Check for EOF and extract chunk header bytes are read one by one so this code remains portable to non-INTEL platforms */
				if (eos(&Stream_Parm))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Connection reset\n");
					break;
				}

				{ /* read basic chunk type */
					unsigned char c1, c2;
					readfromstream(&Stream_Parm, &c1, 1);
					readfromstream(&Stream_Parm, &c2, 1);
					Type = (c2<<8) + c1;

					/* These header types correspond to "H$", "D$" and "E$" (Header, Data and End) */
					if ((Type != MMSH_HEADER_CHUNK) && (Type != MMSH_DATA_CHUNK) && (Type != MMSH_END_CHUNK))
						gui_showstatus(STATUS_ERROR, "Unknown header type: %02X:%02X\n", c2, c1);
				}

				if (Type == MMSH_END_CHUNK)
				{
					gui_showstatus(STATUS_FINISH, "Transfer complete.\n");
					My_ti->Reply = REPLY_FINISH;
					break;
				}

				if (eos(&Stream_Parm))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Connection reset\n");
					break;
				}

				{ /* read chunk length (max 64k) */
					unsigned char l1, l2;
					readfromstream(&Stream_Parm, &l1, 1);
					readfromstream(&Stream_Parm, &l2, 1);
					Length = (l2<<8) + l1;
				}

				if (eos(&Stream_Parm))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Connection reset\n");
					break;
				}

				{ /* read chunk sequence number */
					unsigned char s1, s2, s3, s4;
					readfromstream(&Stream_Parm, &s1, 1);
					readfromstream(&Stream_Parm, &s2, 1);
					readfromstream(&Stream_Parm, &s3, 1);
					readfromstream(&Stream_Parm, &s4, 1);
					SeqNO   = (s4<<24) + (s3<<16) + (s2<<8) + s1;
				}

				if (eos(&Stream_Parm))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Connection reset\n");
					break;
				}

				{ /* read two unknown bytes */
					unsigned char u1, u2;
					readfromstream(&Stream_Parm, &u1, 1);
					readfromstream(&Stream_Parm, &u2, 1);
					PartFlag = (u2<<8) + u1;
				}

				if (eos(&Stream_Parm))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Connection reset\n");
					break;
				}

				{ /* read second length entry (length confirmation) */
					unsigned char l1, l2;
					readfromstream(&Stream_Parm, &l1, 1);
					readfromstream(&Stream_Parm, &l2, 1);
					Length2 = (l2<<8) + l1;
				}

				if (eos(&Stream_Parm))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Connection reset\n");
					break;
				}

				/* Sanity check on chunk header. Second length entry must match the first. */
				if (Length2 != Length)
				{
					gui_showstatus(STATUS_CRITICALERROR, "Length confirmation doesn't match!\n");
					break;
				}

				/* calculate length of chunk body. */
				BodyLength = Length-8;

				/* check if the body length exceeds our buffer size */
				if ((BodyLength > MAX_CHUNK_SIZE) || (BodyLength <= 0))
				{
					gui_showstatus(STATUS_CRITICALERROR, "Illegal ChunkLength. Chunk is %d bytes!\n", Length);
					break;
				}

				/* Read chunk's body data */
				if (Type != MMSH_HEADER_CHUNK) HeaderOffset = 0;
				Got = readfromstream(&Stream_Parm, Buffer + HeaderOffset, BodyLength);
				BodyLength += HeaderOffset;
				Got        += HeaderOffset;

				if (Type == MMSH_HEADER_CHUNK && My_ti->RequestType == REQUEST_INFORMATION)
				{
					/* Headers may be split into several parts with a rising sequence number. */
					if (PartFlag & 0x0400)  /* This indicates the first header part */
					{
						HeaderOffset = 0;
						get_long(&Buffer[0x10], &HeaderLength);
						HeaderLength += DATSEG_HDR_SIZE;
					}

					if (BodyLength < HeaderLength) 	/* header progress indicator */
						gui_showstatus(STATUS_INFORMATION, "receiving ASF header (%d/%d)!\n", BodyLength, HeaderLength);

					if (!(PartFlag & 0x0800)) /* Skip parsing the header if it hasn't been received completely */
					{
						HeaderOffset = BodyLength; /* next header partition will be appended */
						BodyLength = HeaderLength; /* this prevents saving a partial header to the output file */
					}
					else
					    {
						int Offs;

						/* finally got the header */
						gui_showstatus(STATUS_INFORMATION, "received ASF header!\n");

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
				}

				/* Try to extract a timecode from all known chunk/content types(only applies to data chunks) */
				if (Type == MMSH_DATA_CHUNK)
				{
					ustrcpy(TimeCodeString, "?????");
					TimeCode = 0;
					int TCStart;

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

				gui_showstatus(STATUS_INFORMATION, "%d | %7ldKB | %2d.%02d%% | HDR:%c:%c | %5dBYTES | SEQ:%06X | TC:%s\n",
					MyID,
					max( (signed long)(SeqNO+1)*My_ci->ChunkLength - My_ti->DestOffset, 0 )/1024 + 1,
					Progress / 100,
					Progress % 100,
					Type >> 8, Type,
					BodyLength,
					SeqNO,
					TimeCodeString );

				/* some statistics for data chunks only */
				if (Type == MMSH_DATA_CHUNK)
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
					case MMSH_HEADER_CHUNK:
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

					case MMSH_DATA_CHUNK:
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
							if(Type != MMSH_HEADER_CHUNK) gui_showstatus(STATUS_INFORMATION, "(%d) Chunk position below range, ignored.\n", MyID);
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
							ThisChunkSize -= ((ThisChunkPos + ThisChunkSize) - (My_ti->SourOffset + My_ti->DestLength));
						}
					}

					/* perform write action */
					fseek(My_Job->OutFile, ThisChunkPos-My_ti->DestOffset, SEEK_SET);
					fwrite(ThisChunkPtr, ThisChunkSize, 1, My_Job->OutFile);

					MyFilePointer = ThisChunkPos + ThisChunkSize;
				    My_ti->SourOffset += ThisChunkSize;
					My_ti->DestLength -= ThisChunkSize;

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
					//printf("I'm die! ID:%d\n", MyID);
					My_ti->Reply = REPLY_EXIT;
				    break;
				}
			} /* for (;;) */

			/* Cleanup */
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
