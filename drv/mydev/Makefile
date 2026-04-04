KERNELDIR ?= /home/chiming/rk3566/kernel
PWD       := $(shell pwd)

CROSS_COMPILE ?= /home/chiming/rk3566/buildroot/output/rockchip_rk3566/host/usr/bin/aarch64-linux-
USER_TOOLCHAIN_ROOT ?= /opt/atk-dlrk356x-toolchain
CC            := $(USER_TOOLCHAIN_ROOT)/bin/aarch64-buildroot-linux-gnu-gcc
SYSROOT       := $(USER_TOOLCHAIN_ROOT)/aarch64-buildroot-linux-gnu/sysroot

obj-m := mydev.o
mydev-objs := src/main.o src/led.o src/sensor.o src/oled.o src/key.o src/servo.o
ccflags-y += -I$(src)/include/mydev

BUILD_DIR := build
USER_CFLAGS := --sysroot=$(SYSROOT) -static -Wall -O2 -I$(PWD)/include/mydev
KEY_TEST_BIN := $(BUILD_DIR)/key_test
KEY_ONCE_BIN := $(BUILD_DIR)/key_once
KEY_DEBUG_BIN := $(BUILD_DIR)/key_debug_test
SENSOR_TEST_BIN := $(BUILD_DIR)/sensor_test

all: module test key_once key_debug sensor_test

module:
	@echo "=== Building kernel modules ==="
	make -C $(KERNELDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) modules
	mkdir -p $(BUILD_DIR)
	cp mydev.ko $(BUILD_DIR)/
	@echo "Kernel module built: $(BUILD_DIR)/mydev.ko"

test: src/key_test.c
	@echo "=== Building user space key_test ==="
	mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -o $(KEY_TEST_BIN) src/key_test.c
	@echo "User program built: $(BUILD_DIR)/key_test"

key_debug: src/key_debug_test.c
	@echo "=== Building user space key_debug_test ==="
	mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -o $(KEY_DEBUG_BIN) src/key_debug_test.c
	@echo "User program built: $(BUILD_DIR)/key_debug_test"

key_once: src/key_once.c
	@echo "=== Building user space key_once ==="
	mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -o $(KEY_ONCE_BIN) src/key_once.c
	@echo "User program built: $(BUILD_DIR)/key_once"

sensor_test: src/sensor_test.c
	@echo "=== Building user space sensor_test ==="
	mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -o $(SENSOR_TEST_BIN) src/sensor_test.c
	@echo "User program built: $(BUILD_DIR)/sensor_test"

clean:
	make -C $(KERNELDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -rf $(BUILD_DIR)

.PHONY: all module test key_once key_debug sensor_test clean
