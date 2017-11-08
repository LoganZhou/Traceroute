//
// Created by HankZhou on 2017/11/1.
//

// Common routines for resolving addresses and hostnames
// Files:
//      resolve.cpp     - Common routines
//      resolve.h       - Header file for common routines
// Description:
//      This file contains common name resolution and name printing
//      routines and is used by many of the samples on this CD.
//		解析器，与DNS服务器联系处理主机名和地址之间的映射。
#include "stdafx.h"
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <cstdio>
#include <stdlib.h>
#include "resolve.h"
#include <ctype.h>

// Function: PrintAddress
// Description:
//    This routine takes a SOCKADDR structure and its length and prints
//    converts it to a string representation. This string is printed to the console via stdout.
//    接收一个 SOCKADDR 结构体以及其长度，输出其地址以及服务端口号。
int PrintAddress(SOCKADDR *sa, int salen)
{
    char    host[NI_MAXHOST], serv[NI_MAXSERV];
    int     hostlen = NI_MAXHOST, servlen = NI_MAXSERV, rc;

	//设置NI_NUMERICHOST标志将返回主机名的数字形式，而不是其名称
	//设置NI_NUMERICSERV标志将返回服务的端口号，而不是其名称
	//获取sockaddr指向的主机名和服务端口号
    rc = getnameinfo(sa, salen, host, hostlen, serv, servlen, NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
    {
        fprintf(stderr, "%s: getnameinfo() failed with error code %d\n", __FILE__, rc);
        return rc;
    }
    else
        printf("PrintAddress(): getnameinfo() is OK!\n");

    // 如果端口号为0，则不输出
    if (strcmp(serv, "0") != 0)
    {
        if (sa->sa_family == AF_INET)
            printf("[%s]:%s", host, serv);
        else
            printf("%s:%s", host, serv);
    }
    else
        printf("%s", host);

    return NO_ERROR;
}

// Function: FormatAddress
// Description:
//    This is similar to the PrintAddress function except that instead of
//    printing the string address to the console, it is formatted into the supplied string buffer.
//	  接收一个 SOCKADDR 结构体以及字符串缓冲区，将主机名和服务端口号写入字符串缓冲区
int FormatAddress(SOCKADDR *sa, int salen, char *addrbuf, int addrbuflen)
{
    char    host[NI_MAXHOST], serv[NI_MAXSERV];
    int     hostlen = NI_MAXHOST, servlen = NI_MAXSERV, rc;

    rc = getnameinfo(sa, salen, host, hostlen, serv, servlen, NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
    {
        fprintf(stderr, "%s: getnameinfo() failed with error code %d\n", __FILE__, rc);
        return rc;
    }
    else
        printf("FormatAddress(): getnameinfo() is OK!\n");

    if ( (strlen(host) + strlen(serv) + 1) > (unsigned)addrbuflen)
        return WSAEFAULT;
		//Bad address.
		//The system detected an invalid pointer address in attempting to use 
		//a pointer argument of a call.
    if (sa->sa_family == AF_INET)
        sprintf_s(addrbuf, sizeof(addrbuf), "%s:%s", host, serv);
    else if (sa->sa_family == AF_INET6)
        sprintf_s(addrbuf, sizeof(addrbuf), "[%s]:%s", host, serv);
    else
        addrbuf[0] = '\0';

    return NO_ERROR;
}

/**
域名系统(Domain Name System，DNS)主要用于主机名与IP地址间的映射。主机名可以是简单名字，
如solaris或bsdi，也可以是全限定域名FQDN(Fully Qualified Domain Name)，如solaris.kohala.com。
*/

// Function: ResolveAddress
// Description:
//    This routine resolves the specified address and returns a list of addrinfo
//    structure containing SOCKADDR structures representing the resolved addresses.
//    Note that if 'addr' is non-NULL, then getaddrinfo will resolve it whether
//    it is a string literal address or a hostname.
//	  将主机名和服务端口转换为 addrinfo 结构体
struct addrinfo *ResolveAddress(char *addr, char *port, int af, int type, int proto)
{
	//The addrinfo structure is used by the getaddrinfo function to hold host address information.
	//addrinfo 结构体是用来存储 getaddrinfo 函数所需要返回的信息类型
	//*res 是 getaddrinfo 函数返回的地址信息
    struct addrinfo hints,
            *res = NULL;
    int             rc;

    memset(&hints, 0, sizeof(hints));	//清0
    hints.ai_flags  = ((addr) ? 0 : AI_PASSIVE);	//如果地址为空，则为AI_PASSIVE，即服务端，用于bind
    hints.ai_family = af;
    hints.ai_socktype = type;
    hints.ai_protocol = proto;

	//getaddrinfo函数能够处理名字到地址以及服务到端口这两种转换，返回的是一个sockaddr结构
	//而不是一个地址列表，这些sockaddr结构随后可由套接字函数直使用
    rc = getaddrinfo(addr, port, &hints, &res);
    if (rc != 0)
    {
        printf("Invalid address %s, getaddrinfo() failed with error code %d\n", addr, rc);
        return NULL;
    }
    else
        printf("ResolveAddress(): getnameinfo() is OK!\n");

    return res;
}

// Function: ReverseLookup
// Description:
//    This routine takes a SOCKADDR and does a reverse lookup for the name
//    corresponding to that address.
//	  
int ReverseLookup(SOCKADDR *sa, int salen, char *buf, int buflen)
{
    char    host[NI_MAXHOST];
    int     hostlen=NI_MAXHOST, rc;

    rc = getnameinfo(sa, salen, host, hostlen, NULL, 0, 0);
    if (rc != 0)
    {
        fprintf(stderr, "getnameinfo() failed with error code %d\n", rc);
        return rc;
    }
    else
        printf("ReverseLookup(): getnameinfo() is OK!\n");
    strcpy_s(buf, sizeof(host), host);
    return NO_ERROR;
}