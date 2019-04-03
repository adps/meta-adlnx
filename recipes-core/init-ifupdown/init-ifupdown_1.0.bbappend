DESCRIPTION = "Updates /etc/network/interfaces to enable both ethernets"
SECTION = "base"
LICENSE = "MIT"
LIC_FILES_CHKSUM = "file://${COMMON_LICENSE_DIR}/MIT;md5=0835ade698e0bcf8506ecda2f7b4f302"

FILESEXTRAPATHS_append := "${THISDIR}/init-ifupdown-1.0"

SRC_URI += "              \	
        file://interfaces \
   "

