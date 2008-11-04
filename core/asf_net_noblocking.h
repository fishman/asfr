/*

	Header file for asf_net.cxx

*/

#ifndef ASF_NET_HPP
#define ASF_NET_HPP

SOCKET my_socket(int af, int type, int protocol);
int my_connect(SOCKET s, const struct sockaddr *name, int namelen);
int readfromstream(struct STREAM_PARM *My_Parm, unsigned char *dest, int length);
int my_send(SOCKET s, unsigned char *buf, int len, int flags);
int my_closesocket(SOCKET s);
int eos(struct STREAM_PARM *My_Parm);


#endif // ASF_NET_HPP

