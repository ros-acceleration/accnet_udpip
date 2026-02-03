# Getting Started with FPGA UDP Offloading

## Build bitstream and userspace driver

Bitstreams and drivers can be downloaded or built from scratch following the specific documentation.

1) FPGA Bistream:
    - Refer to [10 GbE - Docs](/README.md) for 10 Gb Ethernet acceleration
    - Refer to [1 GbE - Docs](/README.md) for 1 Gb Ethernet acceleration
2) Linux driver:
    - Refer to [UDP IP Core - Docs](/software/README.md) for both kernel and userspace support

## Load AccNet UDP/IP Core

```bash
sudo xmutil unloadapp
sudo xmutil loadapp udp_ip_core
```

From now on, you can take advantage of UDP offloading both using Linux kernel driver or userspace driver. 
If you want more information on them, please refer to `kernel` folder documentation and `userspace` folder documentation in software folder.

## UDP network offload using Linux kernel driver

Please refer to `kernel` documentation to insert the kernel module. 
After that, a new network interface will be available. No more actions are needed!

## UDP network offload using userspace driver

### Configure a 'dummy' network interface

Both FastDDS and cyclonedds can be configured to use a specific ethernet interface.
Without a proper configuration file, they will fallback on the available Ethernet interface.

Therefore, in order to let them use only the interface provided by userspace driver, a ethernet dummy interface it is needed.

```bash
sudo modprobe dummy
sudo ip link add dummy0 type dummy
sudo ip addr add <intf-ip-addr>/24 dev dummy0
sudo ip link set dummy0 up
```

### Integration with CycloneDDS

Clone the cyclonedds repo, change inside and build it.

```bash
git clone git@github.com:eclipse-cyclonedds/cyclonedds.git
cd cyclonedds
mkdir build
cd build
cmake -DCMAKE_INSTALL_PREFIX=install .. -DBUILD_EXAMPLES=ON
cmake --build .
```

Prepare a CycloneDDS configuration file. Make sure to use the dummy interface created and to fix participants IDs for peers in the domains.
You can find boilerplate configuration files (for both Kria and Workstation) in this repository, under the `cfg` folder, i.e. [kria.xml](dds/cyclonedds/kria.xml)

The usage of userspace driver requires root privileges. Switch into root, export `CYCLONEDDS_URI` environment variable with the actual path of the just created XML configuration file.

If you have multiple installation of CycloneDDS, make sure to export `LD_LIBRARY_PATH` with the path desired CycloneDDS installation libraries

```bash
sudo su
export CYCLONEDDS_URI="<path-to-xml>" 
export LD_LIBRARY_PATH=<path-to-cyclonedds-folder>/build/lib:$LD_LIBRARY_PATH
```

Run the HelloWorld application (e.g. Subscriber):

```bash
LD_PRELOAD=/lib/libsock.so <path-to-cyclonedds-folder>/build/bin/HelloworldSubscriber # Publisher on the other side!
```

## Integration with FastDDS

Prepare a FastDDS configuration file. Make sure to use the dummy interface created and to fix participants IDs for peers in the domains.
You can find boilerplate configuration files (for both Kria and Workstation) in this repository, under the `cfg` folder, i.e. [kria.xml](dds/fastdds/kria.xml)

```bash
sudo su
export FASTRTPS_DEFAULT_PROFILES_FILE="<path-to-xml>" 
```

## Run ROS2 Applications

Before running ROS2 run commands, make sure to export `RMW_IMPLEMENTATION` environment variable to configure ROS in adopting either CycloneDDS middleware or FastDDS. 
Then, especially if you have multiple installation of DDS, make sure to export `LD_LIBRARY_PATH` with the path desired DDS installation libraries.

```bash
source /opt/ros/<version>/setup.bash
source <path-to>/ros2_ws/install/setup.bash
```

Then export environment variables depending on DDS flavour.

With CycloneDDS:
```bash
export CYCLONEDDS_URI="<path-to-xml>" 
export RMW_IMPLEMENTATION=rmw_cyclonedds_cpp
export LD_LIBRARY_PATH=<path-to-built-cyclonedds>/lib:$LD_LIBRARY_PATH
LD_PRELOAD=/lib/libsock.so ros2 run py_pubsub talker
```

With FastDDS:
```bash
export FASTRTPS_DEFAULT_PROFILES_FILE="<path-to-xml>" 
export RMW_IMPLEMENTATION=rmw_fastrtps_cpp
export LD_LIBRARY_PATH=<path-to-built-fastdds>/lib:$LD_LIBRARY_PATH
LD_PRELOAD=/lib/libsock.so ros2 run py_pubsub talker
```