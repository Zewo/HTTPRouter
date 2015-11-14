/*

  Copyright (c) 2015 Martin Sustrik

  Permission is hereby granted, free of charge, to any person obtaining a copy
  of this software and associated documentation files (the "Software"),
  to deal in the Software without restriction, including without limitation
  the rights to use, copy, modify, merge, publish, distribute, sublicense,
  and/or sell copies of the Software, and to permit persons to whom
  the Software is furnished to do so, subject to the following conditions:

  The above copyright notice and this permission notice shall be included
  in all copies or substantial portions of the Software.

  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS
  IN THE SOFTWARE.

*/

#if defined __linux__
#define _GNU_SOURCE
#include <netdb.h>
#include <sys/eventfd.h>
#endif

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#if !defined __sun
#include <ifaddrs.h>
#endif
#include <unistd.h>

#include "ip.h"
#include "tcp.h"
#include "utils.h"

CT_ASSERT(sizeof(ipaddr) >= sizeof(struct sockaddr_in));
CT_ASSERT(sizeof(ipaddr) >= sizeof(struct sockaddr_in6));

static ipaddr tide_ipany(int port, int mode)
{
    ipaddr addr;
    if(tide_slow(port < 0 || port > 0xffff)) {
        ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
        errno = EINVAL;
        return addr;
    }
    if (mode == 0 || mode == IPADDR_IPV4 || mode == IPADDR_PREF_IPV4) {
        struct sockaddr_in *ipv4 = (struct sockaddr_in*)&addr;
        ipv4->sin_family = AF_INET;
        ipv4->sin_addr.s_addr = htonl(INADDR_ANY);
        ipv4->sin_port = htons((uint16_t)port);
    }
    else {
        struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)&addr;
        ipv6->sin6_family = AF_INET6;
        memcpy(&ipv6->sin6_addr, &in6addr_any, sizeof(in6addr_any));
        ipv6->sin6_port = htons((uint16_t)port);
    }
    errno = 0;
    return addr;
}

/* Convert literal IPv4 address to a binary one. */
static ipaddr tide_ipv4_literal(const char *addr, int port) {
    ipaddr raddr;
    struct sockaddr_in *ipv4 = (struct sockaddr_in*)&raddr;
    int rc = inet_pton(AF_INET, addr, &ipv4->sin_addr);
    tide_assert(rc >= 0);
    if(rc == 1) {
        ipv4->sin_family = AF_INET;
        ipv4->sin_port = htons((uint16_t)port);
        errno = 0;
        return raddr;
    }
    ipv4->sin_family = AF_UNSPEC;
    errno = EINVAL;
    return raddr;
}

/* Convert literal IPv6 address to a binary one. */
static ipaddr tide_ipv6_literal(const char *addr, int port) {
    ipaddr raddr;
    struct sockaddr_in6 *ipv6 = (struct sockaddr_in6*)&raddr;
    int rc = inet_pton(AF_INET6, addr, &ipv6->sin6_addr);
    tide_assert(rc >= 0);
    if(rc == 1) {
        ipv6->sin6_family = AF_INET6;
        ipv6->sin6_port = htons((uint16_t)port);
        errno = 0;
        return raddr;
    }
    ipv6->sin6_family = AF_UNSPEC;
    errno = EINVAL;
    return raddr;
}

/* Convert literal IPv4 or IPv6 address to a binary one. */
static ipaddr tide_ipliteral(const char *addr, int port, int mode) {
    ipaddr raddr;
    struct sockaddr *sa = (struct sockaddr*)&raddr;
    if(tide_slow(!addr || port < 0 || port > 0xffff)) {
        sa->sa_family = AF_UNSPEC;
        errno = EINVAL;
        return raddr;
    }
    switch(mode) {
    case IPADDR_IPV4:
        return tide_ipv4_literal(addr, port);
    case IPADDR_IPV6:
        return tide_ipv6_literal(addr, port);
    case 0:
    case IPADDR_PREF_IPV4:
        raddr = tide_ipv4_literal(addr, port);
        if(errno == 0)
            return raddr;
        return tide_ipv6_literal(addr, port);
    case IPADDR_PREF_IPV6:
        raddr = tide_ipv6_literal(addr, port);
        if(errno == 0)
            return raddr;
        return tide_ipv4_literal(addr, port);
    default:
        tide_assert(0);
    }
}

int tide_ipfamily(ipaddr addr) {
    return ((struct sockaddr*)&addr)->sa_family;
}

int tide_iplen(ipaddr addr) {
    return tide_ipfamily(addr) == AF_INET ?
        sizeof(struct sockaddr_in) : sizeof(struct sockaddr_in6);
}

int tide_ipport(ipaddr addr) {
    return ntohs(tide_ipfamily(addr) == AF_INET ?
        ((struct sockaddr_in*)&addr)->sin_port :
        ((struct sockaddr_in6*)&addr)->sin6_port);
}

ipaddr iplocal(const char *name, int port, int mode) {
    if(!name)
        return tide_ipany(port, mode);
    ipaddr addr = tide_ipliteral(name, port, mode);
#if defined __sun
    return addr;
#else
    if(errno == 0)
       return addr;
    /* Address is not a literal. It must be an interface name then. */
    struct ifaddrs *ifaces = NULL;
    int rc = getifaddrs(&ifaces);
    tide_assert (rc == 0);
    tide_assert (ifaces);
    /*  Find first IPv4 and first IPv6 address. */
    struct ifaddrs *ipv4 = NULL;
    struct ifaddrs *ipv6 = NULL;
    struct ifaddrs *it;
    for(it = ifaces; it != NULL; it = it->ifa_next) {
        if(!it->ifa_addr)
            continue;
        if(strcmp(it->ifa_name, name) != 0)
            continue;
        switch(it->ifa_addr->sa_family) {
        case AF_INET:
            tide_assert(!ipv4);
            ipv4 = it;
            break;
        case AF_INET6:
            tide_assert(!ipv6);
            ipv6 = it;
            break;
        }
        if(ipv4 && ipv6)
            break;
    }
    /* Choose the correct address family based on mode. */
    switch(mode) {
    case IPADDR_IPV4:
        ipv6 = NULL;
        break;
    case IPADDR_IPV6:
        ipv4 = NULL;
        break;
    case 0:
    case IPADDR_PREF_IPV4:
        if(ipv4)
           ipv6 = NULL;
        break;
    case IPADDR_PREF_IPV6:
        if(ipv6)
           ipv4 = NULL;
        break;
    default:
        tide_assert(0);
    }
    if(ipv4) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)&addr;
        memcpy(inaddr, ipv4->ifa_addr, sizeof (struct sockaddr_in));
        inaddr->sin_port = htons(port);
        freeifaddrs(ifaces);
        errno = 0;
        return addr;
    }
    if(ipv6) {
        struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)&addr;
        memcpy(inaddr, ipv6->ifa_addr, sizeof (struct sockaddr_in6));
        inaddr->sin6_port = htons(port);
        freeifaddrs(ifaces);
        errno = 0;
        return addr;
    }
    freeifaddrs(ifaces);
    ((struct sockaddr*)&addr)->sa_family = AF_UNSPEC;
    errno = ENODEV;
    return addr;
#endif
}

ipaddr ipremote(const char *name, int port, int mode) {
    ipaddr addr = tide_ipliteral(name, port, mode);
#if !defined HAVE_LIBANL
    return addr;
#else
    if(errno == 0)
       return addr;
    struct addrinfo request;
    memset(&request, 0, sizeof(request));
    request.ai_family = AF_UNSPEC;
    request.ai_socktype = SOCK_STREAM;
    struct gaicb gcb;
    memset(&gcb, 0, sizeof(gcb));
    gcb.ar_name = name;
    gcb.ar_service = NULL;
    gcb.ar_request = &request;
    gcb.ar_result = NULL;
    struct gaicb *pgcb = &gcb;
    int rc = getaddrinfo_a(GAI_WAIT, &pgcb, 1, NULL);
    if(tide_slow(rc != 0)) {
        if(rc == EAI_AGAIN || rc == EAI_MEMORY) {
            errno = ENOMEM;
            return addr;
        }
        tide_assert(0);
    }
    rc = gai_error(&gcb);
    if(rc != 0) {
        errno = EINVAL;
        return addr;
    }
    struct addrinfo *ipv4 = NULL;
    struct addrinfo *ipv6 = NULL;
    struct addrinfo *it = gcb.ar_result;
    while(it) {
        if(!ipv4 && it->ai_family == AF_INET)
            ipv4 = it;
        if(!ipv6 && it->ai_family == AF_INET6)
            ipv6 = it;
        if(ipv4 && ipv6)
            break;
        it = it->ai_next;
    }
    switch(mode) {
    case IPADDR_IPV4:
        ipv6 = NULL;
        break;
    case IPADDR_IPV6:
        ipv4 = NULL;
        break;
    case 0:
    case IPADDR_PREF_IPV4:
        if(ipv4)
           ipv6 = NULL;
        break;
    case IPADDR_PREF_IPV6:
        if(ipv6)
           ipv4 = NULL;
        break;
    default:
        tide_assert(0);
    }
    if(ipv4) {
        struct sockaddr_in *inaddr = (struct sockaddr_in*)&addr;
        memcpy(inaddr, ipv4->ai_addr, sizeof (struct sockaddr_in));
        inaddr->sin_port = htons(port);
    }
    if(ipv6) {
        struct sockaddr_in6 *inaddr = (struct sockaddr_in6*)&addr;
        memcpy(inaddr, ipv6->ai_addr, sizeof (struct sockaddr_in6));
        inaddr->sin6_port = htons(port);
    }
    freeaddrinfo(gcb.ar_result);
    errno = 0;
    return addr;
#endif
}

