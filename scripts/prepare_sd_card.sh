#!/usr/bin/env bash
# Chuẩn bị thẻ micro SD cho project SD_Card_Reader:
# - Format FAT32
# - Tạo file test.txt với nội dung "Hello SD"
#
# Cách dùng:
#   1. Cắm đầu đọc thẻ SD (USB) vào máy tính
#   2. Chạy: bash scripts/prepare_sd_card.sh
#   3. Chọn đúng thiết bị thẻ SD (KHÔNG chọn ổ cứng!)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
MOUNT_DIR="/tmp/sd_card_reader_mount"
TEST_CONTENT="Hello SD"

red()   { printf '\033[0;31m%s\033[0m\n' "$*"; }
green() { printf '\033[0;32m%s\033[0m\n' "$*"; }
yellow(){ printf '\033[1;33m%s\033[0m\n' "$*"; }

if [[ "${EUID:-$(id -u)}" -ne 0 ]]; then
  red "Script cần quyền root để format thẻ."
  echo "Chạy lại bằng lệnh:"
  echo "  sudo bash $SCRIPT_DIR/prepare_sd_card.sh"
  exit 1
fi

if ! command -v mkfs.vfat >/dev/null 2>&1; then
  yellow "Đang cài dosfstools (công cụ format FAT32)..."
  apt-get update -qq && apt-get install -y dosfstools
fi

echo "=============================================="
echo "  Chuẩn bị thẻ SD cho STM32 SD_Card_Reader"
echo "=============================================="
echo
echo "Các thiết bị lưu trữ hiện có:"
echo
lsblk -o NAME,SIZE,TYPE,FSTYPE,LABEL,MOUNTPOINT,MODEL | grep -v loop || lsblk
echo

# Gợi ý thiết bị removable (thường là thẻ SD)
mapfile -t CANDIDATES < <(
  lsblk -dpno NAME,RM,TYPE | awk '$2=="1" && $3=="disk" {print $1}'
)

if ((${#CANDIDATES[@]} > 0)); then
  yellow "Thiết bị có thể là thẻ SD (removable):"
  for dev in "${CANDIDATES[@]}"; do
    echo "  - $dev"
  done
  echo
fi

read -r -p "Nhập tên thiết bị thẻ SD (ví dụ: /dev/sdb hoặc /dev/mmcblk0): " DISK
DISK="${DISK%/}"

if [[ -z "$DISK" ]]; then
  red "Chưa nhập tên thiết bị."
  exit 1
fi

if [[ ! -b "$DISK" ]]; then
  red "Không tìm thấy thiết bị: $DISK"
  exit 1
fi

# Chặn format nhầm ổ hệ thống
case "$DISK" in
  /dev/nvme*|/dev/sda)
    red "CẢNH BÁO: $DISK có thể là ổ cứng hệ thống!"
    red "Hãy chắc chắn đây là thẻ SD, không phải ổ Windows/Linux."
    read -r -p "Bạn CHẮC CHẮN muốn tiếp tục? (gõ YES): " CONFIRM
    [[ "$CONFIRM" == "YES" ]] || exit 1
    ;;
esac

# Chọn partition hoặc tạo mới trên cả disk
if [[ "$DISK" == *mmcblk* ]]; then
  PART="${DISK}p1"
else
  PART="${DISK}1"
fi

red "TOÀN BỘ DỮ LIỆU trên $DISK sẽ bị XÓA!"
echo "Sẽ format partition: $PART thành FAT32"
read -r -p "Gõ YES để xác nhận: " CONFIRM
[[ "$CONFIRM" == "YES" ]] || { echo "Đã hủy."; exit 0; }

# Unmount mọi partition của disk
while read -r mp; do
  umount "$mp" 2>/dev/null || true
done < <(lsblk -lnpo MOUNTPOINT "$DISK" | grep -v '^$' || true)

# Tạo partition table mới nếu chưa có partition hợp lệ
if ! lsblk -lnpo NAME "$DISK" | grep -qE "${PART##*/}$"; then
  yellow "Tạo partition mới trên $DISK ..."
  parted -s "$DISK" mklabel msdos
  parted -s "$DISK" mkpart primary fat32 1MiB 100%
  partprobe "$DISK" 2>/dev/null || true
  sleep 2
fi

yellow "Đang format FAT32 trên $PART ..."
mkfs.vfat -F 32 -n SD_CARD "$PART"

mkdir -p "$MOUNT_DIR"
mount "$PART" "$MOUNT_DIR"

echo "$TEST_CONTENT" > "$MOUNT_DIR/test.txt"
sync

green "Hoàn tất!"
echo
echo "  Partition : $PART"
echo "  Hệ thống  : FAT32 (nhãn SD_CARD)"
echo "  File      : test.txt"
echo "  Nội dung  : $TEST_CONTENT"
echo
ls -la "$MOUNT_DIR"

umount "$MOUNT_DIR"
rmdir "$MOUNT_DIR"

green "Thẻ SD đã sẵn sàng. Rút thẻ an toàn rồi cắm vào module SPI trên STM32."
