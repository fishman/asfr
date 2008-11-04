#ifndef __LINUX_H_
#define __LINUX_H_

/* networking headers for Unix platform */
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <netdb.h>
#include <errno.h>
#include <pthread.h>

/* System dependent definitions */
#include "compat.h"

/* networking compatibility for Unix platform */
#define SOCKET int
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
typedef struct sockaddr_in SOCKADDR_IN;
#define closesocket close

/* string compatbilitiy for Unix platform */
#define stricmp strcasecmp
#define strnicmp strncasecmp
 
#define max(x,y) (((x)>(y)) ? (x) : (y))
#define TRUE 1
#define FALSE 0

#define pthread_attr_default NULL
#define SIG_TO_KILL_THREAD  SIGSTOP
#define MUTEX           while(My_Job->Mutex) usleep(10); My_Job->Mutex=TRUE;
#define DEMUTEX         My_Job->Mutex=FALSE;

#endif /* __LINUX_H_ */
