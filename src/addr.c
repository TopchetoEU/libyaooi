// libyaooi, Copyright (C) 2025-2026 topchetoeu, see LICENSE for full LGPL text

#pragma once

#include <ctype.h>
#include <string.h>

#include <yaooi/addr.h> // IWYU pragma: export

static bool yo_parse_ipv4(const char *str, yo_addr_t *pres) {
	yo_addr_t res;
	res.type = YO_ADDR_IPV4;

	const char *it = str;

	for (int i = 0; i < 4; i++) {
		uint64_t part = 0;

		if (!isdigit(*it)) return false;

		while (isdigit(*it)) {
			if (part > 100) return false;
			part = part * 10 + *it - '0';
			it++;
		}

		if (part > 255) return false;

		if (*it == '.') {
			if (i == 3) return false;
			it++;
		}
		if (*it == '\0' && i != 3) return false;

		res.v4[i] = part;
	}

	if (*it != '\0') return false;

	if (pres) *pres = res;
	return true;
}
static bool yo_parse_ipv6(const char *str, yo_addr_t *pres) {
	yo_addr_t res = { 0 };
	res.type = YO_ADDR_IPV6;

	const char *it = str;
	int zeroes_i = -1;
	int i = 0;

	if (it[0] == ':' && it[1] == ':') {
		it += 2;
		zeroes_i = 0;

		if (*it == '\0') {
			*pres = res;
			return true;
		}
	}

	for (i = 0; i < 8; i++) {
		if (!isxdigit(*it)) return false;

		for (int j = 0; j < 4; j++) {
			if (!isxdigit(*it)) break;

			res.v6[i] <<= 4;
			if (isdigit(*it)) res.v6[i] |= *it - '0';
			if (islower(*it)) res.v6[i] |= *it - 'a' + 10;
			if (isupper(*it)) res.v6[i] |= *it - 'A' + 10;
			it++;
		}

		if (*it == ':') {
			it++;
			continue;
		}

		if (it[0] == ':' && it[1] == ':') {
			if (zeroes_i != -1) return false;
			zeroes_i = i;
			it += 2;
		}

		if (*it == '\0') break;
	}

	if (*it != '\0') return false;

	if (zeroes_i > 0) {
		int trailing_n = i - zeroes_i;
		memmove(res.v6 + (16 - trailing_n), res.v6 + zeroes_i, sizeof *res.v6 * trailing_n);
	}

	if (pres) *pres = res;
	return true;
}

bool yo_addrparse(const char *str, yo_addr_t *pres) {
	if (yo_parse_ipv4(str, pres)) return true;
	if (yo_parse_ipv6(str, pres)) return true;
	return false;
}
bool yo_addrcmp(yo_addr_t a, yo_addr_t b) {
	if (a.type != b.type) return false;
	if (a.type == YO_ADDR_IPV4) {
		return !memcmp(a.v4, b.v4, sizeof a.v4);
	}
	else {
		return !memcmp(a.v6, b.v6, sizeof a.v6);
	}
}
