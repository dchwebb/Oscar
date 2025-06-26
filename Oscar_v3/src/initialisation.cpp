#include "initialisation.h"

GpioPin debugPin(GPIOC, 10, GpioPin::Type::Output);
volatile ADCValues adc;
float samplingFrequency;

struct PLLDividers {
	uint32_t M;
	uint32_t N;
	uint32_t P;
	uint32_t Q;
};
const PLLDividers mainPLL {4, 180, 2, 7};		// Clock: 8MHz / 4(M) * 180(N) / 2(P) = 180MHz
const PLLDividers saiPLL {6, 144, 4, 0};		// USB:   8MHz / 6(M) * 144(N) / 4(P) = 48MHz

void InitClocks()
{
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;		// enable system configuration clock
	[[maybe_unused]] volatile uint32_t dummy = RCC->APB2ENR & RCC_APB2ENR_SYSCFGEN;		// delay

	RCC->APB1ENR |= RCC_APB1ENR_PWREN;			// Enable Power Control clock
	PWR->CR |= PWR_CR_VOS_0;					// Enable VOS voltage scaling - allows maximum clock speed

	SCB->CPACR |= ((3 << 10 * 2) | (3 << 11 * 2));	// CPACR register: set full access privileges for coprocessors

	RCC->CR |= RCC_CR_HSEON;					// HSE ON
	while ((RCC->CR & RCC_CR_HSERDY) == 0);		// Wait till HSE is ready

	RCC->PLLCFGR = 	(mainPLL.M << RCC_PLLCFGR_PLLM_Pos) |
					(mainPLL.N << RCC_PLLCFGR_PLLN_Pos) |
					(((mainPLL.P >> 1) - 1) << RCC_PLLCFGR_PLLP_Pos) |
					(mainPLL.Q << RCC_PLLCFGR_PLLQ_Pos) |
					RCC_PLLCFGR_PLLSRC_HSE;

	RCC->CFGR |= RCC_CFGR_HPRE_DIV1 |			// HCLK = SYSCLK / 1
				 RCC_CFGR_PPRE1_DIV4 |			// PCLK1 = HCLK / 4 (APB1)
				 RCC_CFGR_PPRE2_DIV2;			// PCLK2 = HCLK / 2 (APB2)

	RCC->CR |= RCC_CR_PLLON;					// Enable the main PLL
	while ((RCC->CR & RCC_CR_PLLRDY) == 0);		// Wait till the main PLL is ready

	// PLLSAI used for USB
	RCC->PLLSAICFGR = (saiPLL.M << RCC_PLLSAICFGR_PLLSAIM_Pos) |
					  (saiPLL.N << RCC_PLLSAICFGR_PLLSAIN_Pos) |
					  (((saiPLL.P >> 1) - 1) << RCC_PLLSAICFGR_PLLSAIP_Pos);

	RCC->CR |= RCC_CR_PLLSAION;					// Enable the SAI PLL for USB

	// Configure Flash prefetch, Instruction cache, Data cache and wait state
	FLASH->ACR = FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN | FLASH_ACR_LATENCY_5WS;

	// Select the main PLL as system clock source
	RCC->CFGR &= ~RCC_CFGR_SW;
	RCC->CFGR |= RCC_CFGR_SW_PLL;
	while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL);

	SystemCoreClockUpdate();					// Update SystemCoreClock variable
}


void InitHardware()
{
	InitClocks();									// Activates floating point coprocessor and resets clock
	InitBackupRAM();
	InitSysTick();
	InitLCDHardware();
	InitADC();
	InitEncoders();
	InitMIDIUART();
}


void InitBackupRAM()
{
	RCC->APB1ENR |= RCC_APB1ENR_PWREN;				// Enable the power interface clock
	PWR->CR |= PWR_CR_DBP;							// Enable access to the	backup domain
	RCC->AHB1ENR |= RCC_AHB1ENR_BKPSRAMEN;			// Enable the backup SRAM clock
}


void InitSysTick()
{
	SysTick_Config(SystemCoreClock / sysTickInterval);	// gives 1ms
	NVIC_SetPriority(SysTick_IRQn, 1);
}


void InitWatchdog()
{
	IWDG->KR = 0x5555;								// Enable access to prescaler register
	IWDG->PR = 0b011;								// divider /32 gives around 4 seconds with default reload of 0xFFF
	IWDG->KR = 0xCCCC;								// Start watchdog
}


void ResetWatchdog()
{
	IWDG->KR = 0xAAAA;								// Triggers reload of RL register into counter
}


void InitLCDHardware()
{
	RCC->APB1ENR |= RCC_APB1ENR_SPI3EN;				//	Enable SPI clocks

	GpioPin::Init(GPIOB, 5, GpioPin::Type::AlternateFunction, 6, GpioPin::DriveStrength::VeryHigh);		// SPI_MOSI AF6
	GpioPin::Init(GPIOB, 3, GpioPin::Type::AlternateFunction, 6, GpioPin::DriveStrength::VeryHigh);		// SPI_SCK AF6

	// Clock is APB1 at 45MHz divided by prescaler
	// ILI9341 data sheet specifies maximum frequency of 10MHz but it seems to work fine up to 40MHz+
	// Works at 22MHz (/4), but uses around 7mA additional current
	SPI3->CR1 |= (SPI_CR1_SSM |						// Software slave management: When SSM bit is set, NSS pin input is replaced with the value from the SSI bit
				  SPI_CR1_SSI |						// Internal slave select
				  SPI_CR1_BR_0 |					// Baud rate control prescaler: 0b001: fPCLK/4; 0b000: fPCLK/2
				  SPI_CR1_MSTR |					// Master selection
				  SPI_CR1_SPE);						// Enable SPI

	RCC->AHB1ENR |= RCC_AHB1ENR_DMA1EN;				// Configure DMA

	// Initialise TX stream - Stream 5 = SPI3_TX; Channel 0; Manual p207
	DMA1_Stream5->CR &= ~DMA_SxCR_CHSEL;			// 0 is DMA_Channel_0
	DMA1_Stream5->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Stream5->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA1_Stream5->CR |= DMA_SxCR_DIR_0;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory
	DMA1_Stream5->CR |= DMA_SxCR_PL_1;				// Set to high priority
	DMA1_Stream5->PAR = (uint32_t) &(SPI3->DR);		// Configure the peripheral data register address
}


void InitAdcPins(ADC_TypeDef* ADC_No, std::initializer_list<uint8_t> channels)
{
	uint8_t sequence = 1;

	for (auto channel: channels) {
		// Set conversion sequence to order ADC channels are passed to this function
		if (sequence < 7) {
			ADC_No->SQR3 |= channel << ((sequence - 1) * 5);
		} else if (sequence < 13) {
			ADC_No->SQR2 |= channel << ((sequence - 7) * 5);
		} else {
			ADC_No->SQR1 |= channel << ((sequence - 13) * 5);
		}

		// 000: 3 cycles, 001: 15 cycles, 010: 28 cycles, 011: 56 cycles, 100: 84 cycles, 101: 112 cycles, 110: 144 cycles, 111: 480 cycles
		if (channel < 10)
			ADC_No->SMPR2 |= 0b010 << (3 * channel);
		else
			ADC_No->SMPR1 |= 0b010 << (3 * (channel - 10));

		sequence++;
	}
}

void InitADC()
{
	//	Setup Timer 2 to trigger ADC
	RCC->APB2ENR |= RCC_APB2ENR_TIM8EN;				// Enable Timer clock
	TIM8->CR2 |= TIM_CR2_MMS_2;						// 100: Compare - OC1REF signal is used as trigger output (TRGO)
	TIM8->PSC = 20 - 1;								// Prescaler
	TIM8->ARR = 50 - 1;								// Auto-reload register (ie reset counter) divided by 100
	TIM8->CCR1 = 50 - 1;							// Capture and compare - ie when counter hits this number PWM high
	TIM8->CCER |= TIM_CCER_CC1E;					// Capture/Compare 1 output enable
	TIM8->CCMR1 |= TIM_CCMR1_OC1M_1 |TIM_CCMR1_OC1M_2;		// 110 PWM Mode 1
	TIM8->CR1 |= TIM_CR1_CEN;

	// Enable ADC1 and GPIO clock sources
	RCC->AHB1ENR |= RCC_AHB1ENR_GPIOAEN;
	RCC->APB2ENR |= RCC_APB2ENR_ADC1EN;

	// Enable ADC - PA7: ADC12_IN7; PA6: ADC12_IN6; PA5: ADC12_IN5
	GPIOA->MODER |= GPIO_MODER_MODER5 | GPIO_MODER_MODER6 | GPIO_MODER_MODER7;	// Set PA5, PA6, PA7 to Analog mode (0b11)

	ADC1->CR1 |= ADC_CR1_SCAN;						// Activate scan mode
	ADC1->SQR1 = (3 - 1) << ADC_SQR1_L_Pos;			// Number of conversions in sequence (array is 4 * size of ADC and values averaged)
	InitAdcPins(ADC1, {7, 6, 5});

	ADC1->CR2 |= ADC_CR2_EOCS |						// The EOC bit is set at the end of each regular conversion. Overrun detection is enabled.
				ADC_CR2_EXTEN_0 |					// ADC hardware trigger 00: Trigger detection disabled; 01: Trigger detection on the rising edge; 10: Trigger detection on the falling edge; 11: Trigger detection on both the rising and falling edges
				ADC_CR2_EXTSEL_1 | ADC_CR2_EXTSEL_2 | ADC_CR2_EXTSEL_3;	// ADC External trigger: 1110 = TIM8_TRGO event

	// Enable DMA - DMA2, Channel 0, Stream 0  = ADC1 (Manual p207)
	ADC1->CR2 |= ADC_CR2_DMA;						// Enable DMA Mode on ADC1
	ADC1->CR2 |= ADC_CR2_DDS;						// DMA requests are issued as long as data are converted and DMA=1
	RCC->AHB1ENR|= RCC_AHB1ENR_DMA2EN;

	DMA2_Stream4->CR &= ~DMA_SxCR_DIR;				// 00 = Peripheral-to-memory
	DMA2_Stream4->CR |= DMA_SxCR_PL_1;				// Priority: 00 = low; 01 = Medium; 10 = High; 11 = Very High
	DMA2_Stream4->CR |= DMA_SxCR_PSIZE_0;			// Peripheral size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream4->CR |= DMA_SxCR_MSIZE_0;			// Memory size: 8 bit; 01 = 16 bit; 10 = 32 bit
	DMA2_Stream4->CR &= ~DMA_SxCR_PINC;				// Peripheral not in increment mode
	DMA2_Stream4->CR |= DMA_SxCR_MINC;				// Memory in increment mode
	DMA2_Stream4->CR |= DMA_SxCR_CIRC;				// circular mode to keep refilling buffer
	DMA2_Stream4->CR &= ~DMA_SxCR_DIR;				// data transfer direction: 00: peripheral-to-memory; 01: memory-to-peripheral; 10: memory-to-memory

	DMA2_Stream4->NDTR |= ADC_BUFFER_LENGTH;		// Number of data items to transfer (ie size of ADC buffer)
	DMA2_Stream4->PAR = (uint32_t)(&(ADC1->DR));	// Configure the peripheral data register address
	DMA2_Stream4->M0AR = (uint32_t)(&adc);			// Configure the memory address (note that M1AR is used for double-buffer mode)
	DMA2_Stream4->CR &= ~DMA_SxCR_CHSEL;			// channel select to 0 for ADC1

	DMA2_Stream4->CR |= DMA_SxCR_EN;				// Enable DMA2
	ADC1->CR2 |= ADC_CR2_ADON;						// Activate ADC
}


void InitSampleAcquisition()
{
	//	Setup Timer 3 on an interrupt to trigger sample acquisition
	RCC->APB1ENR |= RCC_APB1ENR_TIM3EN;				// Enable Timer 3
	TIM3->PSC = 0;									// Set prescaler: 90MHz (APB1 Timer Clock) / 1 (PSC + 1) = 90 MHz
	SetSampleTimer(7000);							// Set auto reload register (90 MHz / 7000 = 12 kHz)

	TIM3->DIER |= TIM_DIER_UIE;						// DMA/interrupt enable register
	NVIC_EnableIRQ(TIM3_IRQn);
	NVIC_SetPriority(TIM3_IRQn, 0);					// Lower is higher priority

	TIM3->CR1 |= TIM_CR1_CEN;
	TIM3->EGR |= TIM_EGR_UG;						// Re-initializes counter and generates update of registers
}


uint32_t GetSampleTimer()
{
	return TIM3->ARR * (TIM3->PSC + 1);
}


void SetSampleTimer(uint32_t val)
{
	// as the counter (ARR) is limited to 16 bits, increment the prescaler to get higher multiples (ie longer sample intervals)
	static constexpr uint32_t maxPrescaler = 5;
	val = std::min(val, (uint32_t)(maxPrescaler * 65536 - 1));
	for (uint32_t i = 0; i < maxPrescaler; ++i) {
		if (val < (i + 1) * 65536) {
			TIM3->PSC = i;
			TIM3->ARR = val / (i + 1);
		}
	}

	samplingFrequency = static_cast<float>(SystemCoreClock) / (2.0f * (val + 1));
}


void RunSampleTimer(bool on)
{
	if (on) 	TIM3->CR1 |= TIM_CR1_CEN;
	else		TIM3->CR1 &= ~TIM_CR1_CEN;
}

void InitEncoders()
{
	RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;			// Enable system configuration clock: used to manage external interrupt line connection to GPIOs

	// L Encoder using timer functionality - PB8 and PB9
	GpioPin::Init(GPIOB, 8, GpioPin::Type::AlternateFunctionPullUp, 1);	 // Alternate function 1 is TIM2_CH1
	GpioPin::Init(GPIOB, 9, GpioPin::Type::AlternateFunctionPullUp, 1);	 // Alternate function 1 is TIM2_CH2

	RCC->APB1ENR |= RCC_APB1ENR_TIM2EN;				// Enable Timer 2
	TIM2->PSC = 0;									// Set prescaler
	TIM2->ARR = 0xFFFF; 							// Set auto reload register to max
	TIM2->SMCR |= TIM_SMCR_SMS_0 |TIM_SMCR_SMS_1;	// SMS=011 for counting on both TI1 and TI2 edges
	TIM2->SMCR |= TIM_SMCR_ETF;						// Enable digital filter
	TIM2->CNT = 32000;								// Start counter at mid way point
	TIM2->CR1 |= TIM_CR1_CEN;

	// R Encoder using timer functionality - PB6 and PB7
	GpioPin::Init(GPIOB, 6, GpioPin::Type::AlternateFunctionPullUp, 2);	 // Alternate function 2 is TIM4_CH1
	GpioPin::Init(GPIOB, 7, GpioPin::Type::AlternateFunctionPullUp, 2);	 // Alternate function 2 is TIM4_CH2

	RCC->APB1ENR |= RCC_APB1ENR_TIM4EN;				// Enable Timer 4
	TIM4->PSC = 0;									// Set prescaler
	TIM4->ARR = 0xFFFF; 							// Set auto reload register to max
	TIM4->SMCR |= TIM_SMCR_SMS_0 |TIM_SMCR_SMS_1;	// SMS=011 for counting on both TI1 and TI2 edges
	TIM4->SMCR |= TIM_SMCR_ETF;						// Enable digital filter
	TIM4->CNT = 32000;								// Start counter at mid way point
	TIM4->CR1 |= TIM_CR1_CEN;
}


void InitMIDIUART()
{
	RCC->APB1ENR |= RCC_APB1ENR_UART4EN;			// UART4 clock enable

	GpioPin::Init(GPIOA, 1, GpioPin::Type::AlternateFunction, 8);	 // PA1 Alternate function 8 is UART4_RX

	const uint32_t Baud = (SystemCoreClock / 4) / (16 * 31250);
	UART4->BRR |= Baud << 4;						// Baud Rate (called USART_BRR_DIV_Mantissa) = (Sys Clock: 180MHz / APB1 Prescaler DIV4: 45MHz) / (16 * 31250) = 90
	UART4->CR1 &= ~USART_CR1_M;						// Clear bit to set 8 bit word length
	UART4->CR1 |= USART_CR1_RE;						// Receive enable

	// Set up interrupts
	UART4->CR1 |= USART_CR1_RXNEIE;
	NVIC_SetPriority(UART4_IRQn, 3);				// Lower is higher priority
	NVIC_EnableIRQ(UART4_IRQn);

	UART4->CR1 |= USART_CR1_UE;						// USART Enable
}


void EnableMidiUart(bool on)
{
	if (on) 	UART4->CR1 |= USART_CR1_UE;
	else		UART4->CR1 &= ~USART_CR1_UE;
}


void DelayMS(uint32_t ms)
{
	// Crude delay system
	const uint32_t now = SysTickVal;
	while (now + ms > SysTickVal) {};
}


void JumpToBootloader()
{
	*reinterpret_cast<unsigned long *>(BKPSRAM_BASE) = 0xDEADBEEF; 	// Use Backup SRAM for DFU flag as this is not cleared at restart

	__disable_irq();

	FLASH->ACR &= ~FLASH_ACR_DCEN;					// Disable data cache

	// Not sure why but seem to need to write this value twice or gets lost - caching issue?
	*reinterpret_cast<unsigned long *>(BKPSRAM_BASE) = 0xDEADBEEF;

	__DSB();
	NVIC_SystemReset();

	while (1) {
		// Code should never reach this loop
	}

}


void Reboot()
{
	__disable_irq();
	__DSB();

	SysTick->CTRL = 0;							// Disable Systick timer

	// Disable peripheral clocks
	RCC->AHB1ENR = 0;
	RCC->APB2ENR = 0;
	RCC->AHB3ENR = 0;

	for (uint32_t i = 0; i < 5; ++i) {			// Clear Interrupt Enable Register & Interrupt Pending Register
		NVIC->ICER[i] = 0xFFFFFFFF;
		NVIC->ICPR[i] = 0xFFFFFFFF;
	}

	NVIC_SystemReset();
}
