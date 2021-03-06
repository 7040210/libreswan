/* ip address type, for libreswan
 *
 * Copyright (C) 1998, 1999, 2000  Henry Spencer.
 * Copyright (C) 1999, 2000, 2001  Richard Guy Briggs
 * Copyright (C) 2019 Andrew Cagney <cagney@gnu.org>
 *
 * This library is free software; you can redistribute it and/or modify it
 * under the terms of the GNU Library General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or (at your
 * option) any later version.  See <https://www.gnu.org/licenses/lgpl-2.1.txt>.
 *
 * This library is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
 * or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Library General Public
 * License for more details.
 *
 */

#ifndef IP_ADDRESS_H
#define IP_ADDRESS_H

#include "lswcdefs.h"		/* for MUST_USE_RESULT */
#include "shunk.h"
#include "chunk.h"
#include "err.h"

struct lswlog;
struct ip_info;

extern bool log_ip; /* false -> redact (aka sanitize) ip addresses */

/*
 * Basic data types for the address-handling functions.
 *
 * ip_address et.al. are supposed to be opaque types; do not use their
 * definitions directly, they are subject to change!
 *
 * Because whack sends raw ip_addresses to pluto using a byte
 * stream, this structure needs to be stream friendly - it
 * must not contain pointers (such as a pointer to struct
 * ip_info). so instead it contains an index.
 */

typedef struct {
	/*
	 * Index into the struct ip_info array; must be stream
	 * friendly.
	 */
	unsigned version; /* 0, 4, 6 */
	/*
	 * We need something that makes static IPv4 initializers possible
	 * (struct in_addr requires htonl() which is run-time only).
	 */
	uint8_t bytes[16];
#ifndef ENDPOINT_TYPE
	/*
	 * XXX: An address abstraction - type+bytes - should not
	 * contain a port.  If a port is required, the abstraction
	 * ip_endpoint should be used.
	 *
	 * In pluto, port "0" is reserved and indicates all ports (but
	 * does it also denote no port?).  Hopefully it is only paired
	 * with the zero (any) address.
	 *
	 * XXX: Would separate and incompatible ip_hport and ip_nport
	 * types help stop host <-> network port conversion screwups?
	 * For instance, using ntohs() when using htons() is needed -
	 * while wrong they have the same effect.
	 *
	 */
	uint16_t hport;
#endif
} ip_address;

/*
 * Constructors.
 */

ip_address address_from_in_addr(const struct in_addr *in);
ip_address address_from_in6_addr(const struct in6_addr *sin6);
err_t data_to_address(const void *data, size_t sizeof_data,
		      const struct ip_info *af, ip_address *dst) MUST_USE_RESULT;
ip_address address_from_shunk(const struct ip_info *afi, const shunk_t bytes);
/* either SHUNK or CHUNK */
#define hunk_to_address(HUNK, AF, DST) data_to_address(HUNK.ptr, HUNK.len, AF, DST)

/* assumes dotted / colon notation */
err_t numeric_to_address(shunk_t src, const struct ip_info *type, ip_address *dst);
/* if numeric lookup fails, try a DNS lookup */
err_t domain_to_address(shunk_t src, const struct ip_info *type, ip_address *dst);

/*
 * Convert an address to a string:
 *
 * This implements https://tools.ietf.org/html/rfc5952 where zeros in
 * the middle of an IPv6 address are suppressed.  If the IP address is
 * "sensitive" use *_address_sensitive().
 */

typedef struct {
	char buf[(4+1)*8/*0000:...*/ + 1/*\0*/ + 1/*CANARY*/];
} address_buf;

void jam_address(struct lswlog *buf, const ip_address *src);
const char *str_address(const ip_address *src, address_buf *dst);

/*
 * sensitive: don't print address when !log_ip
 *
 * reversed: in-addr format.

 * raw: This is not the format function you are looking for. For IPv6
 * include all zeros, vis :0:..:0:; when SEPC != '\0' use it as the
 * separator instead of '.' (IPv4) or ':' (IPv6).
 */

typedef struct {
	/* string includes NUL, add 1 for canary */
	char buf[sizeof("4.0.0.0.3.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.0.2.0.0.0.1.0.0.0.IP6.ARPA.") + 1];
}  address_reversed_buf;

void jam_address_sensitive(struct lswlog *buf, const ip_address *src);
void jam_address_reversed(struct lswlog *buf, const ip_address *src);
void jam_address_raw(struct lswlog *buf, const ip_address *src, char sepc);

const char *str_address_sensitive(const ip_address *src, address_buf *dst);
const char *str_address_reversed(const ip_address *src, address_reversed_buf *buf);
const char *str_address_raw(const ip_address *src, char sepc, address_buf *dst);

typedef address_buf ipstr_buf;
const char *ipstr(const ip_address *src, ipstr_buf *b);
const char *sensitive_ipstr(const ip_address *src, ipstr_buf *b);

/*
 * Magic values.
 *
 * XXX: While the headers call the all-zero address "ANY" (INADDR_ANY,
 * IN6ADDR_ANY_INIT), the headers also refer to the IPv6 value as
 * unspecified (for instance IN6_IS_ADDR_UNSPECIFIED()) leaving the
 * term "unspecified" underspecified.
 *
 * Consequently to identify an AF_UNSPEC address (i.e.,
 * uninitialized), see if *_type() returns NULL.  There's an
 * address_is_invalid() wrapper for completeness.
 */

/* AF=AF_UNSPEC, ADDR = 0; aka all zeros */
extern const ip_address address_invalid;

/* returns NULL when address_invalid */
const struct ip_info *address_type(const ip_address *address);

/* AF={INET,INET6}, ADDR = 0; aka %any? */
ip_address address_any(const struct ip_info *info);

/* mutually exclusive */
#define address_is_invalid(A) (address_type(A) == NULL)
bool address_is_any(const ip_address *address);
bool address_is_specified(const ip_address *address);
/* implies specified */
bool address_is_loopback(const ip_address *address);

/*
 * Raw address bytes, both read-only and read-write.
 */
shunk_t address_as_shunk(const ip_address *address);
chunk_t address_as_chunk(ip_address *address);

/*
 * XXX: prop up IPv4 centric code that just isn't worth the effort.
 */
uint32_t ntohl_address(const ip_address *address);

/*
 * Old style.
 */

/* looks up names in DNS */
extern err_t ttoaddr(const char *src, size_t srclen, int af, ip_address *dst);

/* does not look up names in DNS */
extern err_t ttoaddr_num(const char *src, size_t srclen, int af, ip_address *dst);

/* RFC 1886 old IPv6 reverse-lookup format is the bulkiest */
#define ADDRTOT_BUF     sizeof(address_reversed_buf)
extern err_t tnatoaddr(const char *src, size_t srclen, int af, ip_address *dst);

/* misc. conversions and related */
extern int addrtypeof(const ip_address *src);
extern int masktocount(const ip_address *src);

/* tests */
extern bool sameaddr(const ip_address *a, const ip_address *b);
extern int addrcmp(const ip_address *a, const ip_address *b);

/* XXX: use address_is_{invalid,any,specified}() instead */
extern bool isanyaddr(const ip_address *src);

#endif
