# PRU Firmware for IR camera FLIR Lepton 3.
#
# This file is a part of the LeptonPRU project.
#
# Copyright (C) 2018-2019 Mikhail Zemlyanukha <gmixaz@gmail.com>
#
# This program is free software; you can redistribute it and/or modify it
# under the terms of the GNU General Public License as published by the
# Free Software Foundation; either version 2 of the License, or (at your
# option) any later version.

FROM dockcross/linux-armv7a

ENV DEFAULT_DOCKCROSS_IMAGE leptonpru

ENV PRU_CGT=/usr/share/ti-cgt-pru_2.3.1 \
    PRU_SP_PATH=/usr/lib/ti/pru-software-support-package-5.1.0

RUN apt-get install -y libc6-i386 lzop

RUN curl -o pru-software-support-package_5.1.0.tar.gz -L https://github.com/bbvch/pru-software-support-package/archive/v5.1.0.tar.gz && \
    mkdir /usr/lib/ti && \
    tar -xzf pru-software-support-package_5.1.0.tar.gz -C /usr/lib/ti

RUN curl -LO http://software-dl.ti.com/codegen/esd/cgt_public_sw/PRU/2.3.1/ti_cgt_pru_2.3.1_linux_installer_x86.bin && \
    chmod +x ti_cgt_pru_2.3.1_linux_installer_x86.bin && \
    ./ti_cgt_pru_2.3.1_linux_installer_x86.bin --mode unattended --prefix /usr/share


