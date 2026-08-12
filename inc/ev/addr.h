#ifndef EV_ADDR_H
#define EV_ADDR_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

typedef enum {
	EV_ADDR_IPV4,
	EV_ADDR_IPV6,
	// TODO: bluetooth maybe?
} ev_addr_type_t;
typedef struct {
	ev_addr_type_t type;
	union {
		uint8_t v4[4];
		uint16_t v6[8];
	};
} ev_addr_t;

// Parses the string to an IP address (ipv4/6 auto-detected)
bool ev_addrparse(const char *str, ev_addr_t *pres);
// Returns true if both addresses are equal
bool ev_addrcmp(ev_addr_t a, ev_addr_t b);

#endif
