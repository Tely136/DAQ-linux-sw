# DDR photon-bin test

Reads 10 contiguous 32-bit unsigned bins (40 bytes) through `/dev/mem`.
It accepts a physical DDR address and only reads memory. It does not allocate
the buffer, initialize bins, configure AXI-Lite, or start the HLS IP.

## Reserve the buffer first

The local `cora-mcs/sdt/system-top.dts` describes DDR from `0x00100000`
through `0x1fffffff`. No reservation was found in that SDT, the generated
`cora-mcs-generated/cortexa9-linux.dts`, or the custom user include inspected.
This does not establish what is currently booted on the board.

`reserved-memory.dtsi` supplies a candidate 4 KiB reservation at `0x1f000000`.
Before using it, verify the deployed DDR map and other reservations/bootloader
buffer placement. Merge this snippet into the **Linux boot device tree** before
boot, rebuild/deploy the DTB actually loaded by the board, and reboot.
The existing custom include is:

`/home/tely1/yocto/custom-layers/meta-cora/recipes-bsp/device-tree/files/cora-z7-07s-user.dtsi`

The adjacent `device-tree.bbappend` adds it through `EXTRA_DT_INCLUDE_FILES`.
The snippet here has not been installed into that layer. Updating only FPGA
programming or BOOT.BIN is insufficient unless that also updates the Linux DTB
actually used at boot. Do not add this reservation as a runtime overlay after
Linux has already allocated the RAM.

On the board, inspect `sudo cat /proc/iomem`, boot messages, and, if `dtc` is
installed, `sudo dtc -I fs -O dts /sys/firmware/devicetree/base` to confirm the
address/size and `no-map` property. The buffer must be excluded from allocatable
System RAM. The program cannot verify the reservation itself.

The reserved region avoids Linux allocating the FPGA buffer; `no-map` prevents
its normal cached kernel mapping. This prototype additionally relies on the
target ARM kernel providing an uncached `/dev/mem` mapping with `O_SYNC`.
`volatile` alone does not provide cache coherence. If the kernel disallows
`/dev/mem` access or suitable mapping attributes cannot be established, use a
kernel driver with the DMA API instead of mapping ordinary RAM.

## HLS setup

1. Ensure the HLS master's address segment reaches PS DDR through `S_AXI_GP`.
2. With the IP stopped, program its output-pointer AXI-Lite register with the
   reserved **physical** address, e.g. `0x1f000000` after the above reservation
   is active. Use the generated HLS `*_hw.h` register definitions, including
   any upper address word. The mapped userspace pointer is not that address.
3. Initialize all 10 bins before incrementing them, either in HLS or through
   an appropriate buffer owner. Reserved DDR contents are not guaranteed zero.
4. Start acquisition, then stop it and wait for outstanding DDR writes to
   complete before running the reader. With a running writer the 10 reads
   are not a consistent snapshot. Completion handling depends on the IP's
   control protocol and generated register map.

`0x40000000` belongs to the PL peripheral aperture in this local design, not
the DDR buffer. The AXI-Lite control base and DDR output base are distinct.
The register map is needed before automatic HLS control can be added.

## Build and run

From this repository root, in a fresh shell:

```sh
source /home/tely1/amd_sdk/cora-gpio/environment-setup-cortexa9t2hf-neon-amd-linux-gnueabi
cmake -S ddr_test -B ddr_test/build
cmake --build ddr_test/build
scp ddr_test/build/ddr_test amd-edf@192.168.137.20:/tmp/
```

After the buffer reservation and HLS setup are complete, on the board:

```sh
sudo /tmp/ddr_test 0x1f000000
```

The address above is only usable after verifying and deploying its reservation.
In VS Code, select the `ddr_test` folder and `Cora Z7 EDF SDK` CMake kit,
configure, then use the build task.

References: [AMD GP slave DDR access](https://docs.amd.com/r/en-US/ug585-zynq-7000-SoC-TRM/PL-DMA-via-General-Purpose-AXI-Slave-GP),
[Linux reserved-memory binding](https://www.kernel.org/doc/Documentation/devicetree/bindings/reserved-memory/reserved-memory.yaml).

## Set the HLS buffer address without BusyBox

The build also produces `ddr_set_address`. With the PL start signal inactive
and the IP idle, run on the board:

```sh
sudo /tmp/ddr_set_address 0x1f000000
```

It writes the low/high address words at `0x40000010` and `0x40000014`, then
checks readback. It does not reserve or initialize DDR, or start acquisition.
The address is checked against the local DDR range, but reservation cannot be
verified by this utility. Keep the IP idle until it exits successfully.
Then trigger acquisition and read the bins with `ddr_test` after writes finish.

Transfer both executables from the development machine:

```sh
scp ddr_test/build/ddr_set_address ddr_test/build/ddr_test amd-edf@192.168.137.20:/tmp/
```

The generated `xmcs_hw.h` supplies these pointer offsets and has no software
start/done registers; acquisition is controlled by the external PL signal.
