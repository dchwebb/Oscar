// Main sample capture
void TIM3_IRQHandler(void)
{
	TIM3->SR &= ~TIM_SR_UIF;					// clear UIF flag

	if (displayMode == Fourier || displayMode == Waterfall) {
		if (capturePos == fft.samples && fft.capturing) {
			fft.dataAvailable[fft.captureBufferIndex] = true;
			fft.capturing = false;
		}

		if (fft.capturing) {
			// For FFT Mode we want a value between +- 2047
			if (fft.channel == channelA)
				fft.fftBuffer[fft.captureBufferIndex][capturePos] = 2047.0f - ((float)(ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9]) / 4.0f);
			else if (fft.channel == channelB)
				fft.fftBuffer[fft.captureBufferIndex][capturePos] = 2047.0f - ((float)(ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10]) / 4.0f);
			else if (fft.channel == channelC)
				fft.fftBuffer[fft.captureBufferIndex][capturePos] = 2047.0f - ((float)(ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11]) / 4.0f);
			capturePos ++;
		}

	} else if (displayMode == Oscilloscope) {
		// Average the last four ADC readings to smooth noise
		adcA = ADC_array[0] + ADC_array[3] + ADC_array[6] + ADC_array[9];
		adcB = ADC_array[1] + ADC_array[4] + ADC_array[7] + ADC_array[10];
		adcC = ADC_array[2] + ADC_array[5] + ADC_array[8] + ADC_array[11];

		// check if we should start capturing - ie not drawing from the capture buffer and crossed over the trigger threshold (or in free mode)
		if (!osc.capturing && (!drawing || captureBufferNumber != osc.oscBufferNumber) && (osc.triggerTest == nullptr || (osc.bufferSamples > osc.triggerX && oldAdc < osc.triggerY && *osc.triggerTest >= osc.triggerY))) {
			osc.capturing = true;

			if (osc.triggerTest == nullptr) {									// free running mode
				capturePos = 0;
				osc.drawOffset[captureBufferNumber] = 0;
				osc.capturedSamples[captureBufferNumber] = -1;
			} else {
				// calculate the drawing offset based on the current capture position minus the horizontal trigger position
				osc.drawOffset[captureBufferNumber] = capturePos - osc.triggerX;
				if (osc.drawOffset[captureBufferNumber] < 0)
					osc.drawOffset[captureBufferNumber] += lcd.drawWidth;

				osc.capturedSamples[captureBufferNumber] = osc.triggerX - 1;	// used to check if a sample is ready to be drawn
			}
		}

		// if capturing check if write buffer is full and switch to next buffer if so; if not full store current reading
		if (osc.capturing && osc.capturedSamples[captureBufferNumber] == lcd.drawWidth - 1) {
			captureBufferNumber = captureBufferNumber == 1 ? 0 : 1;		// switch the capture buffer
			osc.bufferSamples = 0;			// stores number of samples captured since switching buffers to ensure triggered mode works correctly
			osc.capturing = false;
		}

		// If capturing or buffering samples waiting for trigger store current readings in buffer and increment counters
		if (osc.capturing || !drawing || captureBufferNumber != osc.oscBufferNumber) {

			osc.OscBufferA[captureBufferNumber][capturePos] = adcA;
			osc.OscBufferB[captureBufferNumber][capturePos] = adcB;
			osc.OscBufferC[captureBufferNumber][capturePos] = adcC;
			oldAdc = *osc.triggerTest;

			if (capturePos == lcd.drawWidth - 1)	capturePos = 0;
			else								capturePos++;

			osc.capturedSamples[captureBufferNumber]++;
			if (!osc.capturing) {
				osc.bufferSamples++;

				// if trigger point not activating generate a temporary draw buffer
				if (osc.bufferSamples > 1000 && capturePos == 0) {
					captureBufferNumber = captureBufferNumber == 1 ? 0 : 1;		// switch the capture buffer
					osc.bufferSamples = 0;
					osc.drawOffset[captureBufferNumber] = 0;
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
		midi.Queue[midi.QueueWrite] = UART4->DR; 				// accessing DR automatically resets the receive flag
		midi.QueueSize++;
		midi.QueueWrite = (midi.QueueWrite + 1) % MIDIQUEUESIZE;
	}
}

void SysTick_Handler(void) {
	SysTickVal++;
}
