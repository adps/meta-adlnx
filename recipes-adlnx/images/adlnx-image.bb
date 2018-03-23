require recipes-core/images/core-image-minimal.bb

IMAGE_INSTALL += "libstdc++ adz2 startup mtd-utils"
IMAGE_INSTALL += "openssh openssl openssh-sftp-server"
