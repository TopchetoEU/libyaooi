// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#ifndef YO_ADDR_H
#define YO_ADDR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	YO_ADDR_IPV4,
	YO_ADDR_IPV6,
	// TODO: bluetooth maybe?
} yo_addr_type_t;
typedef struct {
	yo_addr_type_t type;
	union {
		uint8_t v4[4];
		uint16_t v6[8];
	};
} yo_addr_t;

// Parses the string to an IP address (ipv4/6 auto-detected)
bool yo_addrparse(const char *str, yo_addr_t *pres);
// Returns true if both addresses are equal
bool yo_addrcmp(yo_addr_t a, yo_addr_t b);

#endif
