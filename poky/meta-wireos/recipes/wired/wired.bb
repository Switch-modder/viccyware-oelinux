DESCRIPTION = "Silly wire daemon for training wakeword and setting performance profile"
LICENSE = "Anki-Inc.-Proprietary"                                                                   
LIC_FILES_CHKSUM = "file://${COREBASE}/meta-qti-bsp/files/anki-licenses/\                           
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

SERVICE_FILE = "wired.service"

SRC_URI = "file://${SERVICE_FILE}"

inherit systemd

do_install_append () {
   if ${@bb.utils.contains('DISTRO_FEATURES', 'systemd', 'true', 'false', d)}; then
       install -d ${D}${systemd_unitdir}/system/
       install -m 0644 ${WORKDIR}/${SERVICE_FILE} -D ${D}${systemd_unitdir}/system/${SERVICE_FILE}
   fi
}

FILES_${PN} += "${systemd_unitdir}/system/"
SYSTEMD_SERVICE_${PN} = "${SERVICE_FILE}"

inherit externalsrc

EXTERNALSRC = "${WORKSPACE}/anki/wired"

do_compile() {
    cd "${EXTERNALSRC}"
    make
}

# wired is packaged with upx, so Yocto can't figure out
# what libs it wants, so Yocto chooses to get rid of
# libvector-gobot.so. we must skip QA and stripping

do_strip() {
    :
}

do_package_qa() {
    :
}

do_install () {
    install -d ${D}/usr/bin
    install -d ${D}/usr/lib
    install -d ${D}/etc/wired
    install -p -m 0755 ${WORKSPACE}/anki/wired/build/wired ${D}/usr/bin/
    install -m 0755 ${WORKSPACE}/anki/wired/build/libvector-gobot.so ${D}/usr/lib/libvector-gobot.so
    cp -R --no-dereference --preserve=mode,links -v ${WORKSPACE}/anki/wired/webroot ${D}/etc/wired/webroot
}

FILES_${PN} += "usr/bin/wired"
FILES_${PN} += "/usr/lib/libvector-gobot.so"
FILES_${PN} += "etc/wired/webroot"

FILES_${PN}-dev = ""
INSANE_SKIP_${PN} += "dev-so"
INSANE_SKIP_${PN} += "ldflags"
INHIBIT_PACKAGE_STRIP = "1"
INHIBIT_PACKAGE_DEBUG_SPLIT = '1'
