require recipes-core/images/core-image-minimal.bb

IMAGE_INSTALL += "libstdc++ startup"
IMAGE_INSTALL += "openssh openssl openssh-sftp-server"
IMAGE_INSTALL += "gdb gdbserver"
IMAGE_INSTALL += "procps mtd-utils"
IMAGE_INSTALL += "unzip cifs-utils"
IMAGE_INSTALL += "e2fsprogs-e2fsck e2fsprogs-mke2fs e2fsprogs-tune2fs e2fsprogs"
IMAGE_INSTALL += "device-tree pciutils"
IMAGE_INSTALL += "xserver-xorg xset xrandr libdrm xf86-video-armsoc"
IMAGE_INSTALL += "x11-common xinit libmali-xlnx xauth xinput xeyes"
IMAGE_INSTALL += "xf86-input-evdev xf86-input-mouse xf86-input-keyboard"

IMAGE_FEATURES += " \
    dev-pkgs \
    tools-sdk \
"

EXTRA_IMAGE_FEATURES += " \
    tools-debug \
    tools-profile \
    debug-tweaks \
"

