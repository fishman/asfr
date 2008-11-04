#ifndef __WIN32_H_
#define __WIN32_H_

#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <process.h>
// #define errno WSAGetLastError()

/* Platform dependent definitions */
#include "compat.h"
#define usleep(x) _sleep(x)
#define MUTEX           while(My_Job->Mutex) _sleep(10); My_Job->Mutex=TRUE;
#define DEMUTEX         My_Job->Mutex=FALSE;

#endif /* __WIN32_H_ */
