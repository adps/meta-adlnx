require recipes-core/images/core-image-minimal.bb

IMAGE_INSTALL += "libstdc++ startup mtd-utils"
IMAGE_INSTALL += "openssh openssl openssh-sftp-server"
IMAGE_INSTALL += "adz2"

IMAGE_INSTALL += "device-tree pciutils"
IMAGE_INSTALL += "xserver-xorg xset xrandr libdrm xf86-video-armsoc"
IMAGE_INSTALL += "x11-common xinit libmali-xlnx xauth xinput"
IMAGE_INSTALL += "xf86-input-evdev xf86-input-mouse"

