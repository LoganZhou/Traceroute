// TraceRouteVS.cpp: 定义控制台应用程序的入口点。
//

//
// Created by HankZhou on 2017/11/1.
//

// Sample: IPv4 and IPv6 Trace Route Sample
//
// Files:
//    iphdrv3.h     - IPv4 and IPv6 packet header definitions
//    Ipv4IPv6traceroutesrc.cpp   - this file
//    resolve.cpp   - Common name resolution routine
//    resolve.h     - Header file for common name resolution routines
//
// Description:
//    This sample illustrates how to use raw sockets to send ICMP
//    echo requests and receive their response in order to determine the
//    route to a particular destination. This sample performs
//    both IPv4 and IPv6 trace route operations. When using raw sockets,
//    the protocol value supplied to the socket API is used as the
//    protocol field (or next header field) of the IP packet. Then
//    as a part of the data submitted to sendto, we include both
//    the ICMP request and data. We start by setting the TTL value to one
//    and sending a request. When an intermediate router intercepts the
//    packet the TTL is decremented. If the value is zero, it sends an
//    ICMP TTL expired message which we receive. From the IP packet's
//    source field we have a location in the route to the destination.
//    With each send, the TTL is incremented by one until the specified
//    destination is reached.
//
// Command Line Options/Parameters:
//     Ipv4IPv6traceroute [-a 4|6] [-d] [-h ttl] [-w timeout] [host]
//
//     -a       Address family (IPv4 or IPv6)
//     -d       Do not resolve the addresses to hostnames
//     -h ttl   Maximum TTL value
//     -w time  Timeout in milliseconds to wait for a response
//     host     Hostname or literal address
//
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

// Link to ws2_32.lib
#include "stdafx.h"
#include <Winsock2.h>
#include <Ws2tcpip.h>
#include <stdio.h>
#include <stdlib.h>
#include "resolve.h"
#include "iphdrv3.h"
#pragma comment(lib, "Ws2_32.lib")

#define DEFAULT_DATA_SIZE      32			// default data size
#define DEFAULT_RECV_TIMEOUT   6000			// six second
#define DEFAULT_TTL            30			// default timeout

// Global variables
int   gAddressFamily = AF_UNSPEC,			// Address family to use
gProtocol = IPPROTO_ICMP,					// Protocol value
gTtl = DEFAULT_TTL,							// Default TTL value
gTimeout = DEFAULT_RECV_TIMEOUT;			// Amount of data to send
BOOL  bResolve = TRUE;						// Resolve addresses to host names
char *gDestination = NULL;					// Destination

// Function: usage
// Description:  Print usage information.
int usage(char *progname)
{
	printf("Usage: %s -r <host> [data size]\n", progname);
	printf("       -a 4|6       Address family\n");
	printf("       -d           Do not resolve addresses to hostnames\n");
	printf("       -h ttl       Maximum hops to search for target\n");
	printf("       -w timeout   Wait timeout in milliseconds for each reply\n");
	printf("        host        Remote machine to ping\n");
	printf("\n");
	return 0;
}

// Function: InitIcmpHeader
// Description: Helper function to fill in various stuff in our ICMP request.
void InitIcmpHeader(char *buf, int datasize)
{
	ICMP_HDR   *icmp_hdr = NULL;
	char       *datapart = NULL;

	icmp_hdr = (ICMP_HDR *)buf;
	icmp_hdr->icmp_type = ICMPV4_ECHO_REQUEST_TYPE;        // request an ICMP echo
	icmp_hdr->icmp_code = ICMPV4_ECHO_REQUEST_CODE;
	icmp_hdr->icmp_id = (USHORT)GetCurrentProcessId();
	icmp_hdr->icmp_checksum = 0;
	icmp_hdr->icmp_sequence = 0;
	icmp_hdr->icmp_timestamp = GetTickCount();

	datapart = buf + sizeof(ICMP_HDR);

	// Place some junk in the buffer.
	memset(datapart, 'E', datasize);
}

// Function: InitIcmp6Header
// Description: Initialize the ICMP6 header as well as the echo request header.
int InitIcmp6Header(char *buf, int datasize)
{
	ICMPV6_HDR          *icmp6_hdr = NULL;
	ICMPV6_ECHO_REQUEST *icmp6_req = NULL;
	char                *datapart = NULL;

	// Initialize the ICMP6 header fields
	icmp6_hdr = (ICMPV6_HDR *)buf;
	icmp6_hdr->icmp6_type = ICMPV6_ECHO_REQUEST_TYPE;
	icmp6_hdr->icmp6_code = ICMPV6_ECHO_REQUEST_CODE;
	icmp6_hdr->icmp6_checksum = 0;

	// Initialize the echo request fields
	icmp6_req = (ICMPV6_ECHO_REQUEST *)(buf + sizeof(ICMPV6_HDR));
	icmp6_req->icmp6_echo_id = (USHORT)GetCurrentProcessId();
	icmp6_req->icmp6_echo_sequence = 0;

	datapart = (char *)buf + sizeof(ICMPV6_HDR) + sizeof(ICMPV6_ECHO_REQUEST);
	memset(datapart, '$', datasize);

	return (sizeof(ICMPV6_HDR) + sizeof(ICMPV6_ECHO_REQUEST));
}

// Function: checksum
// Description:
//    This function calculates the 16-bit one's complement sum
//    of the supplied buffer (ICMP) header.
USHORT checksum(USHORT *buffer, int size)
{
	unsigned long cksum = 0;	//32位
	//累加到cksum
	while (size > 1)
	{
		cksum += *buffer++;
		size -= sizeof(USHORT);
	}
	//如果为奇数，将最后一个字节扩展到双字，再累加到cksum中
	if (size)
	{
		cksum += *(UCHAR*)buffer;
	}
	//将cksum的高16位和低16位相加，取反后得到校验和
	cksum = (cksum >> 16) + (cksum & 0xffff);
	cksum += (cksum >> 16);
	return (USHORT)(~cksum);
}

// Function: ValidateArgs
// Description: Parse the command line arguments.
void ValidateArgs(int argc, char **argv)
{
	int    i;
	for (i = 1; i < argc; i++)
	{
		if ((argv[i][0] == '-') || (argv[i][0] == '/'))//如果字符串以'-'或'/'开始的是参数
		{
			switch (tolower(argv[i][1]))//转小写
			{
			case 'a':        // address family
				if (i + 1 >= argc)
					usage(argv[0]);
				if (argv[i + 1][0] == '4')
					gAddressFamily = AF_INET;
				else if (argv[i + 1][0] == '6')
					gAddressFamily = AF_INET6;
				else
					usage(argv[0]);
				i++;
				break;
			case 'd':        // Don't resolve addresses
				bResolve = FALSE;
				break;
			case 'h':        // Set TTL value
				if (i + 1 >= argc)
					usage(argv[0]);
				gTtl = atoi(argv[++i]);
				break;
			case 'w':        // Timeout in milliseconds for each reply
				if (i + 1 >= argc)
					usage(argv[0]);
				gTimeout = atoi(argv[++i]);
				break;
			default:
				usage(argv[0]);
				break;
			}
		}
		else
		{
			gDestination = argv[i];
		}
	}
	return;
}

// Function: SetIcmpSequence
// Description: This routine sets the sequence number of the ICMP request packet.
void SetIcmpSequence(char *buf)
{
	ULONG    sequence = 0;

	sequence = GetTickCount();	//系统启动后的毫秒数
	if (gAddressFamily == AF_INET)
	{
		ICMP_HDR    *icmpv4 = NULL;

		icmpv4 = (ICMP_HDR *)buf;
		icmpv4->icmp_sequence = (USHORT)sequence;
	}
	else if (gAddressFamily == AF_INET6)
	{
		ICMPV6_HDR          *icmpv6 = NULL;
		ICMPV6_ECHO_REQUEST *req6 = NULL;

		icmpv6 = (ICMPV6_HDR *)buf;
		req6 = (ICMPV6_ECHO_REQUEST *)(buf + sizeof(ICMPV6_HDR));

		req6->icmp6_echo_sequence = (USHORT)sequence;
	}
}

// Function: ComputeIcmp6PseudoHeaderChecksum
// Description:
//    This routine computes the ICMP6 checksum which includes the pseudo
//    header of the IPv6 header (see RFC2460 and RFC2463). The one difficulty
//    here is we have to know the source and destination IPv6 addresses which
//    will be contained in the IPv6 header in order to compute the checksum.
//    To do this we call the SIO_ROUTING_INTERFACE_QUERY ioctl to find which
//    local interface for the outgoing packet.
USHORT ComputeIcmp6PseudoHeaderChecksum(SOCKET s, char *icmppacket, int icmplen, struct addrinfo *dest)
{
	SOCKADDR_STORAGE localif;
	DWORD            bytes;
	char             tmp[65535], *ptr = NULL, proto = 0, zero = 0;
	int              rc, total, length, i;

	// Find out which local interface for the destination
	rc = WSAIoctl(s, SIO_ROUTING_INTERFACE_QUERY, dest->ai_addr, dest->ai_addrlen,
		(SOCKADDR *)&localif, sizeof(localif), &bytes, NULL, NULL);
	if (rc == SOCKET_ERROR)
	{
		fprintf(stderr, "WSAIoctl() failed with error code %d\n", WSAGetLastError());
		return -1;
	}
	else
		printf("WSAIoctl() is OK!\n");

	// We use a temporary buffer to calculate the pseudo header.
	ptr = tmp;
	total = 0;

	// Copy source address
	memcpy(ptr, &((SOCKADDR_IN6 *)&localif)->sin6_addr, sizeof(struct in6_addr));
	ptr += sizeof(struct in6_addr);
	total += sizeof(struct in6_addr);
	// Copy destination address
	memcpy(ptr, &((SOCKADDR_IN6 *)dest->ai_addr)->sin6_addr, sizeof(struct in6_addr));
	ptr += sizeof(struct in6_addr);
	total += sizeof(struct in6_addr);
	// Copy ICMP packet length
	length = htonl(icmplen);

	memcpy(ptr, &length, sizeof(length));
	ptr += sizeof(length);
	total += sizeof(length);
	// Zero the 3 bytes
	memset(ptr, 0, 3);
	ptr += 3;
	total += 3;
	// Copy next hop header
	proto = IPPROTO_ICMP6;

	memcpy(ptr, &proto, sizeof(proto));
	ptr += sizeof(proto);
	total += sizeof(proto);
	// Copy the ICMP header and payload
	memcpy(ptr, icmppacket, icmplen);
	ptr += icmplen;
	total += icmplen;

	for (i = 0; i < icmplen % 2; i++)
	{
		*ptr = 0;
		ptr++;
		total++;
	}
	return checksum((USHORT *)tmp, total);
}

// Function: ComputeIcmpChecksum
// Description:
//    This routine computes the checksum for the ICMP request. For IPv4 its
//    easy, just compute the checksum for the ICMP packet and data. For IPv6,
//    its more complicated. The pseudo checksum has to be computed for IPv6
//    which includes the ICMP6 packet and data plus portions of the IPv6
//    header which is difficult since we aren't building our own IPv6 header.
void ComputeIcmpChecksum(SOCKET s, char *buf, int packetlen, struct addrinfo *dest)
{
	if (gAddressFamily == AF_INET)
	{
		ICMP_HDR    *icmpv4 = NULL;

		icmpv4 = (ICMP_HDR *)buf;
		icmpv4->icmp_checksum = 0;
		icmpv4->icmp_checksum = checksum((USHORT *)buf, packetlen);
	}
	else if (gAddressFamily == AF_INET6)
	{
		ICMPV6_HDR  *icmpv6 = NULL;

		icmpv6 = (ICMPV6_HDR *)buf;
		icmpv6->icmp6_checksum = 0;
		icmpv6->icmp6_checksum = ComputeIcmp6PseudoHeaderChecksum(s, buf, packetlen, dest);
	}
}

// Function: PostRecvfrom
// Description: This routine posts an overlapped WSARecvFrom on the raw socket.
int PostRecvfrom(SOCKET s, char *buf, int buflen, SOCKADDR *from, int *fromlen, WSAOVERLAPPED *ol)
{
	WSABUF  wbuf;
	DWORD   flags, bytes;	//DOWRD = unsigned long，32位
	int     rc;

	wbuf.buf = buf;
	wbuf.len = buflen;

	flags = 0;

	rc = WSARecvFrom(s, &wbuf, 1, &bytes, &flags, from, fromlen, ol, NULL);
	if (rc == SOCKET_ERROR)
	{
		if (WSAGetLastError() != WSA_IO_PENDING)
		{
			fprintf(stderr, "WSARecvFrom() failed with error code %d\n", WSAGetLastError());
			return SOCKET_ERROR;
		}
	}
	else
		printf("WSARecvFrom() is OK!\n");

	return NO_ERROR;
}

// Function: AnalyzePacket
// Description:
//    This routines finds the ICMP packet within the encapsulated header and
//    verifies that the ICMP packet is a TTL expired or echo reply message. If not then an error is returned.
int AnalyzePacket(char *buf, int bytes)
{
	int     hdrlen = 0, routes = 0, rc;

	rc = NO_ERROR;
	if (gAddressFamily == AF_INET)
	{
		IPV4_HDR        *v4hdr = NULL;
		ICMP_HDR        *icmphdr = NULL;

		v4hdr = (IPV4_HDR *)buf;
		hdrlen = (v4hdr->ip_verlen & 0x0F) * 4;	//取出后4位长度然后乘以4

		if (v4hdr->ip_protocol == IPPROTO_ICMP)
		{
			icmphdr = (ICMP_HDR *)&buf[hdrlen];//定位到ICMP头部

			if ((icmphdr->icmp_type != ICMPV4_TIMEOUT) &&				//既不是TTL超时，又不是到达回应	
				(icmphdr->icmp_type != ICMPV4_ECHO_REPLY_TYPE) &&		
				(icmphdr->icmp_code != ICMPV4_ECHO_REPLY_CODE))			
			{
				printf("Received ICMP message type %d instead of TTL expired!\n", icmphdr->icmp_type);
				rc = SOCKET_ERROR;
			}
		}
	}
	else if (gAddressFamily == AF_INET6)
	{
		IPV6_HDR        *v6hdr = NULL;
		ICMPV6_HDR      *icmp6 = NULL;

		v6hdr = (IPV6_HDR *)buf;

		if (v6hdr->ipv6_nexthdr == IPPROTO_ICMP6)
		{
			icmp6 = (ICMPV6_HDR *)&buf[sizeof(IPV6_HDR)];

			if ((icmp6->icmp6_type != ICMPV6_TIME_EXCEEDED_TYPE) &&
				(icmp6->icmp6_code != ICMPV6_TIME_EXCEEDED_CODE) &&
				(icmp6->icmp6_type != ICMPV6_ECHO_REPLY_TYPE) &&
				(icmp6->icmp6_code != ICMPV6_ECHO_REPLY_CODE))
			{
				printf("Received ICMP6 message type %d instead of TTL expired!\n", icmp6->icmp6_type);
				rc = SOCKET_ERROR;
			}
		}
	}
	return rc;
}

// Function: SetTtl
// Description: Sets the TTL on the socket.
int SetTtl(SOCKET s, int ttl)
{
	int     optlevel, option, rc;

	rc = NO_ERROR;
	if (gAddressFamily == AF_INET)
	{
		optlevel = IPPROTO_IP;
		option = IP_TTL;
	}
	else if (gAddressFamily == AF_INET6)
	{
		optlevel = IPPROTO_IPV6;
		option = IPV6_UNICAST_HOPS;
	}
	else
	{
		rc = SOCKET_ERROR;
	}

	if (rc == NO_ERROR)
	{
		rc = setsockopt(s, optlevel, option, (char *)&ttl, sizeof(ttl));
	}

	if (rc == SOCKET_ERROR)
	{
		fprintf(stderr, "SetTtl(): setsockopt() failed with error code %d\n", WSAGetLastError());
	}
	else
		printf("SetTtl(): setsockopt() should be fine!\n");

	return rc;
}

// Function: IsSockaddrEqual
// Description:
//    This routines compares two SOCKADDR structure to determine
//    whether the address portion of them are equal. Zero is returned
//    for equal; non-zero for not equal.
int IsSockaddrEqual(SOCKADDR *sa1, SOCKADDR *sa2)
{
	int rc;

	rc = 1;
	if (sa1->sa_family == sa2->sa_family)
	{
		if (sa1->sa_family == AF_INET)
		{	//按字节比较
			rc = memcmp(
				&((SOCKADDR_IN *)sa1)->sin_addr,
				&((SOCKADDR_IN *)sa2)->sin_addr,
				sizeof(struct in_addr)
			);
			rc = rc;
		}
		else if (sa1->sa_family == AF_INET6)
		{
			rc = memcmp(
				&((SOCKADDR_IN6 *)sa1)->sin6_addr,
				&((SOCKADDR_IN6 *)sa2)->sin6_addr,
				sizeof(struct in6_addr)
			);
			rc = rc;
		}
	}
	return rc;
}

// Function: main
// Description:
//    Setup the ICMP raw socket and create the ICMP header. Add
//    the appropriate IP option header and start sending ICMP
//    echo requests to the endpoint. For each send and receive we
//    set a timeout value so that we don't wait forever for a
//    response in case the endpoint is not responding. When we
//    receive a packet decode it.
int main(int argc, char **argv)
{
	WSADATA            wsd;
	WSAOVERLAPPED      recvol;
	SOCKET             s = INVALID_SOCKET;
	char              *icmpbuf = NULL, recvbuf[0xFFFF], hopname[512];
	struct addrinfo   *dest = NULL, *local = NULL;
	SOCKADDR_STORAGE   from;
	DWORD              bytes, flags;
	int                packetlen = 0, recvbuflen = 0xFFFF, hopbuflen = 512, fromlen, notdone, time = 0, ttl, rc;

	if (argc < 2)	//至少一个参数
	{
		usage(argv[0]);
		exit(1);
	}

	// Load Winsock
	//加载Winsock
	if (WSAStartup(MAKEWORD(2, 2), &wsd) != 0)
	{
		printf("WSAStartup() failed with error code %d\n", WSAGetLastError());
		return -1;
	}
	else
		printf("WSAStartup() is pretty fine!\n");

	// Parse the command line
	// 判断输入参数是否合法
	ValidateArgs(argc, argv);

	// Resolve the destination address
	// 解析目的主机地址
	dest = ResolveAddress(gDestination, "0", gAddressFamily, 0, 0);
	if (dest == NULL)
	{
		printf("Bad name %s\n", gDestination);
		return -1;
	}
	else
		printf("ResolveAddress() is OK!\n");

	//设置IP协议版本
	gAddressFamily = dest->ai_family;

	if (gAddressFamily == AF_INET)
		gProtocol = IPPROTO_ICMP;
	else if (gAddressFamily == AF_INET6)
		gProtocol = IPPROTO_ICMP6;

	// Get the bind address
	// 获取本机IP地址，地址为NULL则自动填充为本机IP地址
	local = ResolveAddress(NULL, "0", gAddressFamily, 0, 0);
	if (local == NULL)
	{
		printf("Unable to obtain the bind address!\n");
		return -1;
	}
	else
		printf("ResolveAddress() is fine!\n");

	// Create the raw socket
	// 创建一个 raw socket
	s = socket(gAddressFamily, SOCK_RAW, gProtocol);
	if (s == INVALID_SOCKET)
	{
		printf("socket() failed with error code %d\n", WSAGetLastError());
		return -1;
	}
	else
		printf("socket() is OK!\n");

	// Figure out the size of the ICMP header and payload
	// 加上ICMP报头的长度
	if (gAddressFamily == AF_INET)
		packetlen += sizeof(ICMP_HDR);
	else if (gAddressFamily == AF_INET6)
		packetlen += sizeof(ICMPV6_HDR) + sizeof(ICMPV6_ECHO_REQUEST);

	// Add in the data size
	// 加上默认数据段长度
	packetlen += DEFAULT_DATA_SIZE;

	// Allocate the buffer that will contain the ICMP request
	// 分配ICMP报文缓冲区（该缓冲区不可移动）
	icmpbuf = (char *)HeapAlloc(GetProcessHeap(), HEAP_ZERO_MEMORY, packetlen);
	if (icmpbuf == NULL)
	{
		fprintf(stderr, "HeapAlloc() for ICMP buffer failed with error code %d\n", GetLastError());
		return -1;
	}
	else
		printf("HeapAlloc() for ICMP buffer is OK!\n");

	// Initialize the ICMP headers
	// 初始化ICMP报头
	if (gAddressFamily == AF_INET)
	{
		InitIcmpHeader(icmpbuf, DEFAULT_DATA_SIZE);
	}
	else if (gAddressFamily == AF_INET6)
	{
		InitIcmp6Header(icmpbuf, DEFAULT_DATA_SIZE);
	}

	// Bind the socket -- need to do this since we post a receive first
	// 将socket和本机IP地址、端口绑定
	rc = bind(s, local->ai_addr, local->ai_addrlen);
	if (rc == SOCKET_ERROR)
	{
		fprintf(stderr, "bind() failed with error code %d\n", WSAGetLastError());
		return -1;
	}
	else
		printf("bind() is OK!\n");

	// Setup the receive operation
	// 初始化重叠I/O，并注册事件
	memset(&recvol, 0, sizeof(recvol));
	recvol.hEvent = WSACreateEvent();	

	// Post the first overlapped receive
	// 先挂起一个重叠I/O（非阻塞的接收操作），然后再发送
	fromlen = sizeof(from);
	PostRecvfrom(s, recvbuf, recvbuflen, (SOCKADDR *)&from, &fromlen, &recvol);	//由于需要记录路由信息，所以用WSARecvfrom

	//输出需要追踪的目的地址信息
	printf("\nTraceroute to %s [", gDestination);
	PrintAddress(dest->ai_addr, dest->ai_addrlen);
	printf("]\nover a maximum of %d hops\n\n", gTtl);

	ttl = 1;//将TTL初始化为1

	// Start sending the ICMP requests
	// 开始发送ICMP请求
	do
	{
		notdone = 1;

		SetTtl(s, ttl);//将socket的TTL置为ttl

		// Set the sequence number and compute the checksum
		// 设置icmp报文的序列号
		SetIcmpSequence(icmpbuf);
		ComputeIcmpChecksum(s, icmpbuf, packetlen, dest);//计算icmp报文的校验和
		// Send the ICMP echo request
		// 计时，并发送ICMP报文
		time = GetTickCount();
		rc = sendto(s, icmpbuf, packetlen, 0, dest->ai_addr, dest->ai_addrlen);
		if (rc == SOCKET_ERROR)
		{
			fprintf(stderr, "sendto() failed with error code %d\n", WSAGetLastError());
			return -1;
		}
		else
			printf("sendto() is OK\n");

		// Wait for a response
		// 等待回应报文
		rc = WaitForSingleObject((HANDLE)recvol.hEvent, gTimeout);//等待挂起的接收操作
		if (rc == WAIT_FAILED)
		{
			fprintf(stderr, "WaitForSingleObject() failed with error code %d\n", GetLastError());
			return -1;
		}
		else if (rc == WAIT_TIMEOUT)
		{
			printf("Request timed out.\n");
		}
		else
		{
			printf("WaitForSingleObject() should be fine lol!\n");
			// Check for an error
			// 获取重叠I/O的结果，并检查是否出错
			rc = WSAGetOverlappedResult(s, &recvol, &bytes, FALSE, &flags);
			if (rc == FALSE)
			{
				fprintf(stderr, "WSAGetOverlappedResult() failed with error code %d\n", WSAGetLastError());
			}
			else
				printf("WSAGetOverlappedResult() looks OK!\n");

			time = GetTickCount() - time;
			//time = time - GetTickCount();
			WSAResetEvent(recvol.hEvent);	//重置事件对象为未置信号
			// See if we got an ICMP ttl expired or echo reply, if not ignore and receive again.
			if (AnalyzePacket(recvbuf, bytes) == NO_ERROR)	//分析是否是需要处理的数据包
			{
				if (bResolve)	//如果需要对收到的地址进行解析，则调用ReverseLookup进行解析
				{
					if (ReverseLookup((SOCKADDR *)&from, fromlen, hopname, hopbuflen) != NO_ERROR)
					{
						printf("ReverseLookup() failed lor!\n");
					}
					else
					{
						printf("TTL:%d   Time:%d ms   Hop name: %s [", ttl, time, hopname);
						PrintAddress((SOCKADDR *)&from, fromlen);
						printf("]\n");
					}
				}

				else
				{
					printf("TTL:%d   Time:%d ms   ", ttl, time);
					PrintAddress((SOCKADDR *)&from, fromlen);
					printf("\n");
				}
				// See if the response is from the desired destination
				// 如果回应源为目的IP，则完成追踪
				notdone = IsSockaddrEqual(dest->ai_addr, (SOCKADDR *)&from);
				// Increment the TTL
				ttl++;
			}
			// Post another receive
			// 挂起新的重叠I/O操作
			if (notdone)
			{
				fromlen = sizeof(from);
				PostRecvfrom(s, recvbuf, recvbuflen, (SOCKADDR *)&from, &fromlen, &recvol);
			}
		}
		Sleep(1000);
	} while ((notdone) && (ttl < gTtl));

	// Cleanup
	printf("Cleaning up stage...\n");
	freeaddrinfo(dest);
	freeaddrinfo(local);
	if (s != INVALID_SOCKET)
		closesocket(s);
	HeapFree(GetProcessHeap(), 0, icmpbuf);
	WSACleanup();
	return 0;
}