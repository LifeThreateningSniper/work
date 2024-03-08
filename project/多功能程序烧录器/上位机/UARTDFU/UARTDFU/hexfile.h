#pragma once

typedef struct
{
	uint8_t *data;
	uint32_t size;
	uint32_t start_addr;
	uint32_t end_addr;
	uint32_t entry_point;
} HexFile;

void HexFile_Free(HexFile *hexfile);
int HexFile_Load(HexFile *hexfile, const char *filename);
