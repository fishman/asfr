#ifndef __CORE_MMSH_H_
#define __CORE_MMSH_H_

#define DUMP_HEADERS 0

#define MMSH_DEFAULT_PORT 80

/* MMSH Chunk types */
#define MMSH_HEADER_CHUNK (('H' << 8) + '$')
#define MMSH_DATA_CHUNK   (('D' << 8) + '$')
#define MMSH_END_CHUNK    (('E' << 8) + '$')

#endif

