# args:
#   $1 - kernel revision ie 4.9.36-ti-r45 (uname -r)
#   $2 - kernel config taken from /boot/config-$(uname -r) on target device

pushd .

cd /work

# taken from /etc/apt/sources.list on BBB
git clone https://github.com/RobertCNelson/linux-stable-rcn-ee --depth 1 --branch $1 --single-branch
cd ./linux-stable-rcn-ee
git checkout -b tmp

# copy from start folder
popd
cp $2 /work/linux-stable-rcn-ee/.config

cd /work/linux-stable-rcn-ee
make -j3 ARCH=arm CROSS_COMPILE=$CROSS_ROOT/bin/$CROSS_TRIPLE-


