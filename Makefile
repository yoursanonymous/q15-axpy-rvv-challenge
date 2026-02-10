CC_RV64 = riscv64-linux-gnu-gcc
CC_RV32 = riscv64-linux-gnu-gcc
QEMU_RV64 = qemu-riscv64
QEMU_RV32 = qemu-riscv32

COMMON_FLAGS = -O3 -Wall -march=rv64gcv -mabi=lp64d -std=c11
RV32_FLAGS = -O3 -Wall -march=rv32gcv -mabi=ilp32d -std=c11

TARGET_RV64 = q15_axpy_rv64.elf
TARGET_RV32 = q15_axpy_rv32.elf
SOURCES = src/q15_axpy_challenge.c

.PHONY: all all_rv32 all_rv64 run_rv32 run_rv64 clean

all: all_rv64

all_rv64: $(TARGET_RV64)
all_rv32: $(TARGET_RV32)

$(TARGET_RV64): $(SOURCES)
	$(CC_RV64) $(COMMON_FLAGS) $(SOURCES) -o $@ -lm

$(TARGET_RV32): $(SOURCES)
	$(CC_RV32) $(RV32_FLAGS) $(SOURCES) -o $@ -lm

run_rv64: $(TARGET_RV64)
	$(QEMU_RV64) -L /usr/riscv64-linux-gnu -cpu rv64,v=true,vlen=128 ./$(TARGET_RV64)

run_rv32: $(TARGET_RV32)
	$(QEMU_RV32) -cpu rv32,v=true,vlen=128 ./$(TARGET_RV32)

clean:
	rm -f *.elf
