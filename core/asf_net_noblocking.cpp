/* The following (blocking) socket calls are encapsulated in an event loop */
/* that allows the GUI code to handle user input during a blocking network */
/* operation */

/* The GUI code must set the socket to non-blocking mode, install an event */
/* notification mechanism on the socket and return on detection of a network */
/* event. */

#include "../asfr.h"
#include "asf_commonh.h"
#include "asf_net.h"


#define STREAMBUFSIZE 40960

struct STREAM_INFO
{
	char StreamBuffer[STREAMBUFSIZE];
	int sb_inpos;
	int sb_outpos;
	int networkeof;
	int networktimeout;
	int streameof;
};

typedef enum {
  RECV_OK=0,	// - No Error, Data received in dwLen
  RECV_ERROR,	// - Error receiving data
  RECV_CLOSE,	// - closed by peer
  RECV_TIMEOUT	// - Timeout in receving data.
} enReturn;

SOCKET my_socket(int af, int type, int protocol)
{
	SOCKET s;
	s = socket(af, type, protocol);
	return s;
}

int my_closesocket(SOCKET s)
{
	return closesocket(s);
}


int my_recv(SOCKET s, char *buf, int len, int& iRecv)
{

	struct timeval timeout;
	timeout.tv_sec = iRecv;	// Use the Recv as a flag.
	timeout.tv_usec = 0;

	struct fd_set readfds;
	FD_ZERO(&readfds);
	FD_SET(s,&readfds);

	// Use the select function to get a timeout, rather than to
	// wait for the termination.
	iRecv=select(s,&readfds,NULL,NULL,&timeout);
	// if(FD_ISSET(s,&readfds) and so on
	if(!iRecv)
		return RECV_TIMEOUT;
	if(iRecv==SOCKET_ERROR)
		return RECV_ERROR;

	iRecv=recv(s, buf, len,0);

	if(iRecv>0)
		return RECV_OK;
	if(iRecv==SOCKET_ERROR)
		return SOCKET_ERROR;
	if(!iRecv)
		return RECV_CLOSE;
        return SOCKET_ERROR;
}

int my_send(SOCKET s, unsigned char *buf, int len, int flags)
{
	int returnval=0;
	returnval = send(s, (const char*)buf, len, flags);
	return returnval;
}

int my_connect(SOCKET s, const struct sockaddr *name, int namelen)
{
	int returnval=0;
	returnval = connect(s, name, namelen);
	return returnval;
}

/* check end of stream status */
int eos(struct STREAM_PARM *My_Parm)
{
	return ((struct STREAM_INFO*)My_Parm->si)->streameof;
}

/* Stream input routines. This code uses a simple, circular buffer */
/* read one or more bytes from the stream. */
int readfromstream(struct STREAM_PARM *My_Parm, unsigned char *dest, int length)
{
	struct STREAM_INFO *si = (struct STREAM_INFO*)My_Parm->si;
	switch(My_Parm->RequestType)
	{
		case REQUEST_INIT:
				return sizeof(*si);
				break;

		case REQUEST_RESET:
				memset(si, 0, sizeof(*si));
				break;

		case REQUEST_GETDATA:
			{
				int lag;
				//int got = 0;
				int totalread = 0;

				while ((!si->streameof) && (length >0))
				{
					lag = (STREAMBUFSIZE + si->sb_outpos - si->sb_inpos) % STREAMBUFSIZE;
					if (lag == 0) lag = STREAMBUFSIZE;

					/* refill circular buffer only when it's total empty */
					if ((!si->networkeof) && (lag >= STREAMBUFSIZE))
				    {
						int toend = si->sb_outpos - si->sb_inpos - 1;
						if (toend < 0) toend = STREAMBUFSIZE - si->sb_inpos;
						if (toend > 0)
					    {
							// 10 seconds timout.
							int retval=10;
							switch(my_recv(My_Parm->conn_socket, &(si->StreamBuffer[si->sb_inpos]), toend,retval)) {
							case RECV_OK:	// - No Error, Data received in dwLen
								si->sb_inpos += retval;
								break;

							case RECV_ERROR:	// - Error receiving data
								gui_showstatus(STATUS_ERROR, "recv() failed: %d\n",errno);
								si->networkeof = 1;
								break;

							case RECV_CLOSE:	// - closed by peer
								si->networkeof = 1;
								break;

							case RECV_TIMEOUT:	// - Timeout in receving data.
								gui_showstatus(STATUS_ERROR, "Stream timeout\n");
								si->networktimeout=1;
								si->networkeof = 1;
								break;
							}
						}
					}

					/* check if circular buffer contains data */
					if (si->sb_outpos != si->sb_inpos)
					{
						/* report EOF only when circular buffer is empty */
						si->sb_inpos = si->sb_inpos % STREAMBUFSIZE;

						while (length > 0)
						{
							*dest++ = si->StreamBuffer[si->sb_outpos++];
							si->sb_outpos = si->sb_outpos % STREAMBUFSIZE;
							length--;
							totalread++;

							if (si->sb_outpos == si->sb_inpos) break;
						}
					}
					else
					{
						if (si->networkeof)
					    {
							si->streameof = 1;
							break;
						}
					}
				}
				return totalread;
				break;
			}
	}
	return 0;
}
