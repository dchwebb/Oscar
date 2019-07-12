#include <osc.h>

void Osc::setDrawBuffer(uint16_t* buff1, uint16_t* buff2) {
	DrawBuffer[0] = buff1;
	DrawBuffer[1] = buff2;
}
