#pragma once
#include <stdint.h>
typedef struct {uint32_t ack,bit,form,stuff,other;} md_can_error_counts_t;
void md_can_error_counts(md_can_error_counts_t *out);
