#ifndef _DFU_H
#define _DFU_H

#define HEADER_CRC_CONFIG 0x32f030c8
#define HEADER_FIRMWARE_INFO 0x32f103c8

#define POLYNOMIAL_CRC8 0x107

typedef void (*Runnable)(void);

typedef __packed struct
{
  uint32_t addr;
  uint32_t size;
  uint8_t checksum;
} DeviceResponse;

typedef __packed struct
{
  uint32_t header;
  uint8_t crc_enabled;
  uint8_t header_checksum;
} CRCConfig;

typedef __packed struct
{
  uint32_t header;
  uint32_t size;
  uint32_t start_addr;
  uint32_t end_addr;
  uint32_t entry_point;
  uint8_t firmware_checksum; // 整个固件的校验和
  uint8_t header_checksum;
} FirmwareInfo;

#endif
