#!/bin/bash

set -e

VICOS_SDK_LINK="https://github.com/kercre123/anki-deps/releases/download/1.1.0-r04/vicos-sdk-1.1.0-r04-x86_64-ubuntu-16.04.tar.gz"
WWISE_LINK="https://github.com/kercre123/anki-deps/releases/download/1.1.0-r04/wwise-2017.2.7_a.tar.gz"

if [[  -d ../anki/anki-ble ]]; then
    cd ..
fi

if [[ ! -d anki/anki-ble ]]; then
    echo "Error. This script must be run in the vicos-oelinux folder, as ./build/docker-ota-build.sh"
    exit 1
fi

if [[ -f anki-deps/.gitignore ]]; then
	echo "vicos-sdk already fetched (git)"
	exit 0
fi

if [[ -f anki-deps/.done ]]; then
	echo "vicos-sdk already fetched"
	exit 0
fi

rm -rf anki-deps

mkdir -p anki-deps/vicos-sdk/dist/1.1.0-r04
mkdir -p anki-deps/wwise/versions/2017.2.7_a

cd anki-deps/vicos-sdk/dist/1.1.0-r04
wget ${VICOS_SDK_LINK}
tar -zxf vicos-sdk-1.1.0-r04-x86_64-ubuntu-16.04.tar.gz
rm vicos-sdk-1.1.0-r04-x86_64-ubuntu-16.04.tar.gz

cd ../../../wwise/versions/2017.2.7_a
wget ${WWISE_LINK}
tar -zxf wwise-2017.2.7_a.tar.gz
rm wwise-2017.2.7_a.tar.gz

cd ../../../../
touch anki-deps/.done
echo $(pwd)
