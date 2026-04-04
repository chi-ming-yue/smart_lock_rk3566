# 开发板内核路径
KERNELDIR ?= /home/chiming/rk3566/kernel
PWD       := $(shell pwd)

# 交叉编译工具链
CROSS_COMPILE ?= /home/chiming/rk3566/buildroot/output/rockchip_rk3566/host/usr/bin/aarch64-linux-
USER_TOOLCHAIN_ROOT ?= /opt/atk-dlrk356x-toolchain
CC            := $(USER_TOOLCHAIN_ROOT)/bin/aarch64-buildroot-linux-gnu-gcc
SYSROOT       := $(USER_TOOLCHAIN_ROOT)/aarch64-buildroot-linux-gnu/sysroot

# 内核模块
obj-m := mydev.o
mydev-objs := main.o led.o sensor.o oled.o key.o servo.o

BUILD_DIR := build
USER_CFLAGS := --sysroot=$(SYSROOT) -static -Wall -O2
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

test: key_test.c
	@echo "=== Building user space key_test ==="
	mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -o $(KEY_TEST_BIN) key_test.c
	cp $(KEY_TEST_BIN) key_test
	@echo "User program built: $(KEY_TEST_BIN)"

key_debug: key_debug_test.c
	@echo "=== Building user space key_debug_test ==="
	mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -o $(KEY_DEBUG_BIN) key_debug_test.c
	cp $(KEY_DEBUG_BIN) key_debug_test
	@echo "User program built: $(KEY_DEBUG_BIN)"

key_once: key_once.c
	@echo "=== Building user space key_once ==="
	mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -o $(KEY_ONCE_BIN) key_once.c
	cp $(KEY_ONCE_BIN) key_once
	@echo "User program built: $(KEY_ONCE_BIN)"

sensor_test: sensor_test.c
	@echo "=== Building user space sensor_test ==="
	mkdir -p $(BUILD_DIR)
	$(CC) $(USER_CFLAGS) -o $(SENSOR_TEST_BIN) sensor_test.c
	cp $(SENSOR_TEST_BIN) sensor_test
	@echo "User program built: $(SENSOR_TEST_BIN)"

clean:
	make -C $(KERNELDIR) M=$(PWD) ARCH=arm64 CROSS_COMPILE=$(CROSS_COMPILE) clean
	rm -rf $(BUILD_DIR)

.PHONY: all module test clean
