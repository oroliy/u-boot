# SPDX-License-Identifier: GPL-2.0+
#
# (C) Copyright 2016 Nexell
# Hyunseok, Jung <hsjung@nexell.co.kr>

obj-y				+= clock.o
obj-y				+= timer.o
ifneq ($(CONFIG_ARM64),y)
obj-y				+= reset-cpu.o
endif
obj-y				+= reset.o
obj-y				+= nx_gpio.o
obj-y				+= tieoff.o
obj-$(CONFIG_ARM64)		+= init.o
obj-$(CONFIG_ARM64)		+= mmu-arm64.o
obj-$(CONFIG_ARM64)		+= ../cpu/armv7/s5p-common/pwm.o
obj-$(CONFIG_ARCH_S5P4418)	+= reg-call.o
obj-$(CONFIG_ARCH_S5P4418)	+= nx_sec_reg.o
