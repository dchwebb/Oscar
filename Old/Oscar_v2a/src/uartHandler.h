#pragma once

#include "initialisation.h"
#include <string>
#include <sstream>
#include <iomanip>


class UARTHandler {
public:
	void Init();
	void ProcessCommand();
	void DataIn(char data);
	size_t SendString(const unsigned char* s, size_t len);
	void SendString(const std::string& s);

private:
	bool cmdPending = false;
	uint32_t cmdPos = 0;
	static constexpr uint32_t maxCmdLen = 64;
	char command[maxCmdLen];
};

extern UARTHandler uart;
