// 本程序用内部晶振(HSI)作为系统时钟, 没有用到外部晶振(HSE), 所以不需要指定HSE_VALUE的值
// 本程序已选择-O3优化。若要用ST-Link调试本程序, 请改选-O0优化
#include <stdio.h>
#include <stm32f1xx.h>
#include <string.h>
#include "common.h"
#include "DFU.h"

// 若想要直接在Keil中下载并用ST-Link调试子程序, 则需要禁用CRC校验
#define START_ADDR 0x08004800 // 一定要保证这个地址不在DFU程序的地址范围中, 否则擦除Flash会直接Hard Error
                              // 可以在项目属性中设置IROM1的Size, 限制主程序的大小, 防止主程序过大
                              // 若主程序太大, 可以在C/C++选项卡中开启-O3优化减小主程序大小
#define RESERVED_SIZE 0x800 // 请参考system_stm32xxx.c中描述的VECT_TAB_OFFSET的对齐方式
                             // 里面有这样一句话: Vector Table base offset field. This value must be a multiple of 0xXXX.
                             // 必须保证设置RESERVED_SIZE的值能被0xXXX整除, 并且在START_ADDR所在的Flash页上, RESERVED_SIZE能占满整页

#define HARDWARE_CRC 0 // 启用硬件CRC校验
#define PROGRAM_ENABLE 1 // 是否真正烧写Flash (调试用)
#define PRINTF_ENABLE 1 // 是否开启调试输出
#define UART_FIFOCNT 35 // 数据缓冲区个数
#define UART_MTU 512 // 每个缓冲区的大小 (必须为4的倍数)

#if UART_MTU % 4 != 0
#error "UART_MTU must be a multiple of 4"
#endif

static int dfu_process_crc_config(const CRCConfig *config);
static int dfu_process_firmware_download(void);
static int dfu_start_transfer(void);

DMA_HandleTypeDef hdma12, hdma13;
DMA_HandleTypeDef hdma17, hdma16;
IWDG_HandleTypeDef hiwdg;
// 下面这几款是目前市面上能买到的芯片, 其他的例如F101、F102、F105系列已经停产, 基本买不到了
// {-页数, -页大小}
// #if defined(STM32F103x6) || defined(STM32F103xB)
  // low-density devices, medium-density devices
static const int16_t flash_pages[] = {-128, -1};
// #elif defined(STM32F103xE) || defined(STM32F103xG) || defined(STM32F107xC)
//   // high-density devices, XL-density devices, connectivity line devices
// static const int16_t flash_pages[] = {-512, -2};
// #endif
static const uint32_t crc_disable __attribute((at(START_ADDR + 4))) = 0x54545454; // 默认禁用CRC, 这样ST-Link下载了主程序后能马上下载子程序
static uint8_t uart_data[UART_FIFOCNT][UART_MTU];
static volatile uint8_t uart_front, uart_rear, uart_busy;
static volatile uint32_t uart_frontaddr, uart_rearaddr;
static DeviceResponse device_response;
static FirmwareInfo firmware_info;

#if HARDWARE_CRC
// 注意: 不是所有的STM32单片机都能使用自定义CRC校验多项式
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

// 请在这里配置串口收发所用的DMA通道
// 注意文件最后还有DMA中断的函数名也要改
static void dfu_dma_init(void)
{
#if 0
  if (hdma12.Instance != NULL)
    return;
  
  __HAL_RCC_DMA1_CLK_ENABLE();
  
  hdma12.Instance = DMA1_Channel2;
  hdma12.Init.Direction = DMA_MEMORY_TO_PERIPH; // 注意方向不要写反了(1)
  hdma12.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma12.Init.MemInc = DMA_MINC_ENABLE;
  hdma12.Init.Mode = DMA_NORMAL;
  hdma12.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma12.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma12.Init.Priority = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&hdma12);
  __HAL_LINKDMA(&huart3, hdmatx, hdma12); // 注意方向不要写反了(2)
  
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
  hdma17.Init.Direction = DMA_MEMORY_TO_PERIPH; // 注意方向不要写反了(1)
  hdma17.Init.MemDataAlignment = DMA_MDATAALIGN_BYTE;
  hdma17.Init.MemInc = DMA_MINC_ENABLE;
  hdma17.Init.Mode = DMA_NORMAL;
  hdma17.Init.PeriphDataAlignment = DMA_PDATAALIGN_BYTE;
  hdma17.Init.PeriphInc = DMA_PINC_DISABLE;
  hdma17.Init.Priority = DMA_PRIORITY_HIGH;
  HAL_DMA_Init(&hdma17);
  __HAL_LINKDMA(&huart2, hdmatx, hdma17); // 注意方向不要写反了(2)
  
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
  // 发送成功进入DFU模式的回应
  memset(sync, 0xcd, sizeof(sync));
  //HAL_UART_Transmit(&huart3, sync, sizeof(sync), HAL_MAX_DELAY);
  HAL_UART_Transmit(&huart2, sync, sizeof(sync), HAL_MAX_DELAY);
  printf("Enter DFU mode!\n");
  
  // 接收请求信息
  i = 0;
  do
  {
    // 跳过冗余的0xab同步信号, 寻找头部字段
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
          // CRC校验配置
          printf("header=HEADER_CRC_CONFIG\n");
          if (len >= sizeof(CRCConfig))
          {
            memcpy(&crccfg, &uart_data[0][j], sizeof(CRCConfig));
            if (calc_crc8(&crccfg, sizeof(CRCConfig)) == 0)
              header_valid = 1;
          }
          break;
        case HEADER_FIRMWARE_INFO:
          // 固件信息头
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
      // 请求重传头部
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
  
  // 处理请求
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

/* 处理CRC校验配置请求 */
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

/* 处理固件下载请求 */
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
    // Flash容量不够
    device_response.addr = 0;
    device_response.size = maxsize;
    device_response.checksum = calc_crc8(&device_response, sizeof(DeviceResponse) - 1);
    //HAL_UART_Transmit(&huart3, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    HAL_UART_Transmit(&huart2, (uint8_t *)&device_response, sizeof(DeviceResponse), HAL_MAX_DELAY);
    return -1;
  }
  else if (firmware_info.start_addr != START_ADDR + RESERVED_SIZE)
  {
    // 程序的起始地址不正确
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
  
  // 利用中断, 并行接收数据
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
  // 擦除需要的Flash页
  HAL_FLASH_Unlock();
  
  addr[0] = FLASH_BASE;
  addr[2] = firmware_info.end_addr - 1; // 子程序结束地址
  memset(&erase, 0xff, sizeof(FLASH_EraseInitTypeDef));
  for (i = 0; i < -flash_pages[0]; i++)
  {
    // i为页号, addr[0]为页i的起始地址, addr[1]为页i的结束地址
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
  
  

  // 将程序大小和CRC校验码保存到RESERVED区域, 并启用CRC校验
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, START_ADDR, firmware_info.size);
  HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, START_ADDR + 4, firmware_info.firmware_checksum);
#endif
  
  while (uart_frontaddr != firmware_info.end_addr)
  {
    // 等待FIFO中有数据
    err = 0;
    ticks = HAL_GetTick();
    while (uart_front == uart_rear)
    {
      if (HAL_GetTick() - ticks > 3500)
      {
        // 进入到这里面, 一般是因为传输过程中丢失了某些字节, 无法构成完整的数据包, DMA传输无法完成
        // (这不同于HAL_UART_RxCpltCallback里面的CRC错误, 只有收到了完整的数据包, 但CRC校验不通过, 才认定为CRC错误)
        // 也可能是因为用户取消了程序烧写
        printf("Data timeout!\n");
        //status = HAL_UART_Abort(&huart3); // 以阻塞方式强行中止中断或DMA方式的串口传输
        status = HAL_UART_Abort(&huart2);
        if (status == HAL_OK)
          uart_busy = 0; // 中止成功
        
        // 注意HAL_UART_Abort和HAL_UART_Abort_IT的区别
        // HAL_UART_Abort在函数返回时已经中止成功, 不会触发回调函数
        // HAL_UART_Abort_IT是以中断方式强行中止中断或DMA方式的串口传输
        // 函数返回时不一定已经中止完毕, 直到HAL_UART_AbortCpltCallback回调函数触发时, 才算中止完毕
        
        // 请求上位机重传数据包
        err++;
        if (err == 3)
          break; // 10秒还没有反应, 强行退出
        ticks = HAL_GetTick();
        dfu_start_transfer();
      }
    }
    if (uart_front == uart_rear)
      break;
    
    // 计算数据量
    maxsize = firmware_info.end_addr - uart_frontaddr;
    size = UART_MTU;
    if (size > maxsize)
      size = maxsize;
    
    // 烧写Flash
    // 注意: 当地址不能被4整除时, 不能用*(uint32_t *)
    //       应该用*(__packed uint32_t *), 即__UNALIGNED_UINT32_READ, 否则会导致Hard Error
#if PROGRAM_ENABLE
    for (i = 0; i < size; i += 4)
      HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD, uart_frontaddr + i, __UNALIGNED_UINT32_READ(uart_data[uart_front] + i));
#endif
    
    // 释放FIFO
    uart_front = (uart_front + 1) % UART_FIFOCNT;
    uart_frontaddr += size;
  }
  
  // 结束请求
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

/* 请求主机传输固件数据 */
static int dfu_start_transfer(void)
{
  uint32_t maxsize;
  
  if (uart_busy)
    return -1;
  // if (huart3.gState != HAL_UART_STATE_READY)
  if (huart2.gState != HAL_UART_STATE_READY)
    return -1; // 如果UART Handle被锁, 则暂不启动传输, 稍后在TxCallback里面重试 (STM32H7就有这种问题)
  if ((uart_rear + 1) % UART_FIFOCNT == uart_front)
    return -1; // FIFO已满
  
  maxsize = firmware_info.end_addr - uart_rearaddr;
  if (maxsize == 0)
    return -1; // 固件已传输完毕
  
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
  hiwdg.Init.Reload = 3124; // 超时时间为(3124+1)*3.2ms=10s
  //hiwdg.Init.Window = 4095; // 有的STM32的IWDG有Window参数, 必须要设置
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
    // CRC校验已关闭
    printf("CRC check has been disabled!\n");
  }
  else
  {
    printf("reserve size=%d, crc=0x%2x\n", *size, *crc);
    // 进行CRC校验
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
  HAL_DeInit(); // 关闭所有外设和中断, 防止子程序的运行受到主程序的影响 (很重要)
  
  // 一旦修改了栈顶指针, 就不能再使用函数中的局部变量, 否则可能会出现Hard Error错误
  // 但可以使用static变量, 所以run变量必须声明为static
  __set_MSP(*msp); // 修改栈顶指针
  run();
  return 0;
}

int main(void)
{
  int dfu_cnt = 3; // DFU请求同步次数 (数值越大, 上位机检测到设备的成功率越高, 但开机后程序启动越慢)
  int ret;
  int dfuSuccessFlag = 0;
  uint8_t *pBootFlag = NULL;
  FLASH_EraseInitTypeDef erase;
  uint32_t err;
  
  SystemCoreClockUpdate(); // system_stm32f1xx.c里面的SystemCoreClock初值有误, 先纠正一下
  HAL_Init(); // 纠正后SysTick才能正确初始化
  
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
      ret = dfu_sync(); // 接收主机的DFU请求
      if (ret == 0)
      {
        // 成功进入DFU模式
        ret = dfu_process(); // DFU处理

        // 暂停, 等待上位机下发命令
        printf("Send any command to continue...\n");
        // while (__HAL_UART_GET_FLAG(&huart3, UART_FLAG_RXNE) == RESET)
          
        // HAL_UART_Receive(&huart3, uart_data[0], 1, HAL_MAX_DELAY);
        
        if (ret == 0)
        {
          dfu_cnt = 0; // 程序下载成功后, 启动程序
          HAL_Delay(2000); // 延迟足够的时间, 保证即使是用有线串口, 也能在打开串口调试助手后看到第一行调试信息
        }
        else
          dfu_cnt = 10; // 程序下载失败时, 不运行程序, 而是重新进入下载模式
      }
    }
    printf("jump app\n");
    HAL_Delay(500);
    // 启动用户程序 (若启动成功, 则函数不返回)
    jump_to_application();
    
    // 启动用户程序失败, 再次进入DFU模式
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
  
  // 进行CRC校验
  crc = calc_crc8(uart_data[uart_rear], device_response.size + 1);
  if (crc == 0)
  {
    uart_rear = (uart_rear + 1) % UART_FIFOCNT; // 校验通过, 接收下一个数据包
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
