if [ -z "$QEMU_PATH" ]
then
  echo "Error: \$QEMU_PATH must be set to the path to Xilinx QEMU repo"
fi

mkdir qemu_tmp

$QEMU_PATH/aarch64-softmmu/qemu-system-aarch64 \
  -M arm-generic-fdt -serial mon:stdio -serial /dev/null -display none \
  -device loader,file=./prebuilt/bl31.elf,cpu-num=0 \
  -kernel Image-initramfs-admvpx39z.bin \
  -device loader,file=./prebuilt/qemu_admvpx39z.dtb,addr=0x1407f000 \
  -device loader,file=./prebuilt/linux-boot.elf \
  -gdb tcp::9000 \
  -dtb ./prebuilt/qemu_admvpx39z.dtb \
  -net nic -net nic -net nic -net nic -net user,tftp=./qemu_tftp,hostfwd=tcp::10022-:22 \
  -hw-dtb ./prebuilt/zynqmp-qemu-multiarch-arm.dtb \
  -global xlnx,zynqmp-boot.cpu-num=0 -global xlnx,zynqmp-boot.use-pmufw=true \
  -machine-path ./qemu_tmp \
  -m 4G

