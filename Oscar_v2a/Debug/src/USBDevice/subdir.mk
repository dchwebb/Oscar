################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/USBDevice/CDCHandler.cpp \
../src/USBDevice/MidiHandler.cpp \
../src/USBDevice/USB.cpp \
../src/USBDevice/USBHandler.cpp 

OBJS += \
./src/USBDevice/CDCHandler.o \
./src/USBDevice/MidiHandler.o \
./src/USBDevice/USB.o \
./src/USBDevice/USBHandler.o 

CPP_DEPS += \
./src/USBDevice/CDCHandler.d \
./src/USBDevice/MidiHandler.d \
./src/USBDevice/USB.d \
./src/USBDevice/USBHandler.d 


# Each subdirectory must supply rules for building sources it contributes
src/USBDevice/%.o src/USBDevice/%.su src/USBDevice/%.cyclo: ../src/USBDevice/%.cpp src/USBDevice/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++20 -g3 -DDEBUG -DSTM32F446xx -c -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"D:/Eurorack/Oscar/Oscar_v2a/src" -I"D:/Eurorack/Oscar/Oscar_v2a/src/USBDevice" -O1 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -Wno-volatile -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-src-2f-USBDevice

clean-src-2f-USBDevice:
	-$(RM) ./src/USBDevice/CDCHandler.cyclo ./src/USBDevice/CDCHandler.d ./src/USBDevice/CDCHandler.o ./src/USBDevice/CDCHandler.su ./src/USBDevice/MidiHandler.cyclo ./src/USBDevice/MidiHandler.d ./src/USBDevice/MidiHandler.o ./src/USBDevice/MidiHandler.su ./src/USBDevice/USB.cyclo ./src/USBDevice/USB.d ./src/USBDevice/USB.o ./src/USBDevice/USB.su ./src/USBDevice/USBHandler.cyclo ./src/USBDevice/USBHandler.d ./src/USBDevice/USBHandler.o ./src/USBDevice/USBHandler.su

.PHONY: clean-src-2f-USBDevice

