if [ -z "$QEMU_PATH" ]
then
  echo "Error: \$QEMU_PATH must be set to the path to Xilinx QEMU repo"
fi

$QEMU_PATH/microblazeel-softmmu/qemu-system-microblazeel \
  -M microblaze-fdt -display none \
  -kernel ./prebuilt/pmu_rom_qemu_sha3.elf \
  -device loader,file=./prebuilt/pmufw.elf \
  -hw-dtb ./prebuilt/zynqmp-qemu-multiarch-pmu.dtb \
  -machine-path ./qemu_tmp \
  -device loader,addr=0xfd1a0074,data=0x1011003,data-len=4 \
  -device loader,addr=0xfd1a007C,data=0x1010f03,data-len=4

