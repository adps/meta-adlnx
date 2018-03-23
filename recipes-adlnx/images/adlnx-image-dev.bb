require recipes-core/images/core-image-minimal.bb

IMAGE_INSTALL += "libstdc++ startup"
IMAGE_INSTALL += "openssh openssl openssh-sftp-server"
IMAGE_INSTALL += "gdb gdbserver"
IMAGE_INSTALL += "procps mtd-utils"
IMAGE_INSTALL += "unzip cifs-utils"

IMAGE_FEATURES += " \
    dev-pkgs \
    tools-sdk \
"

EXTRA_IMAGE_FEATURES += " \
    tools-debug \
    tools-profile \
    debug-tweaks \
"

