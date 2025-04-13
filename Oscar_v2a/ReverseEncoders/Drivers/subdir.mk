################################################################################
# Automatically-generated file. Do not edit!
# Toolchain: GNU Tools for STM32 (12.3.rel1)
################################################################################

# Add inputs and outputs from these tool invocations to the build variables 
C_SRCS += \
../Drivers/stm32f4xx_flash.c 

C_DEPS += \
./Drivers/stm32f4xx_flash.d 

OBJS += \
./Drivers/stm32f4xx_flash.o 


# Each subdirectory must supply rules for building sources it contributes
Drivers/%.o Drivers/%.su Drivers/%.cyclo: ../Drivers/%.c Drivers/subdir.mk
	arm-none-eabi-gcc -c "$<" -mcpu=cortex-m4 -std=gnu11 -g3 -DSTM32F446xx -DREVERSEENCODERS -c -I../src -I../Drivers/CMSIS/Include -I"D:/Eurorack/Oscar/Oscar446/Drivers/CMSIS/Device/STM32F4xx/Include" -O1 -ffunction-sections -fdata-sections -Wall -fstack-usage -fcyclomatic-complexity -MMD -MP -MF"$(@:%.o=%.d)" -MT"$@" --specs=nano.specs -mfpu=fpv4-sp-d16 -mfloat-abi=hard -mthumb -o "$@"

clean: clean-Drivers

clean-Drivers:
	-$(RM) ./Drivers/stm32f4xx_flash.cyclo ./Drivers/stm32f4xx_flash.d ./Drivers/stm32f4xx_flash.o ./Drivers/stm32f4xx_flash.su

.PHONY: clean-Drivers

