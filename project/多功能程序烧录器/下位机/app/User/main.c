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
#include "string.h"
#include "stm32_flash.h"

#define BOOT_IS_IN_USED

#define NVIC_VectTab_FLASH           ((uint32_t)0x08000000)
#define APP1_VECTOR_START_ADDR            (0x8005000)  	// app1��flash��0x5000~0x423FF����20KB��264KB������ʹ��244KB
#define APP2_VECTOR_START_ADDR            (0x8042000)  	// app1��flash��0x42400~0x7F7FF����264KB��510KB������ʹ��246KB
#define USER_VECTOR_START_ADDR 			  (0X807F800)		// user��flash��0x7F800~0x7FFFF����2Kb

#define READ_BUFFER_SIZE 512
typedef void (*Runnable)(void);
uint8_t g_FirmwareWriteBuffer[READ_BUFFER_SIZE] = {0};
uint8_t g_FirmwareReadBuffer[READ_BUFFER_SIZE] = {0};

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
	
	printf("%s\r\n", str);
	fr = f_open(&fil, "logfile.txt", FA_WRITE | FA_OPEN_ALWAYS);
	if (fr == FR_OK) {
		/* Seek to end of the file to append data */
		fr = f_lseek(&fil, f_size(&fil));
		if (fr != FR_OK)
				f_close(&fil);
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
	// ѭ����ȡ�ļ�ֱ���ļ�����
    do {
        ret = f_read(&fp, g_FirmwareWriteBuffer, READ_BUFFER_SIZE, &bytesRead);
        if (ret != FR_OK) {
			printf("Offline FW Upgrade failed: Read sdcard app.bin error.\r\n");
			f_close(&fp);
            return; // ��ȡ�����˳�ѭ��
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
    } while(bytesRead > 0); // ���bytesReadΪ0����ʾ�Ѿ���ȡ���ļ�ĩβ

	nextBootFlag = (bootFlag == 2) ? 1 : 2;
	printf("Offline FW Upgrade success!!. next boot from app%d\r\n", nextBootFlag);

	STM32_FLASH_Write(USER_VECTOR_START_ADDR, &nextBootFlag, 1);

	f_close(&fp);
	f_unlink("app.bin");  // ÿ���������´�����־�ļ�
}

int main()
{
	u32 total, free;
	uint32_t oldfreq = 0;
	uint32_t curfreq = 0;
	char str[32] = {0};
	char buff[128] = {0};
	uint32_t bootAddr;
	uint8_t *bootFlag = NULL;

	// 1. ������ƫ��������
	bootFlag = (uint8_t *)USER_VECTOR_START_ADDR;
	if (*bootFlag == 2) {
		bootAddr = APP2_VECTOR_START_ADDR;
	} else { // Ĭ�ϴ�app1������������¼��������¼�̶���app1������
		bootAddr = APP1_VECTOR_START_ADDR;
	}

	SCB->VTOR = NVIC_VectTab_FLASH | (bootAddr & (uint32_t)0x1FFFFF80);
	__enable_irq();	//ʹ���ж�

	// 2. ��ʼ��ϵͳ����
	SysTick_Init(72);
	NVIC_PriorityGroupConfig(NVIC_PriorityGroup_2);  //�ж����ȼ����� ��2��
	LED_Init();
	USART1_Init(115200);
	printf("Start Run App%d...\r\n", (*bootFlag == 2) ? 2 : 1);

	// 3. ���������ʼ��
	OLED_Init();
	OLED_Clear(); 
	OLED_ShowString(1, 1, "Start SD check..");
	TIM3_IC_Init();	
	my_mem_init(SRAMIN);		//��ʼ���ڲ��ڴ��
	while(SD_Init()) { //��ⲻ��SD��
		printf("SD Card Error!\r\n");
		OLED_ShowString(2, 1, "SD err..");
		delay_ms(500);				
	}		
	
	// 4. �ļ�ϵͳ��ʼ��
	FATFS_Init();				//Ϊfatfs��ر��������ڴ�	
  	f_mount(fs[0],"0:",1); 		//����SD��
	while(fatfs_getfree("0:",&total,&free))	{ //�õ�SD������������ʣ������
		delay_ms(200);
		LED2=!LED2;
	}
	
	sprintf(buff, "boot APP%d. version:1.01", (*bootFlag == 2) ? 2 : 1);
	AppLogWrite(buff);
	memset(buff, 0, sizeof(buff));
	
	OLED_ShowString(2, 1, buff);
	f_unlink("logfile.txt");  // ÿ���������´�����־�ļ�
	OLED_ShowString(3, 1, "SD OK..");
	AppLogWrite("SD Card OK!");
	sprintf(buff, "SD Card Total Size: %dMB, Free Size: %dMB!", total, free);
	AppLogWrite(buff);
	memset(buff, 0, sizeof(buff));
	
	delay_ms(1500);
	// �ѻ���¼
	SD_FirmwareUpgrade(*bootFlag);   
	
	// ��ʼ�ɼ�PWM��Ƶ��
	OLED_ShowString(4, 1, "PWM check..");
	AppLogWrite("Start PWM check");
	
	delay_ms(1500);
	OLED_Clear();
	oldfreq = Get_TIM3_CH1_Freq();
	CalcPlayFreq(oldfreq);
	OLED_Refresh_Gram();
	AppLogWrite("Enter in while....");
	while(1)
	{
		curfreq = Get_TIM3_CH1_Freq();
		if (curfreq != oldfreq) {
			OLED_Clear();
			OLED_DrawClear();
			CalcPlayFreq(oldfreq);
			OLED_Refresh_Gram(); // ��ʱˢ����Ļ

			sprintf(str, "freq: %dHZ", curfreq);
			OLED_ShowString(1, 2, str);

			sprintf(buff, "pwm change freq:%d, duty:%d\r\n", Get_TIM3_CH1_Freq(), Get_TIM3_Duty());
			AppLogWrite(buff);
			memset(buff, 0, sizeof(buff));

			oldfreq = curfreq;
		}

		delay_ms(500);
	}
}
