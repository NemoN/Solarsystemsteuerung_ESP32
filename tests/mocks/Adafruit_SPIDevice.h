#pragma once

#include <cassert>
#include <stddef.h>
#include <stdint.h>

class Adafruit_SPIDevice {
public:
  uint8_t configuration[3] = {0x00, 0x80, 0x00};
  bool readable = true;
  unsigned reads = 0;

  bool write_then_read(const uint8_t* command, size_t commandSize,
                      uint8_t* result, size_t resultSize) {
    assert(commandSize == 1 && command[0] == 0x00 && resultSize == 1);
    *result = configuration[reads++ % 3];
    return readable;
  }
};
