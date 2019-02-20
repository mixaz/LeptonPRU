cd /work
# stolen from from https://github.com/kylemanna/docker-am335x/blob/master/Dockerfile
git clone --depth 1 --branch bb.org-4.1-dt-overlays5 --single-branch https://github.com/RobertCNelson/dtc
cd dtc
git checkout -b tested 1e75ebc95be2eaadf1e959e1956e32203a80432e

CC=cc AS=as AR=ar LD=ld make
CC=cc AS=as AR=ar LD=ld make check install PREFIX=/work/dtc-install
cd -

