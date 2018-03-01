/**
  ******************************************************************************
  * @file    main.c
  * @author  Ac6
  * @version V1.0
  * @date    01-December-2013
  * @brief   Default main function.
  ******************************************************************************
*/


#include <stm32f10x.h>
#include <stm32f10x_gpio.h>
#include <stm32f10x_rcc.h>
#include <stm32f10x_spi.h>
#include <stdlib.h>
#include <stdio.h>
#include <tim2_delay_custom.h>

#include "hw_config.h"
#include "usb_lib.h"
#include "usb_desc.h"
#include "usb_pwr.h"
#include "usb_init.h"

extern volatile uint8_t Receive_Buffer[64];
extern volatile uint32_t Receive_length ;
extern volatile uint32_t length ;
uint8_t Send_Buffer[64];
uint32_t packet_sent=1;
uint32_t packet_receive=1;

void __attribute__((weak)) initialise_monitor_handles(void);

/*---------------------------------------- RTC Related Function START--------------------------------------------*/
void SetSysClockToHSE(void)
{
	ErrorStatus HSEStartUpStatus;

	/* SYSCLK, HCLK, PCLK2 and PCLK1 configuration -----------------------------*/
    /* RCC system reset(for debug purpose) */
    RCC_DeInit();

    /* Enable HSE */
    RCC_HSEConfig( RCC_HSE_ON);

    /* Wait till HSE is ready */
    HSEStartUpStatus = RCC_WaitForHSEStartUp();

    if (HSEStartUpStatus == SUCCESS)
    {
        /* Enable Prefetch Buffer */
        //FLASH_PrefetchBufferCmd( FLASH_PrefetchBuffer_Enable);

        /* Flash 0 wait state */
        //FLASH_SetLatency( FLASH_Latency_0);

        /* HCLK = SYSCLK */
        RCC_HCLKConfig( RCC_SYSCLK_Div1);

        /* PCLK2 = HCLK */
        RCC_PCLK2Config( RCC_HCLK_Div1);

        /* PCLK1 = HCLK */
        RCC_PCLK1Config(RCC_HCLK_Div1);

        /* Select HSE as system clock source */
        RCC_SYSCLKConfig( RCC_SYSCLKSource_HSE);

        /* Wait till PLL is used as system clock source */
        while (RCC_GetSYSCLKSource() != 0x04)
        {
        }
    }
    else
    { /* If HSE fails to start-up, the application will have wrong clock configuration.
     User can add here some code to deal with this error */

        /* Go to infinite loop */
        while (1)
        {
        }
    }
}

unsigned char RTC_Init(void)
{
	// Äîçâîëèòè òàêòóâàííÿ ìîäóë³â óïðàâë³ííÿ æèâëåííÿì ³ óïðàâë³ííÿì ðåçåðâíî¿ îáëàñòþ
	RCC_APB1PeriphClockCmd(RCC_APB1Periph_PWR | RCC_APB1Periph_BKP, ENABLE);
	// Äîçâîëèòè äîñòóï äî îáëàñò³ ðåçåðâíèõ äàíèõ
	PWR_BackupAccessCmd(ENABLE);
	// ßêùî ãîäèííèê âèìêíåíèé - ³í³ö³àë³çóâàòè
	if ((RCC->BDCR & RCC_BDCR_RTCEN) != RCC_BDCR_RTCEN)
	{
		// Âèêîíàòè ñêèäàííÿ îáëàñò³ ðåçåðâíèõ äàíèõ
		RCC_BackupResetCmd(ENABLE);
		RCC_BackupResetCmd(DISABLE);

		// Âèáðàòè äæåðåëîì òàêòîâèõ ³ìïóëüñ³â çîâí³øí³é êâàðö 32768 ³ ïîäàòè òàêòóâàííÿ
		RCC_LSEConfig(RCC_LSE_ON);
		while ((RCC->BDCR & RCC_BDCR_LSERDY) != RCC_BDCR_LSERDY) {}
		RCC_RTCCLKConfig(RCC_RTCCLKSource_LSE);

		RTC_SetPrescaler(0x7FFF); // Âñòàíîâèòè ïîä³ëþâà÷ ùîá ãîäèííèê ðàõóâàâ ñåêóíäè

		// Âìèêàºìî ãîäèííèê
		RCC_RTCCLKCmd(ENABLE);

		// ×åêàºìî íà ñèíõðîí³çàö³þ
		RTC_WaitForSynchro();

		return 1;
	}
	return 0;
}

// (UnixTime = 00:00:00 01.01.1970 = JD0 = 2440588)
#define JULIAN_DATE_BASE	2440588

typedef struct
{
	uint8_t RTC_Hours;
	uint8_t RTC_Minutes;
	uint8_t RTC_Seconds;
	uint8_t RTC_Date;
	uint8_t RTC_Wday;
	uint8_t RTC_Month;
	uint16_t RTC_Year;
} RTC_DateTimeTypeDef;

// Get current date
void RTC_GetDateTime(uint32_t RTC_Counter, RTC_DateTimeTypeDef* RTC_DateTimeStruct) {
	unsigned long time;
	unsigned long t1, a, b, c, d, e, m;
	int year = 0;
	int mon = 0;
	int wday = 0;
	int mday = 0;
	int hour = 0;
	int min = 0;
	int sec = 0;
	uint64_t jd = 0;;
	uint64_t jdn = 0;

	jd = ((RTC_Counter+43200)/(86400>>1)) + (2440587<<1) + 1;
	jdn = jd>>1;

	time = RTC_Counter;
	t1 = time/60;
	sec = time - t1*60;

	time = t1;
	t1 = time/60;
	min = time - t1*60;

	time = t1;
	t1 = time/24;
	hour = time - t1*24;

	wday = jdn%7;

	a = jdn + 32044;
	b = (4*a+3)/146097;
	c = a - (146097*b)/4;
	d = (4*c+3)/1461;
	e = c - (1461*d)/4;
	m = (5*e+2)/153;
	mday = e - (153*m+2)/5 + 1;
	mon = m + 3 - 12*(m/10);
	year = 100*b + d - 4800 + (m/10);

	RTC_DateTimeStruct->RTC_Year = year;
	RTC_DateTimeStruct->RTC_Month = mon;
	RTC_DateTimeStruct->RTC_Date = mday;
	RTC_DateTimeStruct->RTC_Hours = hour;
	RTC_DateTimeStruct->RTC_Minutes = min;
	RTC_DateTimeStruct->RTC_Seconds = sec;
	RTC_DateTimeStruct->RTC_Wday = wday;
}

// Convert Date to Counter
uint32_t RTC_GetRTC_Counter(RTC_DateTimeTypeDef* RTC_DateTimeStruct) {
	uint8_t a;
	uint16_t y;
	uint8_t m;
	uint32_t JDN;

	a=(14-RTC_DateTimeStruct->RTC_Month)/12;
	y=RTC_DateTimeStruct->RTC_Year+4800-a;
	m=RTC_DateTimeStruct->RTC_Month+(12*a)-3;

	JDN=RTC_DateTimeStruct->RTC_Date;
	JDN+=(153*m+2)/5;
	JDN+=365*y;
	JDN+=y/4;
	JDN+=-y/100;
	JDN+=y/400;
	JDN = JDN -32045;
	JDN = JDN - JULIAN_DATE_BASE;
	JDN*=86400;
	JDN+=(RTC_DateTimeStruct->RTC_Hours*3600);
	JDN+=(RTC_DateTimeStruct->RTC_Minutes*60);
	JDN+=(RTC_DateTimeStruct->RTC_Seconds);

	return JDN;
}

void RTC_GetMyFormat(RTC_DateTimeTypeDef* RTC_DateTimeStruct, char * buffer) {
	const char WDAY0[] = "Monday";
	const char WDAY1[] = "Tuesday";
	const char WDAY2[] = "Wednesday";
	const char WDAY3[] = "Thursday";
	const char WDAY4[] = "Friday";
	const char WDAY5[] = "Saturday";
	const char WDAY6[] = "Sunday";
	const char * WDAY[7]={WDAY0, WDAY1, WDAY2, WDAY3, WDAY4, WDAY5, WDAY6};

	const char MONTH1[] = "January";
	const char MONTH2[] = "February";
	const char MONTH3[] = "March";
	const char MONTH4[] = "April";
	const char MONTH5[] = "May";
	const char MONTH6[] = "June";
	const char MONTH7[] = "July";
	const char MONTH8[] = "August";
	const char MONTH9[] = "September";
	const char MONTH10[] = "October";
	const char MONTH11[] = "November";
	const char MONTH12[] = "December";
	const char * MONTH[12]={MONTH1, MONTH2, MONTH3, MONTH4, MONTH5, MONTH6, MONTH7, MONTH8, MONTH9, MONTH10, MONTH11, MONTH12};

	sprintf(buffer, "%s %d %s %04d",
			WDAY[RTC_DateTimeStruct->RTC_Wday],
			RTC_DateTimeStruct->RTC_Date,
			MONTH[RTC_DateTimeStruct->RTC_Month -1],
			RTC_DateTimeStruct->RTC_Year);
}
/*---------------------------------------- RTC Related Function END--------------------------------------------*/
/*---------------------------------------- USB Related START---------------------------------------------------*/
void SetSysClockTo72(void)
{
	ErrorStatus HSEStartUpStatus;
    /* SYSCLK, HCLK, PCLK2 and PCLK1 configuration -----------------------------*/
    /* RCC system reset(for debug purpose) */
    RCC_DeInit();

    /* Enable HSE */
    RCC_HSEConfig( RCC_HSE_ON);

    /* Wait till HSE is ready */
    HSEStartUpStatus = RCC_WaitForHSEStartUp();

    if (HSEStartUpStatus == SUCCESS)
    {
        /* Enable Prefetch Buffer */
    	//FLASH_PrefetchBufferCmd( FLASH_PrefetchBuffer_Enable);

        /* Flash 2 wait state */
        //FLASH_SetLatency( FLASH_Latency_2);

        /* HCLK = SYSCLK */
        RCC_HCLKConfig( RCC_SYSCLK_Div1);

        /* PCLK2 = HCLK */
        RCC_PCLK2Config( RCC_HCLK_Div1);

        /* PCLK1 = HCLK/2 */
        RCC_PCLK1Config( RCC_HCLK_Div2);

        /* PLLCLK = 8MHz * 9 = 72 MHz */
        RCC_PLLConfig(0x00010000, RCC_PLLMul_9);

        /* Enable PLL */
        RCC_PLLCmd( ENABLE);

        /* Wait till PLL is ready */
        while (RCC_GetFlagStatus(RCC_FLAG_PLLRDY) == RESET)
        {
        }

        /* Select PLL as system clock source */
        RCC_SYSCLKConfig( RCC_SYSCLKSource_PLLCLK);

        /* Wait till PLL is used as system clock source */
        while (RCC_GetSYSCLKSource() != 0x08)
        {
        }
    }
    else
    { /* If HSE fails to start-up, the application will have wrong clock configuration.
     User can add here some code to deal with this error */

        /* Go to infinite loop */
        while (1)
        {
        }
    }
}
/*---------------------------------------- USB Related END---------------------------------------------------*/

void _sendData(uint8_t c)
{
	//while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_TXE) == RESET)
	{
	}
	SPI_I2S_SendData(SPI1, c);
	while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_BSY) == SET)
	{
	}
}
void _readData(uint8_t Add, uint8_t* Data)
{
	SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Reset);
	_sendData(Add);

	//while(SPI_I2S_GetFlagStatus(SPI1, SPI_I2S_FLAG_RXNE) == RESET)
	{
	}
	*Data = SPI_I2S_ReceiveData(SPI1);
	SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);
}

/*	GPIO Toggle*/
int main(void)
{
initialise_monitor_handles();
printf("Starting....!!\r\n");

char buffer[80] = {'\0'};
uint32_t RTC_Counter = 0;
uint8_t i;

RTC_DateTimeTypeDef	RTC_DateTime;
GPIO_InitTypeDef  GPIO_InitStructure;
SPI_InitTypeDef SPI_InitStructure;

SetSysClockToHSE();
TIM2_init();

/*Set_System();
SetSysClockTo72();
Set_USBClock();
USB_Interrupts_Config();
USB_Init();*/

/* Initialize LED which connected to PC13 	 /
/  Enable PORTC Clock						*/
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOC, ENABLE);
/* Configure the GPIO_LED pin */
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_13;
GPIO_InitStructure.GPIO_Mode = GPIO_Mode_Out_PP;
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_50MHz;
GPIO_Init(GPIOC, &GPIO_InitStructure);

//GPIO_SetBits(GPIOC, GPIO_Pin_13); // Set C13 to High level ("1")
GPIO_ResetBits(GPIOC, GPIO_Pin_13); // Set C13 to Low level ("0")

RCC_APB2PeriphClockCmd(RCC_APB2Periph_SPI1, ENABLE);
RCC_APB2PeriphClockCmd(RCC_APB2Periph_GPIOA, ENABLE);

GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_7/*MOSI1*/ |  GPIO_Pin_5;/*SCLK*/
GPIO_Init(GPIOA, &GPIO_InitStructure);

GPIO_InitStructure.GPIO_Mode = GPIO_Mode_AF_PP;
GPIO_InitStructure.GPIO_Speed = GPIO_Speed_2MHz;
GPIO_InitStructure.GPIO_Pin = GPIO_Pin_6 | GPIO_Pin_4;/*MISO1*/
GPIO_Init(GPIOA, &GPIO_InitStructure);
//GPIO_ResetBits(GPIOA, GPIO_Pin_6);
//GPIO_SetBits(GPIOA, GPIO_Pin_6);

SPI_InitStructure.SPI_Direction = SPI_Direction_2Lines_FullDuplex;
//SPI_InitStructure.SPI_Direction = SPI_Direction_1Line_Tx;
SPI_InitStructure.SPI_DataSize = SPI_DataSize_8b;
SPI_InitStructure.SPI_CPOL = SPI_CPOL_Low;
SPI_InitStructure.SPI_CPHA = SPI_CPHA_1Edge;
SPI_InitStructure.SPI_NSS = SPI_NSS_Soft;
SPI_InitStructure.SPI_BaudRatePrescaler = SPI_BaudRatePrescaler_8; /*(Fpclk/8 = 8Mhz/8 = 1Mhz)*/
SPI_InitStructure.SPI_FirstBit = SPI_FirstBit_MSB;
SPI_InitStructure.SPI_Mode = SPI_Mode_Master;
SPI_Init(SPI1, &SPI_InitStructure);
SPI_Cmd(SPI1, ENABLE);
SPI_NSSInternalSoftwareConfig(SPI1, SPI_NSSInternalSoft_Set);

if (RTC_Init() == 1)
{
	// ßêùî ïåðøà ³í³ö³àë³çàö³ÿ RTC Âñòàíîâëþºìî ïî÷àòêîâó äàòó, íàïðèêëàä 22.09.2016 14:30:00
	RTC_DateTime.RTC_Date = 27;
	RTC_DateTime.RTC_Month = 2;
	RTC_DateTime.RTC_Year = 2018;

	RTC_DateTime.RTC_Hours = 19;
	RTC_DateTime.RTC_Minutes = 53;
	RTC_DateTime.RTC_Seconds = 00;

	// Ï³ñëÿ ³í³ö³àë³çàö³¿ ïîòð³áíà çàòðèìêà. Áåç íå¿ ÷àñ íå âñòàíîâëþºòüñÿ.
	delay_ms(500);
	RTC_SetCounter(RTC_GetRTC_Counter(&RTC_DateTime));
}

while(1)
{
	GPIOC->ODR ^= GPIO_Pin_13; // Invert C13
	RTC_Counter = RTC_GetCounter();

	RTC_GetDateTime(RTC_Counter, &RTC_DateTime);
	printf("%02d.%02d.%04d  %02d:%02d:%02d\n",
			RTC_DateTime.RTC_Date, RTC_DateTime.RTC_Month, RTC_DateTime.RTC_Year,
			RTC_DateTime.RTC_Hours, RTC_DateTime.RTC_Minutes, RTC_DateTime.RTC_Seconds);

	// Ôóíêö³ÿ ãåíåðóº ó áóôåð³ äàòó âëàñíîãî ôîðìàòó
	RTC_GetMyFormat(&RTC_DateTime, buffer);

	/* delay */
	while (RTC_Counter == RTC_GetCounter())
	{
	}
	_readData(0x75,&i);
	printf("I_am = %d\n\r",i);
}

/*while (1)
{
	if (bDeviceState == CONFIGURED)
	{
		printf("A\n\r");
		CDC_Receive_DATA();
		printf("B\n\r");
		// Check to see if we have data yet
		if (Receive_length  != 0)
		{
			// If received symbol '1' then LED turn on, else LED turn off
			if (Receive_Buffer[0]=='1') {
				GPIO_ResetBits(GPIOC, GPIO_Pin_13);
			}
			else {
				GPIO_SetBits(GPIOC, GPIO_Pin_13);
			}

			// Echo
			if (packet_sent == 1) {
				CDC_Send_DATA ((uint8_t*)Receive_Buffer,Receive_length);
			}

			Receive_length = 0;
		}
	}
	//printf("Ready\n\r");
}

while (1)
{
	 Toggle LED which connected to PC13
	GPIOC->ODR ^= GPIO_Pin_13; // Invert C13
	 delay
	for(i=0;i<0x100000;i++);
	printf("Starting !!..\n\r");
}*/
}
