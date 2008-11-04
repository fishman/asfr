#ifndef __MING32_H_
#define __MINGW32_H_

#ifdef OLD_WINDOWS_HEADERS

/* networking headers for CygWin/EGCS 1.1 and old windows headers */
#include <Windows32/Base.h>
#include <Windows32/Sockets.h>
typedef int caddr_t;
#else /* OLD_WINDOWS_HEADERS */

/* networking headers for GCC 2.95.2 */
#include <winsock2.h>
#undef errno
#endif /* OLD_WINDOWS_HEADERS */

/* networking compatibility for CygWin/MINGW32 */
#define errno WSAGetLastError()

#endif /* __MINGW32_H_ */
