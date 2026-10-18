#pragma once
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>
/* Bounded DNS A/IN response; malformed/compressed questions are rejected. */
size_t md_captive_dns_reply(uint8_t *packet, size_t length, size_t capacity);
#ifdef ESP_PLATFORM
int md_captive_dns_open(void);
void md_captive_dns_poll(int socket_fd);
void md_captive_dns_close(int socket_fd);
bool md_captive_dns_selftest(void);
#endif
