DESCRIPTION = "Snowboy daemon"
LICENSE = "Anki-Inc.-Proprietary"                                                                   
LIC_FILES_CHKSUM = "file://${COREBASE}/meta-qti-bsp/files/anki-licenses/\                           
Anki-Inc.-Proprietary;md5=4b03b8ffef1b70b13d869dbce43e8f09"

# EXPLANATION:
# Kitt-AI only provided hard-float-linking libraries for snowboy.
# So, the only way to use it in our softfp-linking OS is to statically
# compile it into a server program and have vic-anim comm with it over IPC.
# We are just copying in something made with a much newer gnueabihf toolchain.


SRC_URI = "file://sb_server"

do_install () {
    install -d ${D}/anki/bin
    install -p -m 0755 ${WORKDIR}/sb_server ${D}/anki/bin/sb_server
}

FILES_${PN} += "anki/bin/sb_server"
