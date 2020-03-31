qcacld-2.0
==========

Boundary Devices changes to [CodeAurora qcacld-2.0 repository][codeaurora].

This code has only been tested againt the following Boundary Devices kernel branches:
* [boundary-imx\_4.14.x\_2.0.0\_ga kernel branch][branch-4.14.x]

Firmware files
--------------

This branch requires to use the following firmware files:
* [bd-sdmac-qcacld-lea-2.0 firmware branch][branch-fw-lea2.0]

Build instructions
------------------

**1. Download the source code**
```
$ cd
$ git clone https://github.com/boundarydevices/qcacld-2.0 -b boundary-CNSS.LEA.NRT_2.0
$ cd qcacld-2.0/
```

**2. Setup the environment**
* Assuming you are using `gcc-arm-linux-gnueabihf` toolchain available for Debian/Ubuntu
* If not, please make sure to specify the proper toolchain
```
$ export ARCH=arm
$ export CROSS_COMPILE=arm-linux-gnueabihf-
```

**3. Build the module**
* `<kernel_path>` must be replaced with the actual path of kernel source code
```
$ KERNEL_SRC=<kernel_path> make
```
* In order to build the module with debug messages enabled, add `BUILD_DEBUG_VERSION=1` to the previous command

**4. Install the module**
* `<rootfs_path>` must be replaced with the actual path of the target root file-system
* It can either be on your drive (NFS) or directly on an SD card
```
$ KERNEL_SRC=<kernel_path> INSTALL_MOD_PATH=<rootfs_path> make modules_install
```

[codeaurora]: https://source.codeaurora.org/quic/la/platform/vendor/qcom-opensource/wlan/qcacld-2.0/ "CodeAurora qcacld-2.0"
[branch-fw-lea2.0]: https://github.com/boundarydevices/qca-firmware/tree/bd-sdmac-qcacld-lea-2.0 "bd-sdmac-qcacld-lea-2.0 firmware branch"
[branch-4.14.x]: https://github.com/boundarydevices/linux-imx6/tree/boundary-imx_4.14.x_2.0.0_ga "boundary-imx_4.14.x_2.0.0_ga kernel branch"
