require recipes-core/images/core-image-minimal.bb

IMAGE_INSTALL += "libstdc++ adz1 startup"
IMAGE_INSTALL += "openssh openssl openssh-sftp-server"
IMAGE_INSTALL += "gdb gdbserver"
