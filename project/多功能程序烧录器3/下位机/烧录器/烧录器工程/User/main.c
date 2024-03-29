#include "system.h"
#include "SysTick.h"
#include "led.h"
#include "usart.h"
#include "oled.h"
#include "tftlcd.h"
#include "key.h"
#include "malloc.h" 
#include "sd_sdio.h"
#include "flash.h"
#include "ff.h" 
#include "fatfs_app.h"
#include "pwm.h"
#include "upgrade.h"
#include "time.h"
#include "string.h"
#include "stm32_flash.h"


#define BOOT_IS_IN_USED

#define NVIC_VectTab_FLASH           ((uint32_t)0x08000000)
#define APP1_VECTOR_START_ADDR            (0x8005000)  	// app1在flash的0x5000~0x423FF，从20KB到264KB，共计使用244KB
#define APP2_VECTOR_START_ADDR            (0x8042000)  	// app1在flash的0x42400~0x7F7FF，从264KB到510KB，共计使用246KB
#define USER_VECTOR_START_ADDR 			  (0X807F800)		// user在flash的0x7F800~0x7FFFF，共2Kb

#define HEADER_FIRMWARE_INFO 0x32f103c8
#define POLYNOMIAL_CRC8 0x107


#define READ_BUFFER_SIZE 512
#define READ_BUFFER_NUM  20
uint8_t g_FirmwareWriteBuffer[READ_BUFFER_SIZE] = {0};
uint8_t g_FirmwareReadBuffer[READ_BUFFER_SIZE] = {0};

static uint8_t uart_data[READ_BUFFER_NUM][READ_BUFFER_SIZE];
static uint16_t uart_data_len = 0;

uint8_t g_UpgradeMode = 1;

void CalcPlayFreq(int freq)
{
	int playfreq = 2;

	if (freq < 600) {
		playfreq = 32;
	} else if (freq > 9600) {
		playfreq = 1;
	} else {
		playfreq = 32 - freq / 300;
	}
	OLED_DrawPwm(playfreq);
}

void AppLogWrite(char *str)
{
	FRESULT fr;
	FIL fil;
	
	// printf("%s\r\n", str);
	fr = f_open(&fil, "logfile.txt", FA_OPEN_EXISTING | FA_WRITE);
	if (fr == FR_OK) {
		/* Seek to end of the file to append data */
		fr = f_lseek(&fil, f_size(&fil));
		if (fr != FR_OK) {
			f_close(&fil);
			return;
		}
				
  }
	f_printf(&fil, str);
	f_printf(&fil, " \n");
	
	f_close(&fil);
}

void SD_FirmwareUpgrade(uint8_t bootFlag)
{
	FRESULT ret;
	FIL fp;
	UINT bytesRead = 0;
	int offset = 0;
	uint32_t updateStartAddr;
	uint16_t nextBootFlag = 0;

    /* Opens an existing file. If not exist, creates a new file. */
    ret = f_open(&fp, "app.bin", FA_READ);
    if (ret != FR_OK) {
		printf("ret = %d\r\n", ret);
		printf("SD card: no exist app.bin. not do offline upgrade!");
		//f_close(&fp);
		return;
	}
	if (bootFlag == 2) {
		updateStartAddr = APP1_VECTOR_START_ADDR;
	} else {
		updateStartAddr = APP2_VECTOR_START_ADDR;
	}
	printf("Start offline upgrade. please wait...\r\n");
	// 循环读取文件直到文件结束
    do {
        ret = f_read(&fp, g_FirmwareWriteBuffer, READ_BUFFER_SIZE, &bytesRead);
        if (ret != FR_OK) {
			printf("Offline FW Upgrade failed: Read sdcard app.bin error.\r\n");
			f_close(&fp);
            return; // 读取出错，退出循环
        }

		printf("Upgrade addr %p start\r\n", (uint32_t *)(updateStartAddr + offset));
		STM32_FLASH_Write(updateStartAddr + offset, (u16*)g_FirmwareWriteBuffer, READ_BUFFER_SIZE/2);
		STM32_FLASH_Read(updateStartAddr + offset, (u16 *)g_FirmwareReadBuffer, READ_BUFFER_SIZE/2);
		if (memcmp(g_FirmwareWriteBuffer, g_FirmwareReadBuffer, READ_BUFFER_SIZE) != 0) {
			f_close(&fp);
			printf("Offline FW Upgrade failed: Write flash failed. offset=%d\r\n", offset);
			return;
		}
		memset(g_FirmwareWriteBuffer, 0, READ_BUFFER_SIZE);
		memset(g_FirmwareReadBuffer, 0, READ_BUFFER_SIZE);
		printf("Upgrade addr %p end\r\n", (uint32_t *)(updateStartAddr + offset));
		offset += READ_BUFFER_SIZE;
		//delay_ms(200);
    } while(bytesRead > 0); // 如果bytesRead为0，表示已经读取到文件末尾

	nextBootFlag = (bootFlag == 2) ? 1 : 2;
	printf("Offline FW Upgrade success!!. next boot from app%d\r\n", nextBootFlag);

	STM32_FLASH_Write(USER_VECTOR_START_ADDR, &nextBootFlag, 1);

	f_close(&fp);
	f_unlink("app.bin");  // 每次启动重新创建日志文件
}

void ReleaseBuffer()
{
	int i;
	for (i = 0; i < READ_BUFFER_NUM; i++) {
		memset(uart_data[i], 0, READ_BUFFER_SIZE);
	}
	uart_data_len = 0 ;
}

// 将SD卡中的升级文件读到uart_data中
int GetSdcardUpdateFile(FIL *fp)
{
	FRESULT ret;
	UINT bytesRead = 0;
	UINT pos = 0;

	// 循环读取文件直到文件结束
    do {
        ret = f_read(fp, uart_data[pos], READ_BUFFER_SIZE, &bytesRead);
        if (ret != FR_OK) {
			printf("f_read error pos=%d.\r\n", pos);
            return -1; // 读取出错，退出循环
        }
		uart_data_len += bytesRead;
		pos++;
    } while(bytesRead > 0); // 如果bytesRead为0，表示已经读取到文件末尾

	return 0;
}

void SdCardSendHandInfo()
{
	int i;
	uint16_t rxData = 0;
	// 关闭usart3接收中断
	USART_ITConfig(USART3, USART_IT_RXNE, DISABLE);
	//USART_GetITStatus(USART3, USART_IT_RXNE);
	while (1) {
		for (i = 0; i < 16; i++) {
			USART_SendData(USART3, 0xab);
			while(USART_GetFlagStatus(USART3,USART_FLAG_TC) != SET);
		}
		delay_ms(50);
		if(USART_GetFlagStatus(USART3, USART_IT_RXNE) != RESET) {
			rxData = USART_ReceiveData(USART3);
			if (rxData == 0xcd) {
				printf("hand mcu success\r\n");
				break;
			}
		}
		printf("wait mcu hand ack\r\n");
	}			
}

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

static uint8_t calc_crc8(const void *data, int len)
{
	const uint8_t *p = data;
	int i, j;
	uint16_t temp = 0;

	if (len != 0)
		temp = p[0] << 8;

	for (i = 1; i <= len; i++)
	{
		if (i != len)
			temp |= p[i];
		for (j = 0; j < 8; j++)
		{
			if (temp & 0x8000)
				temp ^= POLYNOMIAL_CRC8 << 7;
			temp <<= 1;
		}
	}
	return temp >> 8;
}

typedef __packed struct
{
  uint32_t addr;
  uint32_t size;
  uint8_t checksum;
} DeviceResponse;

void SdCardReciveDataCrc(DeviceResponse *resp)
{
	uint8_t buf[9] = 0;
	uint8_t cnt = 0;
	uint8_t pos = 0;
	while (cnt < 9) {
		if(USART_GetFlagStatus(USART3, USART_IT_RXNE) != RESET) {
			buf[pos] = USART_ReceiveData(USART3);
			pos++;
			cnt++;
			//printf("cnt=%d, rep=%d\r\n", cnt, sizeof(DeviceResponse));
		}
		
	}
	memcpy(resp, buf, sizeof(DeviceResponse));
}

void SdCardSendFirmWare()
{
	FirmwareInfo info;
	uint8_t *p = &uart_data[0][0];
	uint8_t *tmp = NULL;
	uint16_t j;
	DeviceResponse rep = {0};

	// 先发送固件信息
	info.header = HEADER_FIRMWARE_INFO;
	info.size = uart_data_len;
	info.start_addr = APP1_VECTOR_START_ADDR;
	info.end_addr = APP1_VECTOR_START_ADDR + uart_data_len;
	info.entry_point = APP1_VECTOR_START_ADDR + 0x131;
	info.firmware_checksum = calc_crc8(p, uart_data_len);
	info.header_checksum = 0;
	tmp = (uint8_t *)&info;
	for (j =0; j < sizeof(info);j++) {
		USART_SendData(USART3, *(tmp + j));
		while(USART_GetFlagStatus(USART3,USART_FLAG_TC) != SET);
	}
	OLED_ShowString(2, 1, "send bin .....  ");
	delay_ms(10000);
	p = &uart_data[0][0];
	while (1) {
		SdCardReciveDataCrc(&rep);
		// printf("addr=0x%x, size=%d, checksum=0x%x\r\n", rep.addr,
  					// rep.size, rep.checksum);
		if (rep.size == 0) {
			break;
		} else if (rep.addr == 0xffffffff && rep.size == 0xffffffff) {
			continue;
		}
		for (j = 0; j < rep.size; j++) {
			//printf("%x ", *(p + j));
			USART_SendData(USART3, *(p + j));
			while(USART_GetFlagStatus(USART3,USART_FLAG_TC) != SET);
		}
		p += rep.size;
		delay_ms(100);
		USART_SendData(USART3, 0);
		while(USART_GetFlagStatus(USART3,USART_FLAG_TC) != SET);
	}
	

}

int main()
{
	u32 total, free;
	u8 val;
	u8 key = 0;
	uint32_t oldfreq = 0;
	uint32_t curfreq = 0;
	char str[32] = {0};
	char sd_buff[32] = {0};
	char buff[128] = {0};
	FRESULT fpret = FR_INVALID_PARAMETER;
	FIL file;
	int ret = -1;

	// 2. 初始化系统配置
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //中断优先级分组 分2组
	LED_Init();
	KEY_Init();
	USART1_Init(115200);
	USART2_Init(115200);
	USART3_Init(115200);
	TIM4_Init(10000,72000-1);  //定时1s
	OLED_Init();
	OLED_Clear(); 
	TIM3_IC_Init();	
	my_mem_init(SRAMIN);		//初始化内部内存池
	while(SD_Init()) { //检测不到SD卡
		printf("SD Card Error!\r\n");
		OLED_ShowString(2, 1, "SD err..");
		delay_ms(500);				
	}	
	
	// 文件系统初始化
	FATFS_Init();				//为fatfs相关变量申请内存	
  	f_mount(fs[0],"0:",1); 		//挂载SD卡
	while(fatfs_getfree("0:",&total,&free))	{ //得到SD卡的总容量和剩余容量
		delay_ms(200);
		LED2=!LED2;
	}
	f_unlink("logfile.txt");  // 每次启动重新创建日志文件
	delay_ms(1500);

	oldfreq = Get_TIM3_CH1_Freq();
	sprintf(str, "freq: %dHZ", oldfreq);
	OLED_ShowString(3, 1, str);
	CalcPlayFreq(oldfreq);
	OLED_Refresh_Gram();
	// AppLogWrite("In  While");
	while(1)
	{
		curfreq = Get_TIM3_CH1_Freq();
		if (curfreq != oldfreq) {
			//OLED_Clear();
			OLED_DrawClear();
			CalcPlayFreq(oldfreq);
			OLED_Refresh_Gram(); // 定时刷新屏幕
			sprintf(str, "freq: %dHZ", curfreq);
			OLED_ShowString(3, 1, str);
			//AppLogWrite(str);

			oldfreq = curfreq;
		}

		key = KEY_Scan(0);
		switch (key) {
			case KEY_UP_PRESS:
				g_UpgradeMode = 1;
				// AppLogWrite("update mode is wireless");
				break;
			case KEY0_PRESS:
				g_UpgradeMode = 2;
				// AppLogWrite("update mode is wire");
				break;
			case KEY1_PRESS:
				g_UpgradeMode =3;
				// AppLogWrite("update mode is sdcard");
				break;
			default:
				break;
		}
		if (g_UpgradeMode == 1) {
			OLED_ShowString(1, 1, "Wireless upgrade");
		} else if (g_UpgradeMode == 2) {
			OLED_ShowString(1, 1, "Wired    upgrade");
		} else if (g_UpgradeMode == 3) {
			OLED_ShowString(1, 1, "SDcard   upgrade");
		}
		OLED_ShowString(2, 1, sd_buff);

		if (g_UpgradeMode == 3) {
			// 1. 若按键选择SD卡升级，判断sd卡中是否存在升级文件
			printf("prepare sdcard upgrade\r\n");
			OLED_ShowString(2, 1, "Start Sdcard upgrade");
			if (fpret = f_open(&file, "update.bin", FA_READ) != FR_OK) {
				// 2. 若SD卡中无升级文件，则继续主循环
				OLED_ShowString(2, 1, "sd no updatefile");
				printf("sd no update file\r\n");
				delay_ms(2000);
				g_UpgradeMode = 1;
			} else {
				// 3. 若SD卡中有升级文件，则开始读取文件内容到uart_data中
				printf("Read sdcard update file\r\n");
				ret = GetSdcardUpdateFile(&file);
				printf("file data len=%d\r\n", uart_data_len);
				// 4. 等待目标机复位，并发送握手信号.同时关闭uart3中断，阻塞接收
				OLED_ShowString(2, 1, "Please reset mcu");
				printf("Please reset mcu\r\n");
				SdCardSendHandInfo();  // 死等对方回复ack
				OLED_ShowString(2, 1, "sd update hand  ");
				// 5. 告诉目标机此次升级的固件信息，和固件
				SdCardSendFirmWare();
				sprintf(sd_buff, "%s", "sd update finish");
				printf("update mcu secuss\r\n");
				// 6. 清空缓冲区
				ReleaseBuffer();
				USART_ITConfig(USART3, USART_IT_RXNE, ENABLE);
				g_UpgradeMode = 1;
			}
			f_close(&file);
		}
		
		delay_ms(100);
		//OLED_Clear();
	}
}
