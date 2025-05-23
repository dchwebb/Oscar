// Main sample capture
void TIM3_IRQHandler()
{
	TIM3->SR &= ~TIM_SR_UIF;								// clear UIF flag

	if (ui.cfg.displayMode == DispMode::Tuner) {
		tuner.Capture();
	} else if (ui.cfg.displayMode == DispMode::Fourier || ui.cfg.displayMode == DispMode::Waterfall) {
		fft.Capture();
	} else if (ui.cfg.displayMode == DispMode::Oscilloscope) {
		osc.Capture();
	}
}


void UART4_IRQHandler()
{
	// MIDI UART Decoder
	if (UART4->SR | USART_SR_RXNE) {
		midi.queue[midi.queueWrite] = UART4->DR; 			// accessing DR automatically resets the receive flag
		midi.queueCount++;
		midi.queueWrite = (midi.queueWrite + 1) % midi.queueSize;
	}
}


void OTG_FS_IRQHandler(void)
{
	usb.InterruptHandler();
}

void SysTick_Handler(void)
{
	++SysTickVal;
}

void NMI_Handler(void) {}
void HardFault_Handler(void) {
	while (1) {}
}
void MemManage_Handler(void) {
	while (1) {}
}
void BusFault_Handler(void) {
	while (1) {}
}
void UsageFault_Handler(void) {
	while (1) {}
}
void SVC_Handler(void) {}
void DebugMon_Handler(void) {}
void PendSV_Handler(void) {}
