DESCRIPTION = "ADZ1 Driver"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PR = "r6"
PV = "0.1"

inherit module

SRC_URI = "file://Makefile \
           file://adz1.c \
           file://51-adz1.rules \
          "

S = "${WORKDIR}"

FILES_${PN} = " \
	${sysconfdir}/udev/rules.d/51-adz1.rules \
	"

do_install() {
	install -d ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/addon/adz1
	install -m 0644 adz1.ko ${D}${base_libdir}/modules/${KERNEL_VERSION}/kernel/drivers/addon/adz1/adz1.ko
	install -d ${D}${sysconfdir}/udev/rules.d
	install -m 0644 51-adz1.rules ${D}${sysconfdir}/udev/rules.d/51-adz1.rules
}
