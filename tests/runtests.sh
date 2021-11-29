#!/bin/bash

set -e

cd "$(dirname "$0")"

# Build TADS against Remglk
mkdir -p build
cmake -DGlkLibPath=remglk -S../glk/ -Bbuild/
make -C build/ -j$(nproc) --no-print-directory

# Run the tests with regtest.py
echo 'TADS 2'
python regtest.py -i build/tadsr ditch.gam.regtest -t 20
echo 'TADS 3'
python regtest.py -i build/tadsr ditch3.t3.regtest -t 20

rm tadstest.glksave