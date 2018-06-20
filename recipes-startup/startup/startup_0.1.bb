DESCRIPTION = "Startup Script"
SECTION = "base"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"
PR = "r3"
PV = "0.1"

PACKAGES = "${PN}"

SRC_URI = "              \	
   file://startup-script \
   "
   
FILES_${PN} = " \
	${sysconfdir}/init.d \
	${sysconfdir}/rcS.d \
	${sysconfdir}/rc1.d \
	${sysconfdir}/rc2.d \
	${sysconfdir}/rc3.d \
	${sysconfdir}/rc4.d \
	${sysconfdir}/rc5.d \
	${sbindir} \
	${sysconfdir}/init.d/startup-script \
	${sysconfdir}/rcS.d/S90startup-script \
	"

S = "${WORKDIR}"

do_install() {
    install -d ${D}${sysconfdir}/init.d
    install -d ${D}${sysconfdir}/rcS.d
    install -d ${D}${sysconfdir}/rc1.d
    install -d ${D}${sysconfdir}/rc2.d
    install -d ${D}${sysconfdir}/rc3.d
    install -d ${D}${sysconfdir}/rc4.d
    install -d ${D}${sysconfdir}/rc5.d
    install -d ${D}${sbindir}

    install -m 0755 ${WORKDIR}/startup-script  ${D}${sysconfdir}/init.d/
    ln -sf ../init.d/startup-script  ${D}${sysconfdir}/rcS.d/S90startup-script
}
