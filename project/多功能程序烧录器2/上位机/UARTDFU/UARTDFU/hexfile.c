#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hexfile.h"

/* 释放内存 */
void HexFile_Free(HexFile *hexfile)
{
	if (hexfile->data != NULL)
	{
		free(hexfile->data);
		hexfile->data = NULL;
	}
}

/* 载入程序文件 */
int HexFile_Load(HexFile *hexfile, const char *filename)
{
	int i, j, n;
	int ret = 0, started = 0;
	int capacity = 0, size;
	uint8_t ch, *p;
	uint8_t line[50];
	uint32_t addr, base_addr;
	void *mem;
	FILE *fp;

	memset(hexfile, 0, sizeof(HexFile));
	fopen_s(&fp, filename, "r");
	if (fp == NULL)
		return -1;

	while (fgets(line, sizeof(line), fp))
	{
		// 每行必须以冒号开头
		if (line[0] != ':')
			continue;

		// 判断行长度
		p = strchr(line, '\n');
		if (p == NULL)
		{
			ret = -1; // 行长度过长
			break;
		}
		*p = '\0';

		// 读取本行所有字节
		n = (int)((p - line - 1) / 2); // 本行字节数
		if (n < 5)
		{
			ret = -1; // 每行至少5个字节
			break;
		}
		for (i = 0; i < n; i++)
		{
			for (j = 1; j <= 2; j++)
			{
				ch = line[2 * i + j];
				if (ch >= '0' && ch <= '9')
					ch -= '0';
				else if (ch >= 'A' && ch <= 'F')
					ch = ch - 'A' + 10;
				else if (ch >= 'a' && ch <= 'f')
					ch = ch - 'a' + 10;

				if (j == 1)
					line[i] = ch << 4;
				else if (j == 2)
					line[i] |= ch & 15;
			}
		}

		// 检查校验和
		ch = 0;
		for (i = 0; i < n; i++)
			ch += line[i];
		if (ch != 0)
		{
			ret = -1; // 校验不通过
			break;
		}

		// 处理数据
		switch (line[3])
		{
		case 0:
			// 程序内容
			addr = base_addr | (line[1] << 8) | line[2];
			if (!started)
			{
				started = 1;
				hexfile->start_addr = addr;
			}
			hexfile->end_addr = addr + line[0];

			size = hexfile->end_addr - hexfile->start_addr;
			if (size <= 0)
			{
				// 出现错误
				ret = -1;
				break;
			}
			else if (size > capacity)
			{
				// 扩大缓冲区
				while (size > capacity)
					capacity = capacity * 2 + 1024;

				mem = realloc(hexfile->data, capacity);
				if (mem == NULL)
				{
					ret = -1;
					break;
				}
				hexfile->data = mem;
			}

			memcpy(hexfile->data + (addr - hexfile->start_addr), line + 4, line[0]);
			break;
		case 1:
			// 程序结束
			started = 2;
			hexfile->size = hexfile->end_addr - hexfile->start_addr;
			break;
		case 4:
			// 程序地址前16位
			base_addr = (line[4] << 24) | (line[5] << 16);
			break;
		case 5:
			// 程序入口地址
			hexfile->entry_point = (line[4] << 24) | (line[5] << 16) | (line[6] << 8) | line[7];
			break;
		}
		if (ret == -1)
			break;
	}

	if (started == 0)
		ret = -1;
	if (ret != 0)
		HexFile_Free(hexfile);
	fclose(fp);
	return ret;
}
