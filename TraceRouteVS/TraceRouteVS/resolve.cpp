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
//		����������DNS��������ϵ�����������͵�ַ֮���ӳ�䡣
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
//    ����һ�� SOCKADDR �ṹ���Լ��䳤�ȣ�������ַ�Լ�����˿ںš�
int PrintAddress(SOCKADDR *sa, int salen)
{
    char    host[NI_MAXHOST], serv[NI_MAXSERV];
    int     hostlen = NI_MAXHOST, servlen = NI_MAXSERV, rc;

	//����NI_NUMERICHOST��־��������������������ʽ��������������
	//����NI_NUMERICSERV��־�����ط���Ķ˿ںţ�������������
	//��ȡsockaddrָ����������ͷ���˿ں�
    rc = getnameinfo(sa, salen, host, hostlen, serv, servlen, NI_NUMERICHOST | NI_NUMERICSERV);
    if (rc != 0)
    {
        fprintf(stderr, "%s: getnameinfo() failed with error code %d\n", __FILE__, rc);
        return rc;
    }
    else
        printf("PrintAddress(): getnameinfo() is OK!\n");

    // ����˿ں�Ϊ0�������
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
//	  ����һ�� SOCKADDR �ṹ���Լ��ַ��������������������ͷ���˿ں�д���ַ���������
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
����ϵͳ(Domain Name System��DNS)��Ҫ������������IP��ַ���ӳ�䡣�����������Ǽ����֣�
��solaris��bsdi��Ҳ������ȫ�޶�����FQDN(Fully Qualified Domain Name)����solaris.kohala.com��
*/

// Function: ResolveAddress
// Description:
//    This routine resolves the specified address and returns a list of addrinfo
//    structure containing SOCKADDR structures representing the resolved addresses.
//    Note that if 'addr' is non-NULL, then getaddrinfo will resolve it whether
//    it is a string literal address or a hostname.
//	  ���������ͷ���˿�ת��Ϊ addrinfo �ṹ��
struct addrinfo *ResolveAddress(char *addr, char *port, int af, int type, int proto)
{
	//The addrinfo structure is used by the getaddrinfo function to hold host address information.
	//addrinfo �ṹ���������洢 getaddrinfo ��������Ҫ���ص���Ϣ����
	//*res �� getaddrinfo �������صĵ�ַ��Ϣ
    struct addrinfo hints,
            *res = NULL;
    int             rc;

    memset(&hints, 0, sizeof(hints));	//��0
    hints.ai_flags  = ((addr) ? 0 : AI_PASSIVE);	//�����ַΪ�գ���ΪAI_PASSIVE��������ˣ�����bind
    hints.ai_family = af;
    hints.ai_socktype = type;
    hints.ai_protocol = proto;

	//getaddrinfo�����ܹ��������ֵ���ַ�Լ����񵽶˿�������ת�������ص���һ��sockaddr�ṹ
	//������һ����ַ�б���Щsockaddr�ṹ�������׽��ֺ���ֱʹ��
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