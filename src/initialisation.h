#ifdef STM32F446xx

#define USE_HSI
#define PLL_M 8
#define PLL_N 180
#define PLL_P 0		//  Main PLL (PLL) division factor for main system clock can be 2 (PLL_P = 0), 4 (PLL_P = 1), 6 (PLL_P = 2), 8 (PLL_P = 3)
#define PLL_Q 0
#define AHB_PRESCALAR 0b0000
#define APB1_PRESCALAR 0b100		// AHB divided by 2 APB Prescaler: 0b0xx: AHB clock not divided, 0b100 div by  2, 0b101: div by  4, 0b110 div by  8; 0b111 div by 16
#define APB2_PRESCALAR 0b100

#else

#define USE_HSE
#define PLL_M 4
#define PLL_N 168	//	Max is 168
#define PLL_P 0		//  Main PLL (PLL) division factor for main system clock can be 2 (PLL_P = 0), 4 (PLL_P = 1), 6 (PLL_P = 2), 8 (PLL_P = 3)
#define PLL_Q 0
#define AHB_PRESCALAR 0b0000
#define APB1_PRESCALAR 0b101		// AHB divided by 4 APB Prescaler: 0b0xx: AHB clock not divided, 0b100 div by  2, 0b101: div by  4, 0b110 div by  8; 0b111 div by 16
#define APB2_PRESCALAR 0b101

#endif

/* AHB Prescaler
0xxx: system clock not divided
1000: system clock divided by 2
1001: system clock divided by 4
1010: system clock divided by 8
1011: system clock divided by 16
1100: system clock divided by 64
1101: system clock divided by 128
1110: system clock divided by 256
1111: system clock divided by 512
*/

void SystemClock_Config(void)
{
	uint32_t temp = 0x00000000;

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;			// Enable Power Control clock
	PWR->CR |= PWR_CR_VOS_0;					// Enable VOS voltage scaling - allows maximum clock speed

#ifdef USE_HSE
	RCC->CR |= RCC_CR_HSEON;					// HSE ON
	while ((RCC->CR & RCC_CR_HSERDY) == 0);		// Wait till HSE is ready
	temp = RCC_PLLCFGR_PLLSRC_HSE;				// PLL source is HSE
#endif

#ifdef USE_HSI
	RCC->CR |= RCC_CR_HSION;					// HSI ON
	while((RCC->CR & RCC_CR_HSIRDY) == 0);		// Wait till HSI is ready
#endif

	//	Set the clock multipliers and dividers
	temp |= (uint32_t)PLL_M;
	temp |= ((uint32_t)PLL_N << 6);
	temp |= ((uint32_t)PLL_P << 16);
	temp |= ((uint32_t)PLL_Q << 24);
	RCC->PLLCFGR = temp;

	//	Set AHB, APB1 and APB2 prescalars
	temp = RCC->CFGR;
	temp |= ((uint32_t)AHB_PRESCALAR << 4);
	temp |= ((uint32_t)APB1_PRESCALAR << 10);
	temp |= ((uint32_t)APB2_PRESCALAR << 13);
	temp |= RCC_CFGR_SW_1;						// Select PLL as SYSCLK
	RCC->CFGR = temp;

	FLASH->ACR |= FLASH_ACR_LATENCY_5WS;		// Clock faster than 150MHz requires 5 Wait States for Flash memory access time

	RCC->CR |= RCC_CR_PLLON;					// Switch ON the PLL
	while ((RCC->CR & RCC_CR_PLLRDY) == 0);		// Wait till PLL is ready
	while ((RCC->CFGR & RCC_CFGR_SWS_PLL) == 0); // System clock switch status SWS = 0b10 = PLL is really selected

	// STM32F405x/407x/415x/417x Revision Z (0x1001) devices: prefetch is supported DW - assume revision Y (0x100F) is OK
	volatile uint32_t idNumber = DBGMCU->IDCODE;
	idNumber = idNumber >> 16;
	if (idNumber == 0x1001 || idNumber == 0x100F)
		FLASH->ACR |= FLASH_ACR_PRFTEN;			// Enable the Flash prefetch

	// See page 83 of manual for other possibly performance boost options: instruction cache enable (ICEN) and data cache enable (DCEN)
}

void InitLCD(void)
{
	//	Enable GPIO and SPI clocks
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;			// reset and clock control - advanced high performance bus - GPIO port C
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;			// reset and clock control - advanced high performance bus - GPIO port D
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;			// reset and clock control - advanced high performance bus - GPIO port F
	RCC->APB2ENR |= RCC_APB2ENR_SPI5EN;

	// Init WRX pin PD13
	GPIOD->MODER |= GPIO_MODER_MODER13_0;			// 00: Input (reset state)	01: General purpose output mode	10: Alternate function mode	11: Analog mode
	GPIOD->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR13_0;	// Medium  - 00: Low speed; 01: Medium speed; 10: High speed; 11: Very high speed

	// Init CS pin PC2
	GPIOC->MODER |= GPIO_MODER_MODER2_0;			// 00: Input (reset state)	01: General purpose output mode	10: Alternate function mode	11: Analog mode
	GPIOC->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR2_0;		// Medium  - 00: Low speed; 01: Medium speed; 10: High speed; 11: Very high speed

	// Init RESET pin PD12
	GPIOD->MODER |= GPIO_MODER_MODER12_0;			// 00: Input (reset state)	01: General purpose output mode	10: Alternate function mode	11: Analog mode
	GPIOD->PUPDR |= GPIO_PUPDR_PUPDR12_0;			// Pull up - 00: No pull-up, pull-down; 01 Pull-up; 10 Pull-down; 11 Reserved

	// Setup SPI pins -  PF7: SPI5_SCK;  PF8: SPI5_MISO;  PF9: SPI5_MOSI [all alternate function AF5 for SPI5]
	GPIOF->MODER |= GPIO_MODER_MODER7_1;			// 00: Input (reset state)	01: General purpose output mode	10: Alternate function mode	11: Analog mode
	GPIOF->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR7;		// V High  - 00: Low speed; 01: Medium speed; 10: High speed; 11: Very high speed
	GPIOF->AFR[0] |= 0b0101 << 28;					// 0b0101 = Alternate Function 5 (SPI5); 28 is position of Pin 7

	GPIOF->MODER |= GPIO_MODER_MODER8_1;			// 00: Input (reset state)	01: General purpose output mode	10: Alternate function mode	11: Analog mode
	GPIOF->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR8;		// V High  - 00: Low speed; 01: Medium speed; 10: High speed; 11: Very high speed
	GPIOF->AFR[1] |= 0b0101 << 0;					// 0b0101 = Alternate Function 5 (SPI5); 0 is position of Pin 8

	GPIOF->MODER |= GPIO_MODER_MODER9_1;			// 00: Input (reset state)	01: General purpose output mode	10: Alternate function mode	11: Analog mode
	GPIOF->OSPEEDR |= GPIO_OSPEEDER_OSPEEDR9;		// V High  - 00: Low speed; 01: Medium speed; 10: High speed; 11: Very high speed
	GPIOF->AFR[1] |= 0b0101 << 4;					// 0b0101 = Alternate Function 5 (SPI5); 4 is position of Pin 9

	// Configure SPI
	SPI5->CR1 |= SPI_CR1_SSM;						// Software slave management: When SSM bit is set, NSS pin input is replaced with the value from the SSI bit
	SPI5->CR1 |= SPI_CR1_SSI;						// Internal slave select
	SPI5->CR1 |= SPI_CR1_BR_0;						// Baud rate control prescaler: 0b001: fPCLK/4; 0b100: fPCLK/32
	SPI5->CR1 |= SPI_CR1_MSTR;						// Master selection

	SPI5->CR1 |= SPI_CR1_SPE;						// Enable SPI

	// Configure DMA
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;



	/*TM_SPI_DMA_INT_t SPI5_DMA_INT = {SPI5_DMA_TX_CHANNEL , SPI5_DMA_TX_STREAM, SPI5_DMA_RX_CHANNEL, SPI5_DMA_RX_STREAM};
	 SPI5 TX and RX default settings
	#ifndef SPI5_DMA_TX_STREAM
	#define SPI5_DMA_TX_STREAM    DMA2_Stream6
	#define SPI5_DMA_TX_CHANNEL   DMA_Channel_7
	#endif
	#ifndef SPI5_DMA_RX_STREAM
	#define SPI5_DMA_RX_STREAM    DMA2_Stream5
	#define SPI5_DMA_RX_CHANNEL   DMA_Channel_7
	#endif
*/

}

#define ADC_BUFFER_LENGTH 8
volatile uint16_t ADC_array[ADC_BUFFER_LENGTH];

void InitADC(void)
{
	//	Setup Timer 2 to trigger ADC
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;				// Enable Timer 2 clock
	TIM2->CR2 |= TIM_CR2_MMS_2;						// 100: Compare - OC1REF signal is used as trigger output (TRGO)
	TIM2->PSC = 20 - 1;								// Prescaler
	TIM2->ARR = 100 - 1;							// Auto-reload register (ie reset counter) divided by 100
	TIM2->CCR1 = 50 - 1;							// Capture and compare - ie when counter hits this number PWM high
	TIM2->CCER |= TIM_CCER_CC1E;					// Capture/Compare 1 output enable
	TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 |TIM_CCMR1_OC1M_2;	// 110 PWM Mode 1
	TIM2->CR1 |= TIM_CR1_CEN;

	// Enable ADC2 and GPIO clock sources
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	RCC->APB2ENR |= RCC_APB2ENR_ADC2EN;

	// Enable ADC - PC3: ADC123_IN13; PA5: ADC12_IN5;
	GPIOC->MODER |= GPIO_MODER_MODER3;				// Set PC3 to Analog mode (0b11)
	GPIOA->MODER |= GPIO_MODER_MODER5;				// Set PA5 to Analog mode (0b11)

	ADC2->CR1 |= ADC_CR1_SCAN;						// Activate scan mode
	//ADC2->SQR1 = (ADC_BUFFER_LENGTH - 1) << 20;	// Number of conversions in sequence
	ADC2->SQR1 = 1 << 20;							// Number of conversions in sequence (limit to two for now as we are getting multiple samples to average
	ADC2->SQR3 |= 13 << 0;							// Set IN13  1st conversion in sequence
	ADC2->SQR3 |= 5 << 5;							// Set IN5  2nd conversion in sequence

	//	Set to 56 cycles (0b11) sampling speed (SMPR2 Left shift speed 3 x ADC_INx up to input 9; use SMPR1 from 0 for ADC_IN10+)
	// 000: 3 cycles; 001: 15 cycles; 010: 28 cycles; 011: 56 cycles; 100: 84 cycles; 101: 112 cycles; 110: 144 cycles; 111: 480 cycles
	ADC2->SMPR1 |= 0b110 << 9;						// Set speed of IN13
	ADC2->SMPR2 |= 0b110 << 15;						// Set speed of IN5

	ADC2->CR2 |= ADC_CR2_EOCS;						// Trigger interrupt on end of each individual conversion
	ADC2->CR2 |= ADC_CR2_EXTEN_0;					// ADC hardware trigger 00: Trigger detection disabled; 01: Trigger detection on the rising edge; 10: Trigger detection on the falling edge; 11: Trigger detection on both the rising and falling edges
	ADC2->CR2 |= ADC_CR2_EXTSEL_1 | ADC_CR2_EXTSEL_2;	// ADC External trigger: 0110 = TIM2_TRGO event

	// Enable DMA - DMA2, Channel 1, Stream 2  = ADC2 (Manual p207)
	ADC2->CR2 |= ADC_CR2_DMA;						// Enable DMA Mode on ADC2
	ADC2->CR2 |= ADC_CR2_DDS;						// DMA requests are issued as long as data are converted and DMA=1
	RCC->AHB1ENR |= RCC_AHB1ENR_DMA2EN;

	DMA2_Stream2->CR &= ~DMA_SxCR_DIR;				// 00 = Peripheral-to-memory
	DMA2_Stream2->CR |= DMA_SxCR_PL_1;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High
	DMA2_Stream2->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream2->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream2->CR &= ~DMA_SxCR_PINC;				// Peripheral not in increment mode
	DMA2_Stream2->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA2_Stream2->CR |= DMA_SxCR_CIRC;				// circular mode to keep refilling buffer
	DMA2_Stream2->CR &= ~DMA_SxCR_DIR;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory

	DMA2_Stream2->NDTR |= ADC_BUFFER_LENGTH;		// Number of data items to transfer (ie size of ADC buffer)
	DMA2_Stream2->PAR = (uint32_t)(&(ADC2->DR));	// Configure the peripheral data register address
	DMA2_Stream2->M0AR = (uint32_t)(ADC_array);		// Configure the memory address (note that M1AR is used for double-buffer mode)
	DMA2_Stream2->CR |= DMA_SxCR_CHSEL_0;			// channel select to 1 for ADC2

	DMA2_Stream2->CR |= DMA_SxCR_EN;				// Enable DMA2
	ADC2->CR2 |= ADC_CR2_ADON;						// Activate ADC


	/*
	 * Using ADC1
	 *
	 *
	//	Setup Timer 2 to trigger ADC
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;		// Enable Timer 2 clock
	TIM2->CR2 |= TIM_CR2_MMS_2;				// 100: Compare - OC1REF signal is used as trigger output (TRGO)
	TIM2->PSC = 20 - 1;					// Prescaler
	TIM2->ARR = 100 - 1;					// Auto-reload register (ie reset counter) divided by 100
	TIM2->CCR1 = 50 - 1;					// Capture and compare - ie when counter hits this number PWM high
	TIM2->CCER |= TIM_CCER_CC1E;			// Capture/Compare 1 output enable
	TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 |TIM_CCMR1_OC1M_2;		// 110 PWM Mode 1
	TIM2->CR1 |= TIM_CR1_CEN;

	// Enable ADC1 clock source
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	// Enable ADC on PA7 (Pin 23) Alternate mode: ADC12_IN7
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	GPIOA->MODER |= GPIO_MODER_MODER1;			// Set PA1 to Analog mode (0b11)
	GPIOA->MODER |= GPIO_MODER_MODER7;			// Set PA7 to Analog mode (0b11)
	ADC1->CR1 |= ADC_CR1_SCAN;					// Activate scan mode
	ADC1->SQR1 = ADC_SQR1_L_0;					// Two conversions in sequence
	ADC1->SQR3 |= 7 << 0;						// Set Pin7 to first conversion in sequence
	ADC1->SQR3 |= 1 << 5;						// Set Pin1 to second conversion in sequence

	//	Set to slow sampling mode
	ADC1->SMPR2 |= 0b11 << 3;
	ADC1->SMPR2 |= 0b11 << 21;

	ADC1->CR2 |= ADC_CR2_EOCS;							// Trigger interrupt on end of each individual conversion
	ADC1->CR2 |= ADC_CR2_EXTEN_0;						// ADC hardware trigger 00: Trigger detection disabled; 01: Trigger detection on the rising edge; 10: Trigger detection on the falling edge; 11: Trigger detection on both the rising and falling edges
	ADC1->CR2 |= ADC_CR2_EXTSEL_1 | ADC_CR2_EXTSEL_2;	// ADC External trigger: 0110 = TIM2_TRGO event

	 CR2 EXTSEL settings
	0000: Timer 1 CC1 event
	0001: Timer 1 CC2 event
	0010: Timer 1 CC3 event
	0011: Timer 2 CC2 event
	0100: Timer 2 CC3 event
	0101: Timer 2 CC4 event
	0110: Timer 2 TRGO event
	0111: Timer 3 CC1 event
	1000: Timer 3 TRGO event
	1001: Timer 4 CC4 event
	1010: Timer 5 CC1 event
	1011: Timer 5 CC2 event
	1100: Timer 5 CC3 event
	1101: Timer 8 CC1 event
	1110: Timer 8 TRGO event
	1111: EXTI line 11


	 Interrupt settings
	ADC1->CR1 |= ADC_CR1_EOCIE;
	NVIC_EnableIRQ(ADC_IRQn);


	// Enable DMA - DMA2, Channel 0, Stream 0  = ADC1 (Manual p207)
	ADC1->CR2 |= ADC_CR2_DMA;						// Enable DMA Mode on ADC1
	ADC1->CR2 |= ADC_CR2_DDS;						// DMA requests are issued as long as data are converted and DMA=1
	RCC->AHB1ENR|= RCC_AHB1ENR_DMA2EN;

	DMA2_Stream0->CR &= ~DMA_SxCR_DIR;				// 00 = Peripheral-to-memory
	DMA2_Stream0->CR |= DMA_SxCR_PL_1;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High
	DMA2_Stream0->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream0->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream0->CR &= ~DMA_SxCR_PINC;				// Peripheral not in increment mode
	DMA2_Stream0->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA2_Stream0->CR |= DMA_SxCR_CIRC;				// circular mode to keep refilling buffer
	DMA2_Stream0->CR &= ~DMA_SxCR_DIR;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory

	DMA2_Stream0->NDTR |= ADC_BUFFER_LENGTH;		// Number of data items to transfer (ie size of ADC buffer)
	DMA2_Stream0->PAR = (uint32_t)(&(ADC1->DR));	// Configure the peripheral data register address
	DMA2_Stream0->M0AR = (uint32_t)(ADC_array);		// Configure the memory address (note that M1AR is used for double-buffer mode)
	DMA2_Stream0->CR &= ~DMA_SxCR_CHSEL;			// channel select to 0 for ADC1

	DMA2_Stream0->CR |= DMA_SxCR_EN;				// Enable DMA2
	ADC1->CR2 |= ADC_CR2_ADON;						// Activate ADC*/


}

void InitTimer()
{
	//	Setup Timer 3 on an interrupt to trigger sample acquisition
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;				// Enable Timer 3
	TIM3->PSC = 1000;	// Set prescaler to fire at sample rate - this is divided by 4 to match the APB2 prescaler
	TIM3->ARR = 30; //SystemCoreClock / 48000 - 1;	// Set maximum count value (auto reload register) - set to system clock / sampling rate

	SET_BIT(TIM3->DIER, TIM_DIER_UIE);				//  DMA/interrupt enable register
	NVIC_EnableIRQ(TIM3_IRQn);
	NVIC_SetPriority(TIM3_IRQn, 0);

	SET_BIT(TIM3->CR1, TIM_CR1_CEN);
	SET_BIT(TIM3->EGR, TIM_EGR_UG);
}