#pragma once

class Btn;
extern volatile uint32_t SysTickVal;

class GpioPin {
	friend Btn;

public:
	enum class Type {Input, InputPullup, InputPulldown, Output, AlternateFunction, AlternateFunctionPullUp, AlternateFunctionPullDown};
	enum class DriveStrength {Low, Medium, High, VeryHigh};
	enum class OutputType {PushPull, OpenDrain};


	GpioPin(GPIO_TypeDef* port, uint32_t pin, Type pinType, uint32_t alternateFunction = 0, DriveStrength driveStrength = DriveStrength::Low, OutputType outputType = OutputType::PushPull) :
		port(port), pin(pin), pinType(pinType)
	{
		Init(port, pin, pinType, alternateFunction, driveStrength);		// Init function is static so can be called without instantiating object
	}


	static void Init(GPIO_TypeDef* port, const uint32_t pin, const Type pinType, const uint32_t alternateFunction = 0, const DriveStrength driveStrength = DriveStrength::Low, OutputType outputType = OutputType::PushPull)
	{
		// maths to calculate RCC clock to enable
#if defined(STM32H723xx)
	const uint32_t portPos = ((uint32_t)port - GPIOA_BASE) >> 10;
	RCC->AHB4ENR |= (1 << portPos);
#elif defined(STM32H7B0xx)
	const uint32_t portPos = ((uint32_t)port - SRD_AHB4PERIPH_BASE) >> 10;
	RCC->AHB4ENR |= (1 << portPos);
#elif defined(STM32H563xx)
	const uint32_t portPos = ((uint32_t)port - AHB2PERIPH_BASE_NS) >> 10;
	RCC->AHB2ENR |= (1 << portPos);
#elif defined(STM32G473xx)
	const uint32_t portPos = ((uint32_t)port - AHB2PERIPH_BASE) >> 10;
	RCC->AHB2ENR |= (1 << portPos);
#elif defined(STM32F446xx) or defined(STM32F405xx)
	const uint32_t portPos = ((uint32_t)port - AHB1PERIPH_BASE) >> 10;
	RCC->AHB1ENR |= (1 << portPos);
#endif


		// 00: Input, 01: Output, 10: Alternate function, 11: Analog (reset state)
		if (pinType == Type::Input || pinType == Type::InputPullup || pinType == Type::InputPulldown) {
			port->MODER &= ~(0b11 << (pin * 2));

		} else if (pinType == Type::Output) {
			port->MODER |=  (0b01 << (pin * 2));
			port->MODER &= ~(0b10 << (pin * 2));

		} if (pinType == Type::AlternateFunction || pinType == Type::AlternateFunctionPullUp || pinType == Type::AlternateFunctionPullDown) {
			port->MODER |=  (0b10 << (pin * 2));
			port->MODER &= ~(0b01 << (pin * 2));
			if (pin < 8) {
				port->AFR[0] |= alternateFunction << (pin * 4);
			} else {
				port->AFR[1] |= alternateFunction << ((pin - 8) * 4);
			}
		}

		if (pinType == Type::InputPullup || pinType == Type::AlternateFunctionPullUp) {
			port->PUPDR |= (0b1 << (pin * 2));
		}
		if (pinType == Type::InputPulldown || pinType == Type::AlternateFunctionPullDown) {
			port->PUPDR |= (0b10 << (pin * 2));
		}


		port->OSPEEDR |= static_cast<uint32_t>(driveStrength) << (pin * 2);

		if (outputType == OutputType::OpenDrain) {
			port->OTYPER |= 1 << pin;
		}
	}


	static bool IsHigh(GPIO_TypeDef* port, const uint32_t pin) {
		return (port->IDR & (1 << pin));
	}

	bool IsHigh() {
		return (port->IDR & (1 << pin));
	}

	static bool IsLow(GPIO_TypeDef* port, const uint32_t pin) {
		return ((port->IDR & (1 << pin)) == 0);
	}

	bool IsLow() {
		return ((port->IDR & (1 << pin)) == 0);
	}

	static void SetHigh(GPIO_TypeDef* port, const uint32_t pin) {
		port->ODR |= (1 << pin);
	}

	void SetHigh() {
		port->ODR |= (1 << pin);
	}

	static void SetLow(GPIO_TypeDef* port, const uint32_t pin) {
		port->ODR &= ~(1 << pin);
	}

	void SetLow() {
		port->ODR &= ~(1 << pin);
	}

private:
	GPIO_TypeDef* port;
	uint32_t pin;
	Type pinType;
};


// Struct to manage buttons with debounce and GPIO management
struct Btn {
	GpioPin pin;
	uint32_t down = 0;			// set to zero if button has been pressed and is subsequently released
	uint32_t up = 0;			// Set to last release time
	bool longPressed = false;

	// Debounce button press mechanism
	bool Pressed() {
		// Has been pressed and now released
		if (!ButtonDown() && down && SysTickVal > up + 200) {		// button not pressed
			down = 0;
			up = SysTickVal;
			if (longPressed) {
				longPressed = false;
				return false;
			}
			return true;
		}

		if (ButtonDown() && down == 0 && SysTickVal > up + 200) {	// button pressed
			down = SysTickVal;
			up = 0;
		}
		return false;
	}

	bool LongPress() {
		if (ButtonDown() && !longPressed && down && SysTickVal > down + 1500) {
			longPressed = true;
			return true;
		}
		return false;
	}

	bool ButtonDown() {
		return pin.pinType == GpioPin::Type::InputPullup ? pin.IsLow() : pin.IsHigh();
	}
};
