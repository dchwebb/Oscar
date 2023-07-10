// Main sample capture
void TIM3_IRQHandler(void)
{
	TIM3->SR &= ~TIM_SR_UIF;								// clear UIF flag

	if (ui.config.displayMode == DispMode::Tuner) {
		tuner.Capture();
	} else if (ui.config.displayMode == DispMode::Fourier || ui.config.displayMode == DispMode::Waterfall) {
		fft.Capture();
	} else if (ui.config.displayMode == DispMode::Oscilloscope) {
		osc.Capture();
	}
}


//	Coverage timer
void TIM1_BRK_TIM9_IRQHandler()
{
	TIM9->SR &= ~TIM_SR_UIF;								// clear UIF flag
	++coverageTimer;
}


// MIDI Decoder
void UART4_IRQHandler()
{
	if (UART4->SR | USART_SR_RXNE) {
		midi.queue[midi.queueWrite] = UART4->DR; 			// accessing DR automatically resets the receive flag
		midi.queueCount++;
		midi.queueWrite = (midi.queueWrite + 1) % midi.queueSize;
	}
}


void SysTick_Handler(void)
{
	SysTickVal++;
}
