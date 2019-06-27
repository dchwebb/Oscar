#include "initialisation.h"

#define USE_HSE
#define PLL_M 8
#define PLL_N 360
#define PLL_P 2		//  Main PLL (PLL) division factor for main system clock can be 2 (PLL_P = 0), 4 (PLL_P = 1), 6 (PLL_P = 2), 8 (PLL_P = 3)
#define PLL_Q 7

void SystemClock_Config(void) {
	uint32_t temp = 0x00000000;

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;			// Enable Power Control clock
	PWR->CR |= PWR_CR_VOS_0;					// Enable VOS voltage scaling - allows maximum clock speed

	SCB->CPACR |= ((3UL << 10*2)|(3UL << 11*2));// CPACR register: set full access privileges for coprocessors

#ifdef USE_HSE
	RCC->CR |= RCC_CR_HSEON;					// HSE ON
	while ((RCC->CR & RCC_CR_HSERDY) == 0);		// Wait till HSE is ready
	RCC->PLLCFGR = PLL_M | (PLL_N << 6) | (((PLL_P >> 1) -1) << 16) | (RCC_PLLCFGR_PLLSRC_HSE) | (PLL_Q << 24);
#endif

#ifdef USE_HSI
	RCC->CR |= RCC_CR_HSION;					// HSI ON
	while((RCC->CR & RCC_CR_HSIRDY) == 0);		// Wait till HSI is ready
    RCC->PLLCFGR = PLL_M | (PLL_N << 6) | (((PLL_P >> 1) -1) << 16) | (RCC_PLLCFGR_PLLSRC_HSI) | (PLL_Q << 24);
#endif

	RCC->CFGR |= RCC_CFGR_HPRE_DIV1;			// HCLK = SYSCLK / 1
	RCC->CFGR |= RCC_CFGR_PPRE2_DIV2;			// PCLK2 = HCLK / 2
	RCC->CFGR |= RCC_CFGR_PPRE1_DIV4;			// PCLK1 = HCLK / 4
	RCC->CR |= RCC_CR_PLLON;					// Enable the main PLL
	while((RCC->CR & RCC_CR_PLLRDY) == 0);		// Wait till the main PLL is ready

	// Configure Flash prefetch, Instruction cache, Data cache and wait state
	FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS;

	// Select the main PLL as system clock source
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;

	// Wait till the main PLL is used as system clock source
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

}

void InitSysTick()
{

	// Register macros found in core_cm4.h
	SysTick->CTRL = 0;									// Disable SysTick
	SysTick->LOAD = 0x1000000 - 1;						// Set reload register to maximum 2^24

	// Set priority of Systick interrupt to least urgency (ie largest priority value)
//	NVIC_SetPriority (SysTick_IRQn, (1 << __NVIC_PRIO_BITS) - 1);

	SysTick->VAL = 0;									// Reset the SysTick counter value

//	SysTick->CTRL |= SysTick_CTRL_CLKSOURCE_Msk;		// Select processor clock: 1 = processor clock; 0 = external clock
//	SysTick->CTRL |= SysTick_CTRL_TICKINT_Msk;			// Enable SysTick interrupt
	SysTick->CTRL |= SysTick_CTRL_ENABLE_Msk;			// Enable SysTick
}

void InitLCDHardware(void) {
	//	Enable GPIO and SPI clocks
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;			// reset and clock control - advanced high performance bus - GPIO port C
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIODEN;			// reset and clock control - advanced high performance bus - GPIO port D
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOFEN;			// reset and clock control - advanced high performance bus - GPIO port F
	RCC->APB2ENR |= RCC_APB2ENR_SPI5EN;

	// Init WRX (Write execution control) pin PD13
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

	// Initialise TX stream - Stream 6 = SPI5_TX; Stream 5 = SPI5_RX; Manual p308
	DMA2_Stream6->CR |= DMA_SxCR_CHSEL;				// 0b111 is DMA_Channel_7
	DMA2_Stream6->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream6->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream6->CR |= DMA_SxCR_DIR_0;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory
	DMA2_Stream6->CR |= DMA_SxCR_PL_1;				// Set to high priority
	DMA2_Stream6->PAR = (uint32_t) &(SPI5->DR);		// Configure the peripheral data register address
}

void InitADC(void) {
	//	Setup Timer 2 to trigger ADC
	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;				// Enable Timer 2 clock
	TIM2->CR2 |= TIM_CR2_MMS_2;						// 100: Compare - OC1REF signal is used as trigger output (TRGO)
	TIM2->PSC = 20 - 1;								// Prescaler
	TIM2->ARR = 50 - 1;								// Auto-reload register (ie reset counter) divided by 100
	TIM2->CCR1 = 50 - 1;							// Capture and compare - ie when counter hits this number PWM high
	TIM2->CCER |= TIM_CCER_CC1E;					// Capture/Compare 1 output enable
	TIM2->CCMR1 |= TIM_CCMR1_OC1M_1 |TIM_CCMR1_OC1M_2;		// 110 PWM Mode 1
	TIM2->CR1 |= TIM_CR1_CEN;

	// Enable ADC1 and GPIO clock sources
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOCEN;
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	// Enable ADC - PC3: ADC123_IN13; PA5: ADC12_IN5; PC1: ADC123_IN11
	GPIOC->MODER |= GPIO_MODER_MODER3;				// Set PC3 to Analog mode (0b11)
	GPIOA->MODER |= GPIO_MODER_MODER5;				// Set PA5 to Analog mode (0b11)
	GPIOA->MODER |= GPIO_MODER_MODER0;				// Set PA0 to Analog mode (0b11)

	ADC1->CR1 |= ADC_CR1_SCAN;						// Activate scan mode
	ADC1->SQR1 = (3 - 1) << 20;						// Number of conversions in sequence (set to 3, getting multiple samples for each channel to average)
	ADC1->SQR3 |= 13 << 0;							// Set IN13 1st conversion in sequence
	ADC1->SQR3 |= 5 << 5;							// Set IN5  2nd conversion in sequence
	ADC1->SQR3 |= 0 << 10;							// Set IN0 3rd conversion in sequence

	// Set to 56 cycles (0b11) sampling speed (SMPR2 Left shift speed 3 x ADC_INx up to input 9; use SMPR1 from 0 for ADC_IN10+)
	// 000: 3 cycles; 001: 15 cycles; 010: 28 cycles; 011: 56 cycles; 100: 84 cycles; 101: 112 cycles; 110: 144 cycles; 111: 480 cycles
	ADC1->SMPR1 |= 0b110 << 9;						// Set speed of IN13
	ADC1->SMPR2 |= 0b110 << 15;						// Set speed of IN5
	ADC1->SMPR1 |= 0b110 << 0;						// Set speed of IN0

	ADC1->CR2 |= ADC_CR2_EOCS;						// The EOC bit is set at the end of each regular conversion. Overrun detection is enabled.
	ADC1->CR2 |= ADC_CR2_EXTEN_0;					// ADC hardware trigger 00: Trigger detection disabled; 01: Trigger detection on the rising edge; 10: Trigger detection on the falling edge; 11: Trigger detection on both the rising and falling edges
	ADC1->CR2 |= ADC_CR2_EXTSEL_1 | ADC_CR2_EXTSEL_2;	// ADC External trigger: 0110 = TIM2_TRGO event

	// Enable DMA - DMA2, Channel 0, Stream 0  = ADC1 (Manual p207)
	ADC1->CR2 |= ADC_CR2_DMA;						// Enable DMA Mode on ADC1
	ADC1->CR2 |= ADC_CR2_DDS;						// DMA requests are issued as long as data are converted and DMA=1
	RCC->AHB1ENR|= RCC_AHB1ENR_DMA2EN;

	DMA2_Stream4->CR &= ~DMA_SxCR_DIR;				// 00 = Peripheral-to-memory
	//DMA2_Stream4->CR |= DMA_SxCR_PL_1;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High
	DMA2_Stream4->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream4->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream4->CR &= ~DMA_SxCR_PINC;				// Peripheral not in increment mode
	DMA2_Stream4->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA2_Stream4->CR |= DMA_SxCR_CIRC;				// circular mode to keep refilling buffer
	DMA2_Stream4->CR &= ~DMA_SxCR_DIR;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory

	DMA2_Stream4->NDTR |= ADC_BUFFER_LENGTH;		// Number of data items to transfer (ie size of ADC buffer)
	DMA2_Stream4->PAR = (uint32_t)(&(ADC1->DR));	// Configure the peripheral data register address
	DMA2_Stream4->M0AR = (uint32_t)(ADC_array);		// Configure the memory address (note that M1AR is used for double-buffer mode)
	DMA2_Stream4->CR &= ~DMA_SxCR_CHSEL;			// channel select to 0 for ADC1

	DMA2_Stream4->CR |= DMA_SxCR_EN;				// Enable DMA2
	ADC1->CR2 |= ADC_CR2_ADON;						// Activate ADC

}

//	Setup Timer 3 on an interrupt to trigger sample acquisition
void InitSampleAcquisition() {
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;				// Enable Timer 3
	TIM3->PSC = 50;									// Set prescaler
	TIM3->ARR = 300; 								// Set auto reload register

	TIM3->DIER |= TIM_DIER_UIE;						// DMA/interrupt enable register
	NVIC_EnableIRQ(TIM3_IRQn);
	NVIC_SetPriority(TIM3_IRQn, 0);					// Lower is higher priority

	TIM3->CR1 |= TIM_CR1_CEN;
	TIM3->EGR |= TIM_EGR_UG;						//  Re-initializes counter and generates update of registers
}

//	Setup Timer 4 to count clock cycles for coverage profiling
void InitCoverageTimer() {
	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;				// Enable Timer
	TIM4->PSC = 10;
	TIM4->ARR = 65535;

	TIM4->DIER |= TIM_DIER_UIE;						// DMA/interrupt enable register
	NVIC_EnableIRQ(TIM4_IRQn);
	NVIC_SetPriority(TIM4_IRQn, 2);					// Lower is higher priority

}

//	Setup Timer 5 to count time between bounces
void InitDebounceTimer() {
	RCC->APB1ENR |= RCC_APB1ENR_TIM5EN;				// Enable Timer
	TIM5->PSC = 10000;
	TIM5->ARR = 65535;
}

void InitEncoders() {
	// Encoder 1: Button on PA7, up/down on PE8 and PE9; Encoder 2: button on PE4, up/down on PE10 and PE11
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;			// reset and clock control - advanced high performance bus - GPIO port C
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOEEN;			// reset and clock control - advanced high performance bus - GPIO port E
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;			// Enable system configuration clock: used to manage external interrupt line connection to GPIOs

	// encoder connections to pull up
	GPIOA->PUPDR |= GPIO_PUPDR_PUPDR7_0;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved
	GPIOE->PUPDR |= GPIO_PUPDR_PUPDR4_0;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved
	GPIOE->PUPDR |= GPIO_PUPDR_PUPDR8_0;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved
	GPIOE->PUPDR |= GPIO_PUPDR_PUPDR9_0;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved
	GPIOE->PUPDR |= GPIO_PUPDR_PUPDR10_0;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved
	GPIOE->PUPDR |= GPIO_PUPDR_PUPDR11_0;			// Set pin to pull up:  01 Pull-up; 10 Pull-down; 11 Reserved


	// configure PA7 button to fire on an interrupt
	SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI7_PA;	// Select Pin PA7 which uses External interrupt 2
	EXTI->RTSR |= EXTI_RTSR_TR7;					// Enable rising edge trigger
	EXTI->FTSR |= EXTI_FTSR_TR7;					// Enable falling edge trigger
	EXTI->IMR |= EXTI_IMR_MR7;						// Activate interrupt using mask register

	// configure PE4 button to fire on an interrupt
	SYSCFG->EXTICR[1] |= SYSCFG_EXTICR2_EXTI4_PE;	// Select Pin PA0 which uses External interrupt 2
	EXTI->RTSR |= EXTI_RTSR_TR4;					// Enable rising edge trigger
	EXTI->FTSR |= EXTI_FTSR_TR4;					// Enable falling edge trigger
	EXTI->IMR |= EXTI_IMR_MR4;						// Activate interrupt using mask register

	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI8_PE;	// Select Pin PE8 which uses External interrupt 3
	EXTI->RTSR |= EXTI_FTSR_TR8;					// Enable rising edge trigger
	EXTI->FTSR |= EXTI_FTSR_TR8;					// Enable falling edge trigger
	EXTI->IMR |= EXTI_IMR_MR8;						// Activate interrupt using mask register

	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI9_PE;	// Select Pin PE9 which uses External interrupt 3
	EXTI->RTSR |= EXTI_FTSR_TR9;					// Enable rising edge trigger
	EXTI->FTSR |= EXTI_FTSR_TR9;					// Enable falling edge trigger
	EXTI->IMR |= EXTI_IMR_MR9;						// Activate interrupt using mask register

	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI10_PE;	// Select Pin PE10 which uses External interrupt 3
	EXTI->RTSR |= EXTI_RTSR_TR10;					// Enable rising edge trigger
	EXTI->FTSR |= EXTI_FTSR_TR10;					// Enable falling edge trigger
	EXTI->IMR |= EXTI_IMR_MR10;						// Activate interrupt using mask register

	SYSCFG->EXTICR[2] |= SYSCFG_EXTICR3_EXTI11_PE;	// Select Pin PE11 which uses External interrupt 3
	EXTI->RTSR |= EXTI_RTSR_TR11;					// Enable rising edge trigger
	EXTI->FTSR |= EXTI_FTSR_TR11;					// Enable falling edge trigger
	EXTI->IMR |= EXTI_IMR_MR11;						// Activate interrupt using mask register

	NVIC_SetPriority(EXTI4_IRQn, 4);				// Lower is higher priority
	NVIC_EnableIRQ(EXTI4_IRQn);
	NVIC_SetPriority(EXTI9_5_IRQn, 4);				// Lower is higher priority
	NVIC_EnableIRQ(EXTI9_5_IRQn);
	NVIC_SetPriority(EXTI15_10_IRQn, 4);			// Lower is higher priority
	NVIC_EnableIRQ(EXTI15_10_IRQn);
}

void InitUART() {
	// PA0  UART4_TX 23 (AF8)
	// PA1  UART4_RX 24 (AF8) ** NB Dev board seems to have something pulling this pin to ground so can't use
	// PC10 UART4_TX 78
	// PC11 UART4_RX 79
	RCC->APB1ENR |= RCC_APB1ENR_UART4EN;			// UART4 clock enable

	GPIOC->MODER |= GPIO_MODER_MODER11_1;			// Set alternate function on PC11
	GPIOC->AFR[1] |= 0b1000 << 12;					// Alternate function on PC11 for UART4_RX is 1000: AF8

	UART4->BRR |= 90 << 4;							// Baud Rate (called USART_BRR_DIV_Mantissa) = APB1 clock (45MHz) / (16 * 31250) = 90
	UART4->CR1 &= ~USART_CR1_M;						// Clear bit to set 8 bit word length
	UART4->CR1 |= USART_CR1_RE;						// Receive enable

	// Set up interrupts
	UART4->CR1 |= USART_CR1_RXNEIE;
	NVIC_SetPriority(UART4_IRQn, 3);				// Lower is higher priority
	NVIC_EnableIRQ(UART4_IRQn);

	UART4->CR1 |= USART_CR1_UE;						// USART Enable

}
