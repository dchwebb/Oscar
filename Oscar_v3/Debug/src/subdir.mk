################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (13.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
CPP_SRCS += \
../src/MidiEvents.cpp \
../src/configManager.cpp \
../src/fft.cpp \
../src/initialisation.cpp \
../src/lcd.cpp \
../src/main.cpp \
../src/osc.cpp \
../src/tuner.cpp \
../src/uartHandler.cpp \
../src/ui.cpp 

S_SRCS += \
../src/startup_stm32f446retx.s 

C_SRCS += \
../src/syscalls.c \
../src/sysmem.c \
../src/system_stm32f4xx.c 

S_DEPS += \
./src/startup_stm32f446retx.d 

C_DEPS += \
./src/syscalls.d \
./src/sysmem.d \
./src/system_stm32f4xx.d 

OBJS += \
./src/MidiEvents.o \
./src/configManager.o \
./src/fft.o \
./src/initialisation.o \
./src/lcd.o \
./src/main.o \
./src/osc.o \
./src/startup_stm32f446retx.o \
./src/syscalls.o \
./src/sysmem.o \
./src/system_stm32f4xx.o \
./src/tuner.o \
./src/uartHandler.o \
./src/ui.o 

CPP_DEPS += \
./src/MidiEvents.d \
./src/configManager.d \
./src/fft.d \
./src/initialisation.d \
./src/lcd.d \
./src/main.d \
./src/osc.d \
./src/tuner.d \
./src/uartHandler.d \
./src/ui.d 


# Each subdirectory must supply rules for building sources it contributes
src/%.o src/%.su src/%.cyclo: ../src/%.cpp src/subdir.mk
	arm-none-eabi-g++ "$<" -mcpu=cortex-m4 -std=gnu++20 -g3 -DDEBUG -DSTM32F446xx -c -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"D:/Eurorack/Oscar/Oscar_v3/src" -I"D:/Eurorack/Oscar/Oscar_v3/src/USBDevice" -O1 -ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-use-cxa-atexit -Wall -Wno-volatile -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"
src/%.o: ../src/%.s src/subdir.mk
	arm-none-eabi-gcc -mcpu=cortex-m4 -g3 -DDEBUG -c -I"D:/Eurorack/Oscar/Oscar_v3/src" -I"D:/Eurorack/Oscar/Oscar_v3/src/USBDevice" -x assembler-with-cpp -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@" "$<"
src/%.o src/%.su src/%.cyclo: ../src/%.c src/subdir.mk
	arm-none-eabi-gcc "$<" -mcpu=cortex-m4 -std=gnu18 -g3 -DDEBUG -DSTM32F446xx -c -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -I../Drivers/CMSIS/Include -I"D:/Eurorack/Oscar/Oscar_v3/src" -I"D:/Eurorack/Oscar/Oscar_v3/src/USBDevice" -O1 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-src

clean-src:
	-$(RM) ./src/MidiEvents.cyclo ./src/MidiEvents.d ./src/MidiEvents.o ./src/MidiEvents.su ./src/configManager.cyclo ./src/configManager.d ./src/configManager.o ./src/configManager.su ./src/fft.cyclo ./src/fft.d ./src/fft.o ./src/fft.su ./src/initialisation.cyclo ./src/initialisation.d ./src/initialisation.o ./src/initialisation.su ./src/lcd.cyclo ./src/lcd.d ./src/lcd.o ./src/lcd.su ./src/main.cyclo ./src/main.d ./src/main.o ./src/main.su ./src/osc.cyclo ./src/osc.d ./src/osc.o ./src/osc.su ./src/startup_stm32f446retx.d ./src/startup_stm32f446retx.o ./src/syscalls.cyclo ./src/syscalls.d ./src/syscalls.o ./src/syscalls.su ./src/sysmem.cyclo ./src/sysmem.d ./src/sysmem.o ./src/sysmem.su ./src/system_stm32f4xx.cyclo ./src/system_stm32f4xx.d ./src/system_stm32f4xx.o ./src/system_stm32f4xx.su ./src/tuner.cyclo ./src/tuner.d ./src/tuner.o ./src/tuner.su ./src/uartHandler.cyclo ./src/uartHandler.d ./src/uartHandler.o ./src/uartHandler.su ./src/ui.cyclo ./src/ui.d ./src/ui.o ./src/ui.su

.PHONY: clean-src

