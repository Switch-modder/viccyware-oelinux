#!/bin/bash

set -e

if [[  -d ../anki/anki-ble ]]; then
    cd ..
fi

if [[ ! -d anki/anki-ble ]]; then
    echo "Error. This script must be run in the vicos-oelinux folder, as ./build/docker-ota-build.sh"
    exit 1
fi

function usage() {
	if [[ ${DO_SIGN} == "1" ]]; then
		usagesign
	else
		usagenosign
	fi
}

function usagenosign() {
    echo "Usage: ./build/docker-ota-build.sh <oskr/dev> <incremental> <oskr boot image signing password>"
    echo "example: ./build/docker-ota-build.sh dev 6079 <password> <password>"
    exit 0
}

function usagesign() {
    echo "Usage: ./build/docker-ota-build.sh <prod/oskr/dev> <incremental> <ota_key_password> <prod/oskr boot image signing password>"
    echo "example: ./build/docker-ota-build.sh prod 6079 <password> <password>"
    exit 0
}

if [[ $1 == *"h"* ]]; then
    usage
fi

if [[ $1 != "prod" && $1 != "oskr" && $1 != "ep" && $1 != "dev" ]]; then
    usage
fi

if [[ ! $2 =~ ^-?[0000-9999]+$ ]]; then
    usage
fi

if [[ ${DO_SIGN} == "1" ]]; then
	if [[ $3 == "" ]]; then
		usage
	fi

	if [[ $4 == "" && $1 != "dev" ]]; then
		usage
	fi
        PRODorOSKR=$1
        INCREMENT=$2
	OTA_SIGNING_PASSWORD=$3
        BOOT_SIGNING_PASSWORD=$4
else
	if [[ $3 == "" && $1 != "dev" ]]; then
		usage
	fi
	PRODorOSKR=$1
	INCREMENT=$2
	BOOT_SIGNING_PASSWORD=$3
fi

function check_sign_prod() {
    if openssl rsa -in ota/qtipri.encrypted.key -passin pass:"$BOOT_SIGNING_PASSWORD" -noout 2>/dev/null; then
        echo "Prod boot image key password confirmed to be correct!"
    else
        echo
        echo -e "\033[1;31mProd boot image signing password is incorrect. exiting.\033[0m"
        echo -e "\033[1;31mHINT: we are using an older version of the key which has the same password as the ABOOT key\033[0m"
        echo
        exit 1
    fi
}

function check_sign_oskr() {
    if openssl rsa -in ota/qtioskrpri.encrypted.key -passin pass:"$BOOT_SIGNING_PASSWORD" -noout 2>/dev/null; then
        echo "OSKR boot image key password confirmed to be correct!"
    else
        echo
        echo -e "\033[1;31mOSKR boot image signing password is incorrect. exiting.\033[0m"
        echo
        exit 1
    fi
}

if [[ ${PRODorOSKR} == "prod" ]]; then
    echo "Building a prod image"
    ToDo="build-victor-robot-user-image && export IMG_DIR=../poky/build/tmp-glibc/deploy/images/apq8009-robot-robot-perf && cd ~/vicos-oelinux/ota && export BOOT_IMAGE_SIGNING_PASSWORD=$BOOT_SIGNING_PASSWORD && make prodsign"
    ADEV=0
    check_sign_prod
    OTA_NAME=Viccyware-$(cat ANKI_VERSION).${INCREMENT}.ota
    PERForUSER="-user"
elif [[ ${PRODorOSKR} == "ep" ]]; then
    echo "Building an EP image"
    ToDo="build-victor-robot-ep-image && export IMG_DIR=../poky/build/tmp-glibc/deploy/images/apq8009-robot-robot-perf && cd ~/vicos-oelinux/ota && export BOOT_IMAGE_SIGNING_PASSWORD=$BOOT_SIGNING_PASSWORD && make prodsign"
    ADEV=0
    check_sign_prod
    OTA_NAME=Viccyware-$(cat ANKI_VERSION).${INCREMENT}ep.ota
    PERForUSER="-user"
elif [[ ${PRODorOSKR} == "dev" ]]; then
    echo "Building a dev image"
    ToDo="build-victor-robot-perf-image && export IMG_DIR=../poky/build/tmp-glibc/deploy/images/apq8009-robot-robot-perf && cd ~/vicos-oelinux/ota && export BOOT_IMAGE_SIGNING_PASSWORD=$BOOT_SIGNING_PASSWORD && make devsign"
    ADEV=1
    OTA_NAME=Viccyware-B${INCREMENT}-$(cat ANKI_VERSION)d.ota
    PERForUSER="-perf"
else
    echo "Building an OSKR image"
    ToDo="build-victor-robot-oskr-image && export IMG_DIR=../poky/build/tmp-glibc/deploy/images/apq8009-robot-robot-perf && cd ~/vicos-oelinux/ota && export BOOT_IMAGE_SIGNING_PASSWORD=$BOOT_SIGNING_PASSWORD && make oskrsign"
    ADEV=1
    check_sign_oskr
    OTA_NAME=Viccyware-B${INCREMENT}-$(cat ANKI_VERSION)oskr.ota
    PERForUSER="-perf"
fi

#&& bitbake -c clean victor && bitbake -c cleansstate victor

if [[ ${DO_SIGN} == "1" ]]; then
	if openssl rsa -in ota/ota_prod.key -passin pass:"$OTA_SIGNING_PASSWORD" -noout 2>/dev/null; then
		echo "OTA signing key password is confirmed to be correct!"
	else
		echo
		echo -e "\033[1;31mOTA signing key is incorrect. exiting.\033[0m"
		echo
		exit 1
	fi
fi

BUILD_ARGS_TERM="-it"
if [[ ${NO_TTY} == "1" ]]; then
	BUILD_ARGS_TERM="-t"
fi

docker build --build-arg UID=$(id -u $USER) --build-arg GID=$(id -g $USER) -t victor-builder build/

docker run ${BUILD_ARGS_TERM} \
	-v "$(pwd):/home/build/vicos-oelinux" \
	victor-builder bash -c "cd ~/vicos-oelinux && ./build/deps.sh"

docker run ${BUILD_ARGS_TERM} \
    -v "$(pwd):/home/build/vicos-oelinux" \
    -v "$(pwd)/anki-deps:/home/build/.anki" \
    -v "$(pwd)/build/cache/ccache:/home/build/.ccache" \
    victor-builder bash -c "cd ~/vicos-oelinux/poky && rm -rf build/tmp-glibc/deploy/images/apq8009-robot-robot-perf/* && source build/conf/set_bb_env.sh && export ANKI_BUILD_VERSION=$INCREMENT && source ~/vicos-oelinux/poky/build/conf/set_anki_build_version.sh && setenv-8009-robot-image$PERForUSER && bitbake -c cleanall victor anki-version rampost base-files wired iptables machine-robot-image core-image-anki-initramfs && $ToDo && export OTA_MANIFEST_SIGNING_KEY=$OTA_SIGNING_PASSWORD && ANKIDEV=$ADEV DO_SIGN=$DO_SIGN make && exit 0"

echo
echo -e "\033[1;32mComplete. Your $PRODorOSKR OTA is in $(pwd)/_build/$OTA_NAME.\033[0m"
echo
