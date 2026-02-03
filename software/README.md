# Linux support for AccNet UDP/IP Core

<!-- TOC -->

- [Linux support for AccNet UDP/IP Core](#linux-support-for-accnet-udpip-core)
    - [Overview](#overview)
        - [Loadable Linux kernel driver .ko module](#loadable-linux-kernel-driver-ko-module)
        - [Userspace driver](#userspace-driver)
    - [Getting started - Kernel driver](#getting-started---kernel-driver)
        - [Download kernel module for Ubuntu 22.04 - Kernel 5.15.0](#download-kernel-module-for-ubuntu-2204---kernel-5150)
        - [alternative Compile the kernel module](#alternative-compile-the-kernel-module)
        - [Insert the kernel module](#insert-the-kernel-module)
        - [optional Make the module loadable at boot.](#optional-make-the-module-loadable-at-boot)
        - [Configure the interface](#configure-the-interface)
    - [Getting started - Userspace driver](#getting-started---userspace-driver)
        - [Compile the userspace driver library](#compile-the-userspace-driver-library)
        - [Take advantage of it](#take-advantage-of-it)
    - [Kernel module - Development and internals](#kernel-module---development-and-internals)
        - [Device-tree support](#device-tree-support)
        - [Module logic overview](#module-logic-overview)
    - [Userspace driver - Development and internals](#userspace-driver---development-and-internals)
        - [Porting the driver to a different OS](#porting-the-driver-to-a-different-os)

<!-- /TOC -->


## Overview

This folder contains all software to take advantage of AccNet UDP/IP Core on Linux, specifically
- [Linux kernel driver](kernel/)
- [Linux userspace driver](userspace/driver)
- [Linux userspace benchmarks](userspace/benchmark/)
- [Linux userspace examples](userspace/example/)

Drivers provide a support for controlling and configuring the AccNet UDP IP Core enabling seamless integration of the network functionality into Linux, allowing to leverage the high-performance, hardware-accelerated network stack for communication over UDP/IP. 

The driver is shipped in two flavours:
- loadable Linux kernel driver (.ko module)
- userspace driver (with limited kernel facility support)

Understand the differences between those two flavours is fundamental for a correct usage and integration.

### Loadable Linux kernel driver (.ko module)

The loadable Linux kernel driver registers a virtual network interface (by default named `udpip0`).
That interface can be configured with an IP address and supports Linux networking tools (such as `ifconfig` from `net-tools`) or `ip addr` command line.
Furthermore, the interface exposes a set of devlink configurable options.

However, due to usage of kernel facilities, it needs to do a few packet copies, so that performances are limited.

### Userspace driver

The userspace driver allows to get advantage of zero-copy packet management, working directly with the FPGA device memory.
For this purpose, it should be run with root privileges and makes use of Xilinx's XRT library which should be installed on the target platform.

The userspace driver can be integrated in a custom application for highest performance; for existing applications a UDP-socket compatible layer is provided which allows to replace socket-related syscalls seamlessy. However, only a subset of socket functionalities are implemeted, therefore, out-of-the-box integration, is not guaranteed to all applications.

## Getting started - Kernel driver

### 1. Download kernel module for Ubuntu 22.04 - Kernel 5.15.0

A pre-compiled kernel module for Ubuntu 22.04 with Kernel 5.15.0 (the stock version provided by Canonical for AMD Kria KR260) can be downloaded from Gitlab. You can download the compiled module and use it if your kernel version matches it.

As soon as you have downloaded it, you can move to step 2.

### 1. (alternative) Compile the kernel module


Clone the repository and change into the folder:

```bash
$ git clone git@github.com:ros-acceleration/accnet_udpip.git
$ cd accnet_udpip/software/kernel
```

Compile the kernel module using Make:

```
make clean && make
```

At this point you should have a `udp-core.ko` compiled kernel module in the folder.

### 2. Insert the kernel module

Insert the kernel module and verify that the kernel recognizes it.

```bash
$ sudo insmod udp_core.ko
$ lsmod | grep udp_core 
udp_core               16384  0
```

Double check that kernel module initialization succeeded:

```
[ 1761.974192] udp_core: loading out-of-tree module taints kernel.
[ 1761.974290] udp_core: module verification failed: signature and/or required key missing - tainting kernel
[ 1761.975548] udp-core: initializing kernel module.
```

The kernel module, by now, is not shipped with a sign.

### 3. (optional) Make the module loadable at boot.

Edit the `/etc/modules` file and add the name of the module (without the `.ko` extension) on its own line. On boot, the kernel will try to load all the modules named in this file.

Copy the module to a suitable folder in /lib/modules/`uname -r`/kernel/drivers. This will place the module in modprobe's database.

Run `depmod`. This will find all the dependencies of your module.

Reboot Linux. To verify that the module is correctly inserted, please execute `lsmod | grep udp_core`.

### 4. Configure the interface

An IP address should be manually assigned to the network interface. 
`ip addr` can be used for that purpose.

```bash
sudo ip addr add <your-ip-addr>/24 dev udpip0
```

`ip addr` configuration do not survive after reboots. Therefore, to assign permanently an IP address to the interface you can take advantage of `nmcli`.
An example script which sets up an IP address using `nmcli` can be found in tools folder: [set-if.sh](/utils/set-if.sh)

The gateway address should be manually set up for the interface, because the system-wide one typically points to a different network interface.
That address can be configured using a `devlink` command.

```bash
sudo devlink dev param show # shows all configurable options
sudo devlink dev param set platform/a0010000.fpga name GATEWAY_IP value <your-gw-ip-addr> cmode runtime
```

## Getting started - Userspace driver

### 1. Compile the userspace driver library 

Install dependencies:

```bash
$ sudo apt-get install -y uuid-dev
```

Clone the repository and change into the folder:

```bash
$ git clone git@github.com:ros-acceleration/accnet_udpip.git
$ cd accnet_udpip/software/userspace/driver
```

Setup the following defines in `socket.h` to configure the userspace driver

```c
#define LOCAL_IP                {192, 168, 1, 128}                      
#define GW_IP                   {192, 168, 1, 2}                    
#define LOCAL_SUBNET            {255, 255, 255, 0}
#define LOCAL_MAC               {0x02, 0x00, 0x00, 0x00, 0x00, 0x00}

#define LOCAL_PORT_MIN          7400
#define LOCAL_PORT_MAX          7500
```

Compile the shared library using Make:

```bash
make clean && make lib-prod && make install
```

The library will be installed in `/lib/` as `libsock.so`.

### 2. Take advantage of it

With an existing application, you can preload the shared library to make use of customized socket support.

```bash
LD_PRELOAD=/lib/libsock.so ./example-application
```

In order to use the library, you should run the application with root privileges.

## Kernel module - Development and internals

The module was tested with several kernel versions. It should support every recent kernel version.
The target architecture for the module is `aarch64`.

### Device-tree support

The driver module can be dynamically loaded into the Linux kernel using the `modprobe` or `insmod` commands. 

To enable automatic probing of the kernel module, the device tree overlay must define a compatible device.
The device tree entry should include a compatible property that matches the driver's expected value, allowing the kernel to recognize the device and load the module automatically. 
An example device tree snippet would look like this:

```
/dts-v1/;

/ {
     ....

     udp_ip_core {
         interrupt-parent = <&gic>;
         interrupts = <GIC_SPI 89 IRQ_TYPE_EDGE_RISING GIC_SPI 90 IRQ_TYPE_EDGE_RISING>;
         compatible = "accelerat,udp-core";
     };
};
```

This configuration specifies the interrupt settings and the `compatible` string `accelerat,udp-core`, which the driver uses to identify the AccNet UDP IP Core.
Additionally, the device tree entry should include an interrupts property, which specifies the details of the interrupts that the IP core can generate. 
For each interrupt, the following information must be provided:

- Type of interrupt: SPI (Shared Peripheral Interrupt)
- Hardware interrupt number: The unique interrupt number assigned to the device.
- Line status: The condition that triggers the interrupt, rising edge detection.

This ensures that the system correctly handles the interrupts generated by the IP core.

`devlink` can be used to dump the status of all device registers. The script [devlink-dump.sh](/utils/devlink-dump.sh) is meant for it.

### Module logic overview

In the init phase, the module reads the HW-IRQs from the device tree, obtains a mapping in the Linux IRQ domain for each of them, and asks for a handle to the kernel. 
Once inserted, registered IRQs can be verified with `procfs`:

```
~ # cat /proc/interrupts 
           CPU0       CPU1       CPU2       CPU3       
  9:          0          0          0          0     GICv2  25 Level     vgic
 11:      40433      38366      42941      39459     GICv2  30 Level     arch_timer
 12:          0          0          0          0     GICv2  27 Level     kvm guest vtimer
 14:          0          0          0          0     GICv2  67 Level     zynqmp-ipi
 ....
 18:      40433      38366      42941      39459     GICv2  89 Edge      udp-core
 19:          0          0          0          0     GICv2  90 Edge      udp-core
```

When the UDP-IP Core in FPGA receives a packet, it copies it into memory. When the copy is finished, a IRQ is triggered.
IRQs are managed using Linux kernel's NAPI.

## Userspace driver - Development and internals

The main file (`main.c`) contains a structured example on how to take advantage of the user space driver. It can be used as a starting skeleton and contains user setup parameters (ip addresses, port bindings, ..).

On the other hand, `udriver.h` and `udriver.c` contains the driver main functions and configurations. 
When using the userspace driver, the `udriver.h` library should be included and `udriver.c` compiled along.

### Porting the driver to a different OS

The userspace driver can be ported onto different OSes or RTOSes, such as FreeRTOS. The driver is composed by a hardware management layer (`udriver`) and a socket-compatible layer (`socket`). 

In order to port the user space driver to a different OS, the hardware management layer is the only mandatory part to be adapted. The socket-compatible layer is only needed when dealing with existing application using socket based API.

Therefore, the following steps, are needed to port the hardware management layer (`udriver`) to a different OS / RTOS.

1. A few standars library functions are used, for example, to print out errors. Replace standard lib functions with RTOS-specific.

2. The device registers, in Linux, are accessed via virtual memory addressing. The driver, therefore, maps physical memory locations to virtual address using mmap. Replace memory mapping using physical address pointers or memory-mapped macros instead of virtual address mapping.

3. A few XRT Runtime APIs are used, especially to handle cache memory buffer allocation and memory access and synchronization. On bare-metal, this entire buffer-management logic must use physical memory directly or via a custom memory allocator in BRAM/DDR.

4. IRQ support is implemented through the usage of a loadable kernel module and abstracted via file descriptors. Depending on the OS, replace the code inside macros `IRQ_SUPPORT` with specific ISR registration and callback.



