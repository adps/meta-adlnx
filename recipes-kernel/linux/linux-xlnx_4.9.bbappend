LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS_append := "${THISDIR}/files"


SRC_URI += " \
	file://adlnx-image.cfg \
	file://displayport-timeout-fix.patch \
	"

