DESCRIPTION = "GNU nano (Nano's ANOther editor, or \
Not ANOther editor) is an enhanced clone of the \
Pico text editor."
HOMEPAGE = "http://www.nano-editor.org/"
LICENSE = "GPLv2"
LIC_FILES_CHKSUM = "file://COPYING;md5=f27defe1e96c2e1ecd4e0c9be8967949"
SECTION = "console/utils"
DEPENDS = "ncurses"
RDEPENDS_${PN} = "ncurses-terminfo"

INC_PR = "r3"

PV_MAJOR = "${@d.getVar('PV',1).split('.')[0]}.${@d.getVar('PV',1).split('.')[1]}"

#https://nano-editor.org/dist/v2.2/nano-2.2.5.tar.gz
SRC_URI = "https://nano-editor.org/dist/v${PV_MAJOR}/nano-${PV}.tar.gz"

inherit autotools gettext

PACKAGECONFIG[tiny] = "--enable-tiny,--enable-all,"