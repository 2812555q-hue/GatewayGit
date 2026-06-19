#ifndef _CRC8_H_
#define _CRC8_H_

#include <stdint.h>
#include <stddef.h>

uint8_t crc8_compute(const uint8_t *data, size_t len);

#endif
