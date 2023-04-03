// Main sample capture
void TIM3_IRQHandler(void)
{
	TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag

	if (ui.displayMode == DispMode::Tuner) {
		tuner.Capture();
	} else if (ui.displayMode == DispMode::Fourier || ui.displayMode == DispMode::Waterfall) {
		fft.Capture();

	} else if (ui.displayMode == DispMode::Oscilloscope) {
		// Average the last four ADC readings to smooth noise
		osc.adcA = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		osc.adcB = ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10];
		osc.adcC = ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11];

		// check if we should start capturing - ie not drawing from the capture buffer and crossed over the trigger threshold (or in free mode)
		if (!osc.capturing && (!osc.drawing || osc.captureBufferNumber != osc.oscBufferNumber) &&
				(osc.triggerTest == nullptr || (osc.bufferSamples > osc.triggerX && osc.oldAdc < osc.triggerY && *osc.triggerTest >= osc.triggerY))) {
			osc.capturing = true;

			if (osc.triggerTest == nullptr) {									// free running mode
				osc.capturePos = 0;
				osc.drawOffset[osc.captureBufferNumber] = 0;
				osc.capturedSamples[osc.captureBufferNumber] = -1;
			} else {
				// calculate the drawing offset based on the current capture position minus the horizontal trigger position
				osc.drawOffset[osc.captureBufferNumber] = osc.capturePos - osc.triggerX;
				if (osc.drawOffset[osc.captureBufferNumber] < 0)
					osc.drawOffset[osc.captureBufferNumber] += lcd.drawWidth;

				osc.capturedSamples[osc.captureBufferNumber] = osc.triggerX - 1;	// used to check if a sample is ready to be drawn
			}
		}

		// if capturing check if write buffer is full and switch to next buffer if so; if not full store current reading
		if (osc.capturing && osc.capturedSamples[osc.captureBufferNumber] == lcd.drawWidth - 1) {
			osc.captureBufferNumber = osc.captureBufferNumber == 1 ? 0 : 1;		// switch the capture buffer
			osc.bufferSamples = 0;			// stores number of samples captured since switching buffers to ensure triggered mode works correctly
			osc.capturing = false;
		}

		// If capturing or buffering samples waiting for trigger store current readings in buffer and increment counters
		if (osc.capturing || !osc.drawing || osc.captureBufferNumber != osc.oscBufferNumber) {

			osc.OscBufferA[osc.captureBufferNumber][osc.capturePos] = osc.adcA;
			osc.OscBufferB[osc.captureBufferNumber][osc.capturePos] = osc.adcB;
			osc.OscBufferC[osc.captureBufferNumber][osc.capturePos] = osc.adcC;
			osc.oldAdc = *osc.triggerTest;

			if (osc.capturePos == lcd.drawWidth - 1) {
				osc.capturePos = 0;
			} else {
				++osc.capturePos;
			}

			osc.capturedSamples[osc.captureBufferNumber]++;
			if (!osc.capturing) {
				++osc.bufferSamples;

				// if trigger point not activating generate a temporary draw buffer
				if (osc.bufferSamples > 1000 && osc.capturePos == 0) {
					osc.captureBufferNumber = osc.captureBufferNumber == 1 ? 0 : 1;		// switch the capture buffer
					osc.bufferSamples = 0;
					osc.drawOffset[osc.captureBufferNumber] = 0;
					osc.noTriggerDraw = true;
				}
			}
		}

	}
}



//	Coverage timer
void TIM1_BRK_TIM9_IRQHandler(void) {
	TIM9->SR &= ~TIM_SR_UIF;									// clear UIF flag
	coverageTimer ++;
}

// MIDI Decoder
void UART4_IRQHandler(void) {
	if (UART4->SR | USART_SR_RXNE) {
		midi.queue[midi.queueWrite] = UART4->DR; 				// accessing DR automatically resets the receive flag
		midi.queueCount++;
		midi.queueWrite = (midi.queueWrite + 1) % midi.queueSize;
	}
}

void SysTick_Handler(void) {
	SysTickVal++;
}
