################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (9-2020-q2-update)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/stm32f4xx_flash.c 

C_DEPS += \
./Drivers/stm32f4xx_flash.d 

OBJS += \
./Drivers/stm32f4xx_flash.o 


# Each subdirectory must supply rules for building sources it contributes
Drivers/stm32f4xx_flash.o: ../Drivers/stm32f4xx_flash.c Drivers/subdir.mk
	arm-none-eabi-gcc -c "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DSTM32F446xx -c -I../src -I../Drivers/CMSIS/Include -I../Drivers/CMSIS/Device/ST/STM32F4xx/Include -O1 -ffunction-sections -fdata-sections -Wall -fstack-usage -MMD -MP -MF"Drivers/stm32f4xx_flash.d" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

