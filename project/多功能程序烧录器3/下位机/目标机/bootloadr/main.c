// ���������ڲ�����(HSI)��Ϊϵͳʱ��, û���õ��ⲿ����(HSE), ���Բ���Ҫָ��HSE_VALUE��ֵ
// ��������ѡ��-O3�Ż�����Ҫ��ST-Link���Ա�����, ���ѡ-O0�Ż�
#include <stdio.h>
#include <stm32f1xx.h>
#include <string.h>
#include "common.h"
#include "DFU.h"

// ����Ҫֱ����Keil�����ز���ST-Link�����ӳ���, ����Ҫ����CRCУ��
#define START_ADDR 0x08004800 // һ��Ҫ��֤�����ַ����DFU����ĵ�ַ��Χ��, �������Flash��ֱ��Hard Error
                              // ��������Ŀ����������IROM1��Size, ����������Ĵ�С, ��ֹ���������
                              // ��������̫��, ������C/C++ѡ��п���-O3�Ż���С�������С
#define RESERVED_SIZE 0x800 // ��ο�system_stm32xxx.c��������VECT_TAB_OFFSET�Ķ��뷽ʽ
                             // ����������һ�仰: Vector Table base offset field. This value must be a multiple of 0xXXX.
                             // ���뱣֤����RESERVED_SIZE��ֵ�ܱ�0xXXX����, ������START_ADDR���ڵ�Flashҳ��, RESERVED_SIZE��ռ����ҳ

#define HARDWARE_CRC 0 // ����Ӳ��CRCУ��
#define PROGRAM_ENABLE 1 // �Ƿ�������дFlash (������)
#define PRINTF_ENABLE 1 // �Ƿ����������
#define UART_FIFOCNT 35 // ���ݻ���������
#define UART_MTU 512 // ÿ���������Ĵ�С (����Ϊ4�ı���)

#if UART_MTU % 4 != 0
#error "UART_MTU must be a multiple of 4"
#endif

static int dfu_process_crc_config(const CRCConfig *config);
static int dfu_process_firmware_download(void);
static int dfu_start_transfer(void);

DMA_HandleTypeDef hdma12, hdma13;
DMA_HandleTypeDef hdma17, hdma16;
IWDG_HandleTypeDef hiwdg;
// �����⼸����Ŀǰ���������򵽵�оƬ, ����������F101��F102��F105ϵ���Ѿ�ͣ��, �����򲻵���
// {-ҳ��, -ҳ��С}
// #if defined(STM32F103x6) || defined(STM32F103xB)
  // low-density devices, medium-density devices
static const int16_t flash_pages[] = {-128, -1};
// #elif defined(STM32F103xE) || defined(STM32F103xG) || defined(STM32F107xC)
//   // high-density devices, XL-density devices, connectivity line devices
// static const int16_t flash_pages[] = {-512, -2};
// #endif
static const uint32_t crc_disable __attribute((at(START_ADDR + 4))) = 0x54545454; // Ĭ�Ͻ���CRC, ����ST-Link������������������������ӳ���
static uint8_t uart_data[UART_FIFOCNT][UART_MTU];
static volatile uint8_t uart_front, uart_rear, uart_busy;
static volatile uint32_t uart_frontaddr, uart_rearaddr;
static DeviceResponse device_response;
static FirmwareInfo firmware_info;

#if HARDWARE_CRC
// ע��: �������е�STM32��Ƭ������ʹ���Զ���CRCУ�����ʽ
CRC_HandleTypeDef hcrc;

static uint8_t calc_crc8(const void *data, int len)
{
  assert_param(HAL_CRC_GetState(&hcrc) == HAL_CRC_STATE_READY);
  return (uint8_t)HAL_CRC_Calculate(&hcrc, (uint32_t *)data, len);
}

static void crc_init(void)
{
  __HAL_RCC_CRC_CLK_ENABLE();
  
  hcrc.Instance = CRC;
  hcrc.Init.CRCLength = CRC_POLYLENGTH_8B;
  hcrc.Init.DefaultInitValueUse = DEFAULT_INIT_VALUE_DISABLE;
  hcrc.Init.DefaultPolynomialUse = DEFAULT_POLYNOMIAL_DISABLE;
  hcrc.Init.GeneratingPolynomial = POLYNOMIAL_CRC8 & 0xff;
  hcrc.Init.InitValue = 0;
  hcrc.Init.InputDataInversionMode = CRC_INPUTDATA_INVERSION_NONE;
  hcrc.Init.OutputDataInversionMode = CRC_OUTPUTDATA_INVERSION_DISABLE;
  hcrc.InputDataFormat = CRC_INPUTDATA_FORMAT_BYTES;
  HAL_CRC_Init(&hcrc);
}
#else
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

#define crc_init()
#endif

// �����������ô����շ����õ�DMAͨ��
// ע���ļ������DMA�жϵĺ�����ҲҪ��
static void dfu_dma_init(void)
{
#if 0
  if (hdma12.Instance != NULL)
    return;
  
  __HAL_RCC_DMA1_CLK_ENABLE();
  
  hdma12.Instance = DMA1_Channel2;
  hdma12.Init.Direction = DMA_MEMORY_TO_PERIPH; // ע�ⷽ��Ҫд����(1)
  hdma12.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma12.Init.MemInc = DMA_MINC_ENABLE;
  hdma12.Init.Mode = DMA_NORMAL;
  hdma12.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma12.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma12.Init.Priority = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&hdma12);
  __HAL_LINKDMA(&huart3, hdmatx, hdma12); // ע�ⷽ��Ҫд����(2)
  
  hdma13.Instance = DMA1_Channel3;
  hdma13.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma13.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma13.Init.MemInc = DMA_MINC_ENABLE;
  hdma13.Init.Mode = DMA_NORMAL;
  hdma13.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma13.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma13.Init.Priority = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&hdma13);
  __HAL_LINKDMA(&huart3, hdmarx, hdma13);
#endif
  if (hdma17.Instance != NULL)
  return;
  
  __HAL_RCC_DMA1_CLK_ENABLE();
  
  hdma17.Instance = DMA1_Channel7;
  hdma17.Init.Direction = DMA_MEMORY_TO_PERIPH; // ע�ⷽ��Ҫд����(1)
  hdma17.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma17.Init.MemInc = DMA_MINC_ENABLE;
  hdma17.Init.Mode = DMA_NORMAL;
  hdma17.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma17.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma17.Init.Priority = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&hdma17);
  __HAL_LINKDMA(&huart2, hdmatx, hdma17); // ע�ⷽ��Ҫд����(2)
  
  hdma16.Instance = DMA1_Channel6;
  hdma16.Init.Direction = DMA_PERIPH_TO_MEMORY;
  hdma16.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma16.Init.MemInc = DMA_MINC_ENABLE;
  hdma16.Init.Mode = DMA_NORMAL;
  hdma16.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma16.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma16.Init.Priority = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&hdma16);
  __HAL_LINKDMA(&huart2, hdmarx, hdma16);
}

static int dfu_process(void)
{
  int header_valid = 0;
  int i, j, len;
  uint8_t sync[17];
  uint32_t header;
  CRCConfig crccfg;
  printf("Enter DFU mode 111!\n");
  // ���ͳɹ�����DFUģʽ�Ļ�Ӧ
  memset(sync, 0xcd, sizeof(sync));
  //HAL_UART_Transmit(&huart3, sync, sizeof(sync), HAL_MAX_DELAY);
  HAL_UART_Transmit(&huart2, sync, sizeof(sync), HAL_MAX_DELAY);
  printf("Enter DFU mode!\n");
  
  // ����������Ϣ
  i = 0;
  do
  {
    // ���������0xabͬ���ź�, Ѱ��ͷ���ֶ�
    memset(uart_data[0], 0, UART_MTU);
    //HAL_UART_Receive(&huart3, uart_data[0], UART_MTU, 10000);
    HAL_UART_Receive(&huart2, uart_data[0], UART_MTU, 10000);
    for (j = 0; j < UART_MTU; j++)
    {
      if (uart_data[0][j] != 0xab)
        break;
    }
    printf("bengin find header\n");
    
    len = UART_MTU - j;
    if (len >= 4)
    {
      memcpy(&header, &uart_data[0][j], 4);
      switch (header)
      {
        case HEADER_CRC_CONFIG:
          // CRCУ������
          printf("header=HEADER_CRC_CONFIG\n");
          if (len >= sizeof(CRCConfig))
          {
            memcpy(&crccfg, &uart_data[0][j], sizeof(CRCConfig));
            if (calc_crc8(&crccfg, sizeof(CRCConfig)) == 0)
              header_valid = 1;
          }
          break;
        case HEADER_FIRMWARE_INFO:
          // �̼���Ϣͷ
          printf("header=HEADER_FIRMWARE_INFO\n");
          if (len >= sizeof(FirmwareInfo))
          {
            memcpy(&firmware_info, &uart_data[0][j], sizeof(FirmwareInfo));
            if (calc_crc8(&firmware_info, sizeof(FirmwareInfo)) == 0)
              header_valid = 1;
          }
          break;
        default:
          printf("Invalid header: 0x%x\n", header);
          break;
      }
      printf("header_valid=%d\n", header_valid);
    }
    
    if (!header_valid)
    {
      // �����ش�ͷ��
      printf("Failed to receive header!\n");
      i++;
      if (i == 5)
        return -1;
      
      device_response.addr = 0xffffffff;
      device_response.size = 0xffffffff;
      device_response.checksum = calc_crc8(&device_response, sizeof(DeviceResponse) - 1);
      //HAL_UART_Transmit(&huart3, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
      HAL_UART_Transmit(&huart2, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    }
  } while (!header_valid);
  
  // ��������
  switch (header)
  {
    case HEADER_CRC_CONFIG:
      return dfu_process_crc_config(&crccfg);
    case HEADER_FIRMWARE_INFO:
      return dfu_process_firmware_download();
    default:
      return -1;
  }
}

/* ����CRCУ���������� */
static int dfu_process_crc_config(const CRCConfig *config)
{
  int i;
  uint8_t buffer[8];
  uint8_t *value = (uint8_t *)START_ADDR + 5;
  uint32_t addr;
#if PROGRAM_ENABLE
  uint32_t err;
  FLASH_EraseInitTypeDef erase;
#endif
  
  memcpy(buffer, (void *)START_ADDR, sizeof(buffer));
  buffer[5] = (config->crc_enabled) ? 0x00 : 0x54;
  
  if (*value != buffer[5])
  {
    addr = FLASH_BASE;
    for (i = 0; i < -flash_pages[0]; i++)
    {
      addr -= flash_pages[1] * 1024;
      if (addr > START_ADDR)
        break;
    }
    
#if PROGRAM_ENABLE
    HAL_FLASH_Unlock();
#ifdef FLASH_BANK_BOTH
    erase.Banks = FLASH_BANK_BOTH;
#else
    erase.Banks = FLASH_BANK_1;
#endif
    erase.TypeErase = FLASH_TYPEERASE_PAGES;
    erase.PageAddress = FLASH_BASE - i * flash_pages[1] * 1024;
    erase.NbPages = 1;
    HAL_FLASHEx_Erase(&erase, &err);
  
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, START_ADDR, __UNALIGNED_UINT32_READ(buffer));
    HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, START_ADDR + 4, __UNALIGNED_UINT32_READ(buffer + 4));
    HAL_FLASH_Lock();
#endif
  }
  
  device_response.addr = (uintptr_t)value;
  device_response.size = (memcmp((void *)START_ADDR, buffer, sizeof(buffer)) == 0);
  device_response.checksum = calc_crc8(&device_response, sizeof(DeviceResponse) - 1);
  // HAL_UART_Transmit(&huart3, (uint8_t *)&device_response, sizeof(device_response), HAL_MAX_DELAY);
  HAL_UART_Transmit(&huart2, (uint8_t *)&device_response, sizeof(device_response), HAL_MAX_DELAY);
  return 0;
}

/* ����̼��������� */
static int dfu_process_firmware_download(void)
{
  int ret;
  uint32_t err, ticks;
  uint32_t maxsize, size;
  HAL_StatusTypeDef status;
#if PROGRAM_ENABLE
  int i;
  uint32_t addr[3], start_page;
  FLASH_EraseInitTypeDef erase;
#endif
  
  maxsize = (FLASH_BASE + FLASH_SIZE) - (START_ADDR + RESERVED_SIZE);
  printf("Available flash: %u\n", maxsize);
  printf("Size: %u\n", firmware_info.size);
  printf("Address: 0x%08x - 0x%08x\n", firmware_info.start_addr, firmware_info.end_addr - 1);
  printf("Entry Point: 0x%08x\n", firmware_info.entry_point);
  printf("Checksum: 0x%02x\n", firmware_info.firmware_checksum);
  if (firmware_info.size > maxsize)
  {
    // Flash��������
    device_response.addr = 0;
    device_response.size = maxsize;
    device_response.checksum = calc_crc8(&device_response, sizeof(DeviceResponse) - 1);
    //HAL_UART_Transmit(&huart3, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    return -1;
  }
  else if (firmware_info.start_addr != START_ADDR + RESERVED_SIZE)
  {
    // �������ʼ��ַ����ȷ
    printf("Invalid firmware address!\n");
    device_response.addr = START_ADDR + RESERVED_SIZE;
    device_response.size = 0;
    device_response.checksum = calc_crc8(&device_response, sizeof(DeviceResponse) - 1);
    // HAL_UART_Transmit(&huart3, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    return -1;
  }
  else if (firmware_info.start_addr + firmware_info.size != firmware_info.end_addr)
  {
    printf("Incorrect firmware size!\n");
    return -1;
  }
  
  // �����ж�, ���н�������
  // HAL_NVIC_EnableIRQ(DMA1_Channel2_IRQn);
  // HAL_NVIC_EnableIRQ(DMA1_Channel3_IRQn);
  // HAL_NVIC_EnableIRQ(USART3_IRQn);
  HAL_NVIC_EnableIRQ(DMA1_Channel7_IRQn);
  HAL_NVIC_EnableIRQ(DMA1_Channel6_IRQn);
  HAL_NVIC_EnableIRQ(USART2_IRQn);
  dfu_dma_init();
  uart_front = 0;
  uart_frontaddr = START_ADDR + RESERVED_SIZE;
  uart_rear = 0;
  uart_rearaddr = uart_frontaddr;
  dfu_start_transfer();

#if PROGRAM_ENABLE
  // ������Ҫ��Flashҳ
  HAL_FLASH_Unlock();
  
  addr[0] = FLASH_BASE;
  addr[2] = firmware_info.end_addr - 1; // �ӳ��������ַ
  memset(&erase, 0xff, sizeof(FLASH_EraseInitTypeDef));
  for (i = 0; i < -flash_pages[0]; i++)
  {
    // iΪҳ��, addr[0]Ϊҳi����ʼ��ַ, addr[1]Ϊҳi�Ľ�����ַ
    addr[1] = addr[0] - flash_pages[1] * 1024 - 1;
    if (START_ADDR >= addr[0] && START_ADDR <= addr[1])
    {
      start_page = i;
      erase.PageAddress = addr[0];
    }
    if (addr[2] >= addr[0] && addr[2] <= addr[1])
    {
      erase.NbPages = i - start_page + 10;
      break;
    }
    addr[0] = addr[1] + 1;
  }
#ifdef FLASH_BANK_BOTH
    erase.Banks = FLASH_BANK_BOTH;
#else
    erase.Banks = FLASH_BANK_1;
#endif
  erase.TypeErase = FLASH_TYPEERASE_PAGES;
  printf("Erase page %d~%d\n", start_page, start_page + erase.NbPages - 1);
  for (i = 0; i < 40; i++) {
    erase.PageAddress = uart_frontaddr + (i * 1024);
    erase.NbPages = 1;
    ret = HAL_FLASHEx_Erase(&erase, &err);
    printf("HAL_FLASHEx_Erase addr = 0x%x\n", erase.PageAddress);
  }
  
  

  // �������С��CRCУ���뱣�浽RESERVED����, ������CRCУ��
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, START_ADDR, firmware_info.size);
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, START_ADDR + 4, firmware_info.firmware_checksum);
#endif
  
  while (uart_frontaddr != firmware_info.end_addr)
  {
    // �ȴ�FIFO��������
    err = 0;
    ticks = HAL_GetTick();
    while (uart_front == uart_rear)
    {
      if (HAL_GetTick() - ticks > 3500)
      {
        // ���뵽������, һ������Ϊ��������ж�ʧ��ĳЩ�ֽ�, �޷��������������ݰ�, DMA�����޷����
        // (�ⲻͬ��HAL_UART_RxCpltCallback�����CRC����, ֻ���յ������������ݰ�, ��CRCУ�鲻ͨ��, ���϶�ΪCRC����)
        // Ҳ��������Ϊ�û�ȡ���˳�����д
        printf("Data timeout!\n");
        //status = HAL_UART_Abort(&huart3); // ��������ʽǿ����ֹ�жϻ�DMA��ʽ�Ĵ��ڴ���
        status = HAL_UART_Abort(&huart2);
        if (status == HAL_OK)
          uart_busy = 0; // ��ֹ�ɹ�
        
        // ע��HAL_UART_Abort��HAL_UART_Abort_IT������
        // HAL_UART_Abort�ں�������ʱ�Ѿ���ֹ�ɹ�, ���ᴥ���ص�����
        // HAL_UART_Abort_IT�����жϷ�ʽǿ����ֹ�жϻ�DMA��ʽ�Ĵ��ڴ���
        // ��������ʱ��һ���Ѿ���ֹ���, ֱ��HAL_UART_AbortCpltCallback�ص���������ʱ, ������ֹ���
        
        // ������λ���ش����ݰ�
        err++;
        if (err == 3)
          break; // 10�뻹û�з�Ӧ, ǿ���˳�
        ticks = HAL_GetTick();
        dfu_start_transfer();
      }
    }
    if (uart_front == uart_rear)
      break;
    
    // ����������
    maxsize = firmware_info.end_addr - uart_frontaddr;
    size = UART_MTU;
    if (size > maxsize)
      size = maxsize;
    
    // ��дFlash
    // ע��: ����ַ���ܱ�4����ʱ, ������*(uint32_t *)
    //       Ӧ����*(__packed uint32_t *), ��__UNALIGNED_UINT32_READ, ����ᵼ��Hard Error
#if PROGRAM_ENABLE
    for (i = 0; i < size; i += 4)
      HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, uart_frontaddr + i, __UNALIGNED_UINT32_READ(uart_data[uart_front] + i));
#endif
    
    // �ͷ�FIFO
    uart_front = (uart_front + 1) % UART_FIFOCNT;
    uart_frontaddr += size;
  }
  
  // ��������
  assert_param(!uart_busy);
  HAL_NVIC_DisableIRQ(DMA1_Channel2_IRQn);
  HAL_NVIC_DisableIRQ(DMA1_Channel3_IRQn);
  HAL_NVIC_DisableIRQ(USART3_IRQn);
  if (uart_frontaddr == firmware_info.end_addr)
  {
    device_response.addr = uart_frontaddr;
    device_response.size = 0;
    device_response.checksum = calc_crc8(&device_response, sizeof(DeviceResponse) - 1);
    // HAL_UART_Transmit(&huart3, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    ret = 0;
  }
  else
    ret = -1;
  
#if PROGRAM_ENABLE
  HAL_FLASH_Lock();
#endif
  return ret;
}

/* ������������̼����� */
static int dfu_start_transfer(void)
{
  uint32_t maxsize;
  
  if (uart_busy)
    return -1;
  // if (huart3.gState != HAL_UART_STATE_READY)
  if (huart2.gState != HAL_UART_STATE_READY)
    return -1; // ���UART Handle����, ���ݲ���������, �Ժ���TxCallback�������� (STM32H7������������)
  if ((uart_rear + 1) % UART_FIFOCNT == uart_front)
    return -1; // FIFO����
  
  maxsize = firmware_info.end_addr - uart_rearaddr;
  if (maxsize == 0)
    return -1; // �̼��Ѵ������
  
  uart_busy = 1;
  device_response.addr = uart_rearaddr;
  device_response.size = UART_MTU;
  if (device_response.size > maxsize)
    device_response.size = maxsize;
  device_response.checksum = calc_crc8(&device_response, sizeof(DeviceResponse) - 1);
  //printf("addr=0x%x, size=%d, checksum=0x%x\n", device_response.addr, device_response.size,
  //        device_response.checksum);
  // HAL_UART_Transmit_DMA(&huart3, (uint8_t *)&device_response, sizeof(DeviceResponse));
  // HAL_UART_Receive_DMA(&huart3, uart_data[uart_rear], device_response.size + 1);
  HAL_UART_Transmit_DMA(&huart2, (uint8_t *)&device_response, sizeof(DeviceResponse));
  HAL_UART_Receive_DMA(&huart2, uart_data[uart_rear], device_response.size + 1);
  return 0;
  return 0;
}

static int dfu_sync(void)
{
  int i;
  uint8_t sync[16];
  HAL_StatusTypeDef status;
  //status = HAL_UART_Receive(&huart3, sync, sizeof(sync), 1000);
  status = HAL_UART_Receive(&huart2, sync, sizeof(sync), 1000);
  if (status == HAL_OK)
  {
    for (i = 0; i < sizeof(sync); i++)
    {
      if (sync[i] != 0xab)
        break;
    }
    if (i == sizeof(sync))
      return 0;
  }
  return -1;
}

static void iwdg_init(void)
{
#ifdef __HAL_RCC_DBGMCU_CLK_ENABLE
  __HAL_RCC_DBGMCU_CLK_ENABLE();
#endif
#ifdef __HAL_DBGMCU_FREEZE_IWDG
  __HAL_DBGMCU_FREEZE_IWDG();
#endif
  
  hiwdg.Instance = IWDG;
  hiwdg.Init.Prescaler = IWDG_PRESCALER_128;
  hiwdg.Init.Reload = 3124; // ��ʱʱ��Ϊ(3124+1)*3.2ms=10s
  //hiwdg.Init.Window = 4095; // �е�STM32��IWDG��Window����, ����Ҫ����
  HAL_IWDG_Init(&hiwdg);
}

static int jump_to_application(void)
{
  static Runnable run;
  const uint8_t *crc = (const uint8_t *)(START_ADDR + 4);
  const uint32_t *size = (const uint32_t *)START_ADDR;
  uint32_t *addr = (uint32_t *)(START_ADDR + RESERVED_SIZE + 4);
  uint32_t *msp = (uint32_t *)(START_ADDR + RESERVED_SIZE);
  uint32_t value;
  
  if (*(crc + 1) == 0x54)
  {
    // CRCУ���ѹر�
    printf("CRC check has been disabled!\n");
  }
  else
  {
    printf("reserve size=%d, crc=0x%2x\n", *size, *crc);
    // ����CRCУ��
    if (START_ADDR + RESERVED_SIZE + *size > FLASH_BASE + FLASH_SIZE)
    {
      printf("Program data error!\n");
      return -1;
    }
    
    value = calc_crc8((const uint8_t *)START_ADDR + RESERVED_SIZE, *size);
    if (*crc == value)
      printf("CRC passed!\n");
    else
    {
      printf("CRC failed! 0x%02x!=0x%02x\n", *crc, value);
      return -1;
    }
  }
  printf("app addr=%p, msp=%p\n", addr, msp);
  if ((*msp & 0xf0000000) != 0x20000000 || (*addr & 0xff000000) != 0x08000000)
  {
    printf("Program data error!\n");
    return -1;
  }
  HAL_Delay(500);
  printf("Jump to application...\n");
  run = (Runnable)*addr;
  HAL_DeInit(); // �ر�����������ж�, ��ֹ�ӳ���������ܵ��������Ӱ�� (����Ҫ)
  
  // һ���޸���ջ��ָ��, �Ͳ�����ʹ�ú����еľֲ�����, ������ܻ����Hard Error����
  // ������ʹ��static����, ����run������������Ϊstatic
  __set_MSP(*msp); // �޸�ջ��ָ��
  run();
  return 0;
}

int main(void)
{
  int dfu_cnt = 3; // DFU����ͬ������ (��ֵԽ��, ��λ����⵽�豸�ĳɹ���Խ��, ���������������Խ��)
  int ret;
  int dfuSuccessFlag = 0;
  uint8_t *pBootFlag = NULL;
  FLASH_EraseInitTypeDef erase;
  uint32_t err;
  
  SystemCoreClockUpdate(); // system_stm32f1xx.c�����SystemCoreClock��ֵ����, �Ⱦ���һ��
  HAL_Init(); // ������SysTick������ȷ��ʼ��
  
  //iwdg_init();
  usart_init(115200);
  printf_enable(PRINTF_ENABLE);
  printf("STM32F10x DFU\n");
  printf("SystemCoreClock=%u\n", SystemCoreClock);
  
  crc_init();
  while (1)
  {
    while (dfu_cnt > 0)
    {
      dfu_cnt--;
      printf("wait upgrade cnt=%d\n", dfu_cnt);
      ret = dfu_sync(); // ����������DFU����
      if (ret == 0)
      {
        // �ɹ�����DFUģʽ
        ret = dfu_process(); // DFU����

        // ��ͣ, �ȴ���λ���·�����
        printf("Send any command to continue...\n");
        // while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE) == RESET)
          
        // HAL_UART_Receive(&huart3, uart_data[0], 1, HAL_MAX_DELAY);
        
        if (ret == 0)
        {
          dfu_cnt = 0; // �������سɹ���, ��������
          HAL_Delay(2000); // �ӳ��㹻��ʱ��, ��֤��ʹ�������ߴ���, Ҳ���ڴ򿪴��ڵ������ֺ󿴵���һ�е�����Ϣ
        }
        else
          dfu_cnt = 10; // ��������ʧ��ʱ, �����г���, �������½�������ģʽ
      }
    }
    printf("jump app\n");
    HAL_Delay(500);
    // �����û����� (�������ɹ�, ����������)
    jump_to_application();
    
    // �����û�����ʧ��, �ٴν���DFUģʽ
    HAL_Delay(250);
    dfu_cnt = 1;
  }
}

void DMA1_Channel2_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma12);
}

void DMA1_Channel3_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma13);
}

void USART3_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart3);
}

void DMA1_Channel7_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma17);
}

void DMA1_Channel6_IRQHandler(void)
{
  HAL_DMA_IRQHandler(&hdma16);
}

void USART2_IRQHandler(void)
{
  HAL_UART_IRQHandler(&huart2);
}

void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
  if (huart->Instance == USART1)
    printf("USART1 error: 0x%x!\n", huart->ErrorCode);
  else if (huart->Instance == USART3)
    printf("USART3 error: 0x%x!\n", huart->ErrorCode);
  else if (huart->Instance == USART2)
    printf("USART2 error: 0x%x!\n", huart->ErrorCode);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
  uint8_t crc;
  
  // ����CRCУ��
  crc = calc_crc8(uart_data[uart_rear], device_response.size + 1);
  if (crc == 0)
  {
    uart_rear = (uart_rear + 1) % UART_FIFOCNT; // У��ͨ��, ������һ�����ݰ�
    uart_rearaddr += device_response.size;
  }
  else
    printf("Data CRC failed!\n");
  
  uart_busy = 0;
  dfu_start_transfer();
}

void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
  dfu_start_transfer();
}
