#!/usr/bin/env bash

set -euo pipefail

# Paths and names to customize.
VITIS_SETTINGS="/opt/Xilinx/2026.1/Vitis/settings64.sh"
SDT_DIR="/home/tely1/yocto/hardware/cora-mcs/sdt/"
XSA_PATH="/mnt/c/DAQ-HW/cora-mcs/counter_wrapper.xsa"
MACHINE_NAME="cora-mcs-generated"
IMAGE_MACHINE_NAME="amd-cortexa9thf-neon-common"
REMOTE_HOST="amd-edf@192.168.137.20"
REMOTE_PATH="/tmp/boot.bin"
JUMP_HOST="thomas@pi5"

EDF_ROOT="/home/tely1/yocto/edf"
EDF_BUILD_DIR="$EDF_ROOT/build"
CUSTOM_LAYER="/home/tely1/yocto/custom-layers/meta-cora"
BOOTBIN_PATH="/home/tely1/yocto/edf/build/tmp/deploy/images/cora-mcs-generated/boot.bin"
WIC_IMAGE="/home/tely1/yocto/edf/build/tmp/deploy/images/amd-cortexa9thf-neon-common/edf-linux-disk-image-amd-cortexa9thf-neon-common.rootfs.wic"
IMAGE_RECIPE="edf-linux-disk-image"

RUN_SDT=false
RUN_BOOTBIN=false
RUN_IMAGE=false
RUN_WIC=false
RUN_DEPLOY=false

usage() {
	cat <<'USAGE'
Usage: amd-edf-tools.sh [phase ...]

Phases:
  --sdt       Generate the system device tree with SDTGen
  --bootbin   Generate the machine configuration and BOOT.BIN
  --image     Build edf-linux-disk-image
  --wic       Copy BOOT.BIN into WIC partition 1
  --deploy    SCP BOOT.BIN to the configured remote path
  --all       Run all phases (the default)
  -h, --help  Show this help

Examples:
  ./amd-edf-tools.sh --sdt --bootbin
  ./amd-edf-tools.sh --deploy
  ./amd-edf-tools.sh --image --wic
USAGE
}

if [[ $# -eq 0 ]]; then
	RUN_SDT=true
	RUN_BOOTBIN=true
	RUN_IMAGE=true
	RUN_WIC=true
else
	for phase in "$@"; do
		case "$phase" in
			--sdt) RUN_SDT=true ;;
			--bootbin) RUN_BOOTBIN=true ;;
			--image) RUN_IMAGE=true ;;
			--wic) RUN_WIC=true ;;
			--deploy) RUN_DEPLOY=true ;;
			--all)
				RUN_SDT=true
				RUN_BOOTBIN=true
				RUN_IMAGE=true
				RUN_WIC=true
				RUN_DEPLOY=true
				;;
			-h|--help) usage; exit 0 ;;
			*) printf 'Unknown phase: %s\n\n' "$phase" >&2; usage >&2; exit 2 ;;
		esac
	done
fi

if [[ "$RUN_SDT" == true && ! -f "$VITIS_SETTINGS" ]]; then
	printf 'Vitis settings file not found: %s\n' "$VITIS_SETTINGS" >&2
	exit 1
fi

if [[ "$RUN_SDT" == true && ! -d "$SDT_DIR" ]]; then
	printf 'SDT directory not found: %s\n' "$SDT_DIR" >&2
	exit 1
fi

if [[ "$RUN_SDT" == true && ! -f "$SDT_DIR/system-top.dts" ]]; then
	printf 'system-top.dts not found in SDT directory: %s\n' "$SDT_DIR" >&2
	exit 1
fi

# Generate the SDT in a Vitis environment isolated from EDF.
if [[ "$RUN_SDT" == true ]]; then
(
	set +u
    source "$VITIS_SETTINGS"
	set -u

    sdtgen <<SDTGEN_COMMANDS
set_dt_param -dir "$SDT_DIR"
set_dt_param -xsa "$XSA_PATH"
generate_sdt
SDTGEN_COMMANDS
)
fi

# Generate the machine configuration and build BOOT.BIN in an EDF environment.

if [[ "$RUN_BOOTBIN" == true || "$RUN_IMAGE" == true || "$RUN_WIC" == true ]]; then
(
	set +u
	source "$EDF_ROOT/edf-init-build-env" "$EDF_BUILD_DIR"
	set -u

	if [[ "$RUN_BOOTBIN" == true ]]; then
		gen-machine-conf \
			--hw-description "$SDT_DIR" \
			--machine-name "$MACHINE_NAME" \
			--config-dir "$CUSTOM_LAYER/conf" \
			parse-sdt

		MACHINE="$MACHINE_NAME" bitbake xilinx-bootbin
	fi

	if [[ "$RUN_IMAGE" == true ]]; then
		MACHINE="$IMAGE_MACHINE_NAME" bitbake "$IMAGE_RECIPE"
	fi

	if [[ "$RUN_WIC" == true && ! -f "$BOOTBIN_PATH" ]]; then
		printf 'BOOT.BIN not found: %s\n' "$BOOTBIN_PATH" >&2
		exit 1
	fi

	if [[ "$RUN_WIC" == true && ! -f "$WIC_IMAGE" ]]; then
		printf 'WIC image not found: %s\n' "$WIC_IMAGE" >&2
		exit 1
	fi

	if [[ "$RUN_WIC" == true ]]; then
		wic cp "$BOOTBIN_PATH" "$WIC_IMAGE:1"
	fi
)
fi

if [[ "$RUN_DEPLOY" == true ]]; then
	if [[ ! -f "$BOOTBIN_PATH" ]]; then
		printf 'BOOT.BIN not found: %s\n' "$BOOTBIN_PATH" >&2
		exit 1
	fi

	scp -o "ProxyJump=$JUMP_HOST" "$BOOTBIN_PATH" "$REMOTE_HOST:$REMOTE_PATH"
fi