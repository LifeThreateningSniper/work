#pragma once

#define ADVANCED_MODE 1 // 启用为开发人员定制的版本
#define DEFAULT_TEXT TEXT("每次下载固件时都会重新读取一次文件")
#define POLYNOMIAL_CRC8 0x107
#define SAMPLE_CODE TEXT("static void reset_check(void)\n") \
                    TEXT("{\n") \
                    TEXT("  int i;\n") \
                    TEXT("  uint8_t cmd[2];\n") \
                    TEXT("  HAL_StatusTypeDef status;\n") \
                    TEXT("  \n") \
                    TEXT("  if (__HAL_UART_GET_FLAG(&huart1, UART_FLAG_RXNE) != RESET)\n") \
                    TEXT("  {\n") \
                    TEXT("    status = HAL_UART_Receive(&huart1, cmd, sizeof(cmd), 200);\n") \
                    TEXT("    if (status == HAL_OK)\n") \
                    TEXT("    {\n") \
                    TEXT("      if (cmd[0] == 0xab && cmd[1] == 0xab)\n") \
                    TEXT("      {\n") \
                    TEXT("        __disable_irq();\n") \
                    TEXT("        for (i = 0; i < 2000000; i++);\n") \
                    TEXT("        \n") \
                    TEXT("        HAL_NVIC_SystemReset();\n") \
                    TEXT("      }\n") \
                    TEXT("    }\n") \
                    TEXT("  }\n") \
                    TEXT("}\n")

#define HEADER_FIRMWARE_INFO 0x32f103c8
#if ADVANCED_MODE
#define HEADER_CRC_CONFIG 0x32f030c8
#endif

#pragma pack(push, 1)
typedef struct
{
	uint32_t addr;
	uint32_t size;
	uint8_t checksum;
} DeviceResponse;

typedef struct
{
	uint32_t header;
	uint32_t size;
	uint32_t start_addr;
	uint32_t end_addr;
	uint32_t entry_point;
	uint8_t firmware_checksum; // 整个固件的校验和
	uint8_t header_checksum;
} FirmwareInfo;

#if ADVANCED_MODE
typedef struct
{
	uint32_t header;
	uint8_t crc_enabled;
	uint8_t header_checksum;
} CRCConfig;
#endif
#pragma pack(pop)
