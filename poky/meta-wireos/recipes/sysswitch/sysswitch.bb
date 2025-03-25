DESCRIPTION = "Simple wrapper for bootctl"
LICENSE = "Anki-Inc.-Proprietary"                                                                   
LIC_FILES_CHKSUM = "file://${COREBASE}/meta-qti-bsp/files/anki-licenses/\                           
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SRC_URI = "file://sysswitch.sh"

do_install () {
    mkdir -p ${D}/sbin
    install -p -m 0755 ${WORKDIR}/sysswitch.sh ${D}/sbin/sysswitch
}

FILES_${PN} += "sbin/sysswitch"
