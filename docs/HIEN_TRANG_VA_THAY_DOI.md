# SD_Card_Reader — Hiện trạng dự án và hướng dẫn rebuild theo Nhung-Vao-Tim-Em

Tài liệu này mô tả **trạng thái hiện tại** của dự án `SD_Card_Reader/` và liệt kê **các thay đổi cần bổ sung/sửa** để đạt mức tương đương dự án tham chiếu `Nhung-Vao-Tim-Em/`.

---

## 1. Tổng quan so sánh nhanh

| Hạng mục | SD_Card_Reader (hiện tại) | Nhung-Vao-Tim-Em (mục tiêu) |
| --- | --- | --- |
| MCU | STM32F429ZIT6 | STM32F429ZIT6 |
| Giao tiếp SD | SPI1 (PA5/6/7, CS = PG13) | SPI1 (cùng chân) |
| FatFs | Có | Có |
| Driver SD-SPI | `Core/Src/sd_spi.c` (tự viết) | `FATFS/Target/user_diskio_spi.c` (kiwih/ChaN) |
| UART debug | **Chưa có** | USART1 115200 (PA9 TX, PA10 RX) |
| USB Mass Storage | **Chưa có** | USB OTG HS — MSC |
| Luồng demo `main.c` | Ghi/đọc `test.txt`, không log | Mount, thống kê dung lượng, đọc `test.txt`, ghi `write.txt`, log UART |
| README / tài liệu | **Chưa có** | `README.md`, `docs/PROJECT_OVERVIEW_VI.md` |
| `.gitignore` | **Chưa có** | Nên có (không commit `Debug/`) |
| PLLQ (clock USB 48 MHz) | PLLQ = 4 → 36 MHz | PLLQ = 3 → 48 MHz |

**Kết luận:** `SD_Card_Reader` đã có nền tảng SD + FatFs hoạt động được, nhưng thiếu UART, USB MSC, tài liệu và luồng demo giống dự án tham chiếu.

---

## 2. Hiện trạng chi tiết — SD_Card_Reader

### 2.1. Mục tiêu hiện tại

Firmware đọc/ghi file trên thẻ micro SD (FAT) qua SPI. Chưa có kênh debug và chưa expose thẻ SD ra PC qua USB.

### 2.2. Cấu trúc thư mục quan trọng

```text
SD_Card_Reader/
├── Core/
│   ├── Inc/
│   │   ├── main.h          # Định nghĩa SD_CS (PG13)
│   │   └── sd_spi.h        # API driver SD tự viết
│   └── Src/
│       ├── main.c          # Demo SD_FileTest()
│       ├── sd_spi.c        # Driver SD-SPI đầy đủ (~890 dòng)
│       ├── stm32f4xx_hal_msp.c
│       └── stm32f4xx_it.c
├── FATFS/
│   ├── App/fatfs.c
│   └── Target/
│       ├── user_diskio.c   # Gọi trực tiếp sd_spi.h
│       └── ffconf.h
├── Middlewares/Third_Party/FatFs/
├── Drivers/                # CMSIS + HAL (chỉ SPI, GPIO, RCC, DMA…)
├── SD_Card.ioc             # CubeMX: SPI1 + FatFs, không UART/USB
├── Debug/                  # Artifact build (đang có trong repo)
└── STM32F429ZITX_FLASH.ld
```

**Không có:** `USB_DEVICE/`, `Middlewares/ST/STM32_USB_Device_Library/`, `README.md`, `docs/`, `.gitignore`.

### 2.3. Cấu hình phần cứng (CubeMX — `SD_Card.ioc`)

| Thành phần | Cấu hình |
| --- | --- |
| SPI1 | Master, PA5 SCK, PA6 MISO, PA7 MOSI, prescaler 128 (~562.5 Kbit/s) |
| GPIO | PG13 = SD_CS (output) |
| USART1 | **Chưa bật** |
| USB OTG HS | **Chưa bật** |
| SYSCLK | 72 MHz (HSE + PLL) |
| PLLQ | 4 → USB clock 36 MHz (chưa dùng USB) |

### 2.4. HAL modules đã bật (`stm32f4xx_hal_conf.h`)

- Đã bật: `HAL_SPI`, `HAL_GPIO`, `HAL_RCC`, `HAL_DMA`, `HAL_FLASH`, `HAL_PWR`, `HAL_CORTEX`, `HAL_EXTI`
- **Chưa bật:** `HAL_UART`, `HAL_PCD` (cần cho UART và USB)

### 2.5. Lớp driver SD-SPI (`sd_spi.c`)

Dự án dùng driver **tự viết**, không dùng `user_diskio_spi.c` của kiwih:

| Hàm | Vai trò |
| --- | --- |
| `SD_Init()` | Khởi tạo thẻ (CMD0, CMD8, ACMD41, CMD58, CMD16…) |
| `SD_IsReady()` | Kiểm tra đã init |
| `SD_ReadBlocks()` | Đọc sector (CMD17/CMD18) |
| `SD_WriteBlocks()` | Ghi sector (CMD24/CMD25) |
| `SD_GetSectorCount()` | Đọc CSD, tính dung lượng |
| `SD_GetCardType()` | SDSC / SDHC |

**Ưu điểm so với dự án tham chiếu:** Code rõ ràng, comment tiếng Việt, tách file `sd_spi.h/c` trong `Core/`. **Không cần thay** bằng `user_diskio_spi.c` nếu driver hiện tại đã chạy ổn trên board.

### 2.6. Lớp disk I/O FatFs (`user_diskio.c`)

Adapter gọi thẳng API `sd_spi.h`:

```c
USER_initialize() → SD_Init()
USER_status()     → SD_IsReady()
USER_read()       → SD_ReadBlocks()
USER_write()      → SD_WriteBlocks()
USER_ioctl()      → GET_SECTOR_COUNT / GET_SECTOR_SIZE / CTRL_SYNC
```

Đã implement đầy đủ read/write/ioctl — **tương thích** với cách USB MSC sẽ gọi `USER_read`, `USER_write`, `USER_ioctl` sau này.

### 2.7. Luồng chương trình (`main.c`)

```
HAL_Init()
  → SystemClock_Config()
  → MX_GPIO_Init()
  → MX_SPI1_Init()
  → MX_FATFS_Init()
  → SD_FileTest()
      → f_mount()
      → f_open("test.txt", FA_CREATE_ALWAYS | FA_WRITE)
      → f_write("Hello from STM32!\r\n")
      → f_sync() → f_close()
      → f_open("test.txt", FA_READ)
      → f_read() → so sánh memcmp
  → while(1) {}
```

**Đặc điểm:**

- Demo **tự tạo** `test.txt` (không cần file sẵn trên thẻ).
- Kết quả chỉ lưu trong biến `sd_result` — **không in ra UART**.
- Không có `f_getfree()`, không ghi `write.txt`, không unmount.
- Không gọi `MX_USB_DEVICE_Init()`.

### 2.8. Điểm mạnh hiện tại

1. Driver SD-SPI hoàn chỉnh, hỗ trợ SDSC/SDHC, đọc/ghi multi-sector.
2. `user_diskio.c` đã nối FatFs với driver, có xử lý lỗi cơ bản.
3. Pinout SPI/CS **giống hệt** dự án tham chiếu.
4. SPI1 config (mode, prescaler) giống Nhung-Vao-Tim-Em.

### 2.9. Thiếu sót so với mục tiêu

1. Không có UART → khó debug trên board.
2. Không có USB MSC → PC không nhận thẻ như ổ USB.
3. PLLQ = 4 → cần đổi thành 3 khi bật USB (48 MHz).
4. Không có README, tài liệu nhóm, checklist test.
5. Commit thư mục `Debug/` — nên dọn bằng `.gitignore`.

---

## 3. Hiện trạng dự án tham chiếu — Nhung-Vao-Tim-Em

### 3.1. Tính năng bổ sung

1. **USART1** — `myprintf()` gửi log 115200 baud.
2. **USB Mass Storage** — PC đọc/ghi thẻ SD qua USB OTG HS (PB13 VBUS, PB14 DM, PB15 DP).
3. **Luồng demo** — mount, `f_getfree`, đọc `test.txt` có sẵn, ghi `write.txt`, unmount.
4. **Tài liệu** — README nhóm, `docs/PROJECT_OVERVIEW_VI.md`.

### 3.2. Khác biệt driver SD

Dự án tham chiếu dùng `user_diskio_spi.c` (port từ thư viện ChaN/kiwih), có macro `FCLK_SLOW()` / `FCLK_FAST()` đổi tốc độ SPI khi init. `SD_Card_Reader` dùng `sd_spi.c` riêng — **chức năng tương đương**, chỉ khác cách tổ chức file.

### 3.3. USB storage bridge (`usbd_storage_if.c`)

Các callback MSC gọi chung lớp disk I/O:

```c
STORAGE_Init_HS()        → USER_initialize()
STORAGE_GetCapacity_HS() → USER_ioctl(GET_SECTOR_COUNT/SIZE)
STORAGE_IsReady_HS()     → USER_status()
STORAGE_Read_HS()        → USER_read()
STORAGE_Write_HS()       → USER_write()
```

Vì `SD_Card_Reader` đã có `user_diskio.c` hoàn chỉnh, chỉ cần copy/cấu hình phần `USB_DEVICE/` và sửa `usbd_storage_if.c` tương tự.

---

## 4. Các thay đổi cần thực hiện

### Phương án khuyến nghị

**Giữ `sd_spi.c`** (driver hiện tại) + **thêm UART, USB MSC, cập nhật `main.c`** theo luồng tham chiếu. Không cần thay bằng `user_diskio_spi.c` trừ khi gặp lỗi tương thích trên board.

---

### Bước 1 — Cấu hình CubeMX (`SD_Card.ioc`)

Mở `SD_Card.ioc` trong STM32CubeMX, bổ sung:

#### 1.1. USART1

| Thiết lập | Giá trị |
| --- | --- |
| Mode | Asynchronous |
| PA9 | USART1_TX |
| PA10 | USART1_RX |
| Baud Rate | 115200 |
| Word Length | 8 Bits |
| Stop Bits | 1 |
| Parity | None |

#### 1.2. USB OTG HS (Device Only)

| Thiết lập | Giá trị |
| --- | --- |
| Mode | Device_Only |
| Class | Mass Storage Class (MSC) |
| PB13 | USB_OTG_HS_VBUS |
| PB14 | USB_OTG_HS_DM |
| PB15 | USB_OTG_HS_DP |

Trong **Middleware → USB_DEVICE**: chọn `Class for HS IP` = **Mass Storage Class**.

#### 1.3. Clock — PLLQ cho USB 48 MHz

Đổi **PLLQ** từ `4` → `3`:

- Với HSE 8 MHz, PLLM=4, PLLN=72: VCO = 144 MHz
- PLLQ = 3 → **48 MHz** (yêu cầu USB)
- Cập nhật `SystemClock_Config()` trong `main.c`: `RCC_OscInitStruct.PLL.PLLQ = 3`

#### 1.4. Generate Code

- Generate lại project, chọn **Copy only relevant** hoặc merge cẩn thận để không ghi đè `sd_spi.c` và logic trong `USER CODE` sections.

---

### Bước 2 — Bật HAL modules

Trong `Core/Inc/stm32f4xx_hal_conf.h`, bỏ comment:

```c
#define HAL_UART_MODULE_ENABLED
#define HAL_PCD_MODULE_ENABLED
```

Sau generate CubeMX, kiểm tra các file HAL driver được thêm vào project:

- `stm32f4xx_hal_uart.c`
- `stm32f4xx_hal_pcd.c`, `stm32f4xx_ll_usb.c`

---

### Bước 3 — Thêm UART debug logger

#### 3.1. `Core/Inc/main.h`

Thêm prototype và (tuỳ chọn) macro SPI:

```c
void myprintf(const char *fmt, ...);

/* Tuỳ chọn — sd_spi.c dùng extern hspi1 trực tiếp, không bắt buộc */
#define SD_SPI_HANDLE hspi1
```

#### 3.2. `Core/Src/main.c`

Thêm includes và hàm `myprintf`:

```c
#include <stdarg.h>

UART_HandleTypeDef huart1;  /* do CubeMX sinh */

void myprintf(const char *fmt, ...) {
    static char buffer[256];
    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, sizeof(buffer), fmt, args);
    va_end(args);
    HAL_UART_Transmit(&huart1, (uint8_t*)buffer, strlen(buffer), HAL_MAX_DELAY);
}
```

Gọi `MX_USART1_UART_Init()` trong `main()` **trước** thao tác SD.

---

### Bước 4 — Cập nhật luồng demo `main.c`

Thay `SD_FileTest()` bằng luồng tương tự Nhung-Vao-Tim-Em (có thể giữ `SD_FileTest` như hàm phụ):

```c
myprintf("\r\n~ SD card demo ~\r\n\r\n");
HAL_Delay(1000);

FATFS FatFs;
FIL fil;
FRESULT fres;

/* 1. Mount */
fres = f_mount(&FatFs, "", 1);
while (fres != FR_OK) {
    myprintf("f_mount error (%i)\r\n", fres);
    HAL_Delay(1000);
}

/* 2. Thống kê dung lượng */
DWORD free_clusters;
FATFS *getFreeFs;
fres = f_getfree("", &free_clusters, &getFreeFs);
if (fres == FR_OK) {
    DWORD total_sectors = (getFreeFs->n_fatent - 2) * getFreeFs->csize;
    DWORD free_sectors  = free_clusters * getFreeFs->csize;
    myprintf("SD card stats:\r\n%10lu KiB total.\r\n%10lu KiB free.\r\n",
             total_sectors / 2, free_sectors / 2);
}

/* 3. Đọc test.txt (tạo sẵn trên thẻ trước khi test) */
fres = f_open(&fil, "test.txt", FA_READ);
if (fres == FR_OK) {
    BYTE readBuf[30];
    if (f_gets((TCHAR*)readBuf, 30, &fil)) {
        myprintf("Read from test.txt: %s\r\n", readBuf);
    }
    f_close(&fil);
} else {
    myprintf("f_open test.txt error (%i)\r\n", fres);
}

/* 4. Ghi write.txt */
fres = f_open(&fil, "write.txt", FA_WRITE | FA_OPEN_ALWAYS | FA_CREATE_ALWAYS);
if (fres == FR_OK) {
  const char *msg = "a new file is made!";
  UINT bw;
  f_write(&fil, msg, 19, &bw);
  myprintf("Wrote %u bytes to write.txt\r\n", bw);
  f_close(&fil);
}

/* 5. Unmount (tuỳ chọn nếu còn dùng USB sau đó) */
f_mount(NULL, "", 0);
```

**Lưu ý:** Chuẩn bị file `test.txt` trên thẻ SD trước khi nạp firmware (khác với demo hiện tại tự tạo file).

---

### Bước 5 — Tích hợp USB Mass Storage

#### 5.1. Copy cấu trúc từ dự án tham chiếu

Sau khi generate CubeMX, project sẽ có:

```text
USB_DEVICE/
├── App/
│   ├── usb_device.c
│   ├── usbd_desc.c
│   └── usbd_storage_if.c   ← cần sửa USER CODE
└── Target/
    └── usbd_conf.c

Middlewares/ST/STM32_USB_Device_Library/
├── Class/MSC/
└── Core/
```

#### 5.2. Sửa `USB_DEVICE/App/usbd_storage_if.c`

Trong các `USER CODE` block, implement giống Nhung-Vao-Tim-Em:

```c
#include "ff_gen_drv.h"
#include "user_diskio.h"

int8_t STORAGE_Init_HS(uint8_t lun) {
    return (USER_initialize(lun) == 0) ? USBD_OK : USBD_FAIL;
}

int8_t STORAGE_GetCapacity_HS(uint8_t lun, uint32_t *block_num, uint16_t *block_size) {
    DWORD sector_count = 0;
    WORD sector_size = 0;
    if (USER_ioctl(lun, GET_SECTOR_COUNT, &sector_count) != RES_OK) return USBD_FAIL;
    if (USER_ioctl(lun, GET_SECTOR_SIZE, &sector_size) != RES_OK) return USBD_FAIL;
    *block_num = sector_count;
    *block_size = sector_size;
    return USBD_OK;
}

int8_t STORAGE_IsReady_HS(uint8_t lun) {
    return (USER_status(lun) == 0) ? USBD_OK : USBD_FAIL;
}

int8_t STORAGE_Read_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len) {
    return (USER_read(lun, buf, blk_addr, blk_len) == RES_OK) ? USBD_OK : USBD_FAIL;
}

int8_t STORAGE_Write_HS(uint8_t lun, uint8_t *buf, uint32_t blk_addr, uint16_t blk_len) {
    return (USER_write(lun, buf, blk_addr, blk_len) == RES_OK) ? USBD_OK : USBD_FAIL;
}
```

#### 5.3. Gọi init USB trong `main.c`

```c
#include "usb_device.h"

// Trong main(), sau MX_FATFS_Init():
MX_USB_DEVICE_Init();
```

#### 5.4. Cảnh báo xung đột FatFs ↔ USB

Nếu PC mount thẻ qua USB **đồng thời** firmware gọi `f_open`/`f_write`, có thể corrupt filesystem. Demo tham chiếu unmount FatFs (`f_mount(NULL,...)`) trước khi PC dùng USB, hoặc chỉ dùng một kênh tại một thời điểm. Production nên thêm mutex/flag trạng thái.

---

### Bước 6 — Cập nhật `stm32f4xx_hal_msp.c` và interrupt

CubeMX sẽ sinh thêm:

- `HAL_UART_MspInit()` — PA9/PA10
- `HAL_PCD_MspInit()` — PB13/14/15
- Handler `OTG_HS_IRQHandler` trong `stm32f4xx_it.c`

Kiểm tra sau generate, không xóa phần SPI MSP hiện có.

---

### Bước 7 — Tài liệu và Git

#### 7.1. Tạo `README.md`

Nội dung tối thiểu (tham khảo `Nhung-Vao-Tim-Em/README.md`):

- Giới thiệu đề bài SD card SPI + FatFs
- Bảng thành viên nhóm
- Sơ đồ nối dây (PG13 CS, PA5/6/7 SPI, PA9/10 UART)
- Hướng dẫn build/nạp/test
- Link demo video (nếu có)

#### 7.2. Thêm `.gitignore`

```gitignore
Debug/
Release/
*.o
*.d
*.elf
*.bin
*.hex
*.map
*.list
*.su
*.cyclo
.metadata/
```

#### 7.3. Xóa `Debug/` khỏi Git (nếu đã commit)

```bash
git rm -r --cached Debug/
```

---

## 5. Bảng checklist thay đổi theo file

| File / Thư mục | Hành động | Mức độ |
| --- | --- | --- |
| `SD_Card.ioc` | Thêm USART1, USB OTG HS MSC, PLLQ=3 | Bắt buộc |
| `Core/Inc/stm32f4xx_hal_conf.h` | Bật HAL_UART, HAL_PCD | Bắt buộc |
| `Core/Src/main.c` | Thêm UART, USB init, luồng demo mới | Bắt buộc |
| `Core/Inc/main.h` | Thêm `myprintf`, tuỳ chọn `SD_SPI_HANDLE` | Bắt buộc |
| `Core/Src/stm32f4xx_hal_msp.c` | UART + USB MSP (CubeMX generate) | Bắt buộc |
| `Core/Src/stm32f4xx_it.c` | USB IRQ handler (CubeMX generate) | Bắt buộc |
| `USB_DEVICE/**` | Thêm mới (CubeMX generate + sửa storage_if) | Bắt buộc |
| `Middlewares/ST/**` | Thêm USB Device Library | Bắt buộc |
| `Core/Src/sd_spi.c` | **Giữ nguyên** (đã đủ chức năng) | Không đổi |
| `FATFS/Target/user_diskio.c` | **Giữ nguyên** (đã nối sd_spi) | Không đổi |
| `README.md` | Tạo mới | Nên có |
| `.gitignore` | Tạo mới | Nên có |
| `docs/` | Tài liệu này + overview | Nên có |

---

## 6. Lộ trình thực hiện đề xuất

```text
Giai đoạn 1 — UART (dễ debug nhất)
  ├── CubeMX: bật USART1
  ├── Thêm myprintf()
  ├── Sửa main.c in log mount/read/write
  └── Test: terminal 115200 thấy log

Giai đoạn 2 — Hoàn thiện demo FatFs
  ├── f_getfree, đọc test.txt, ghi write.txt
  └── Checklist: file trên thẻ khớp log UART

Giai đoạn 3 — USB MSC
  ├── CubeMX: USB OTG HS, PLLQ=3
  ├── Generate + sửa usbd_storage_if.c
  ├── Test PC nhận ổ USB
  └── Tránh FatFs + USB cùng lúc

Giai đoạn 4 — Tài liệu & dọn repo
  ├── README.md, .gitignore
  └── Bỏ Debug/ khỏi Git
```

**Thứ tự quan trọng:** UART trước → FatFs demo → USB MSC cuối.

---

## 7. Checklist kiểm thử

| Bước | Chuẩn bị | Kết quả mong đợi |
| --- | --- | --- |
| UART | Mở terminal 115200 | Thấy banner và log |
| Mount SD | Thẻ FAT32, cắm đúng dây SPI | Không lặp `f_mount error` |
| Dung lượng | — | In tổng/trống (KiB) |
| Đọc | File `test.txt` có sẵn trên thẻ | In nội dung file |
| Ghi | — | Tạo `write.txt`, 19 byte |
| USB MSC | Cáp USB, đã unmount FatFs | PC thấy removable disk |
| Đọc lại thẻ | Tháo thẻ hoặc dùng USB | File `write.txt` tồn tại |

---

## 8. So sánh driver: giữ `sd_spi.c` hay chuyển sang `user_diskio_spi.c`?

| Tiêu chí | `sd_spi.c` (hiện tại) | `user_diskio_spi.c` (tham chiếu) |
| --- | --- | --- |
| Nguồn gốc | Tự viết | Port ChaN/kiwih |
| Vị trí file | `Core/Src/` | `FATFS/Target/` |
| Đổi tốc độ SPI khi init | Prescaler cố định 128 | FCLK_SLOW / FCLK_FAST |
| Tích hợp user_diskio | Gọi `SD_*` API | Gọi `USER_SPI_*` |
| Khuyến nghị | **Giữ** nếu board đã chạy | Chỉ dùng nếu sd_spi lỗi |

Nếu muốn **giống hệt** cấu trúc file Nhung-Vao-Tim-Em: đổi `user_diskio.c` include `user_diskio_spi.h`, copy `user_diskio_spi.c/h` từ dự án tham chiếu, xóa `sd_spi.c/h`, thêm `#define SD_SPI_HANDLE hspi1` vào `main.h`. Đây là refactor lớn hơn và **không bắt buộc** về mặt chức năng.

---

## 9. Tóm tắt

**SD_Card_Reader** đã hoàn thành phần khó nhất: driver SD-SPI + FatFs disk I/O + demo đọc/ghi im lặng. Để ngang tầm **Nhung-Vao-Tim-Em**, cần chủ yếu:

1. Bật **USART1** và `myprintf()` để debug.
2. Đổi luồng `main.c` sang mount → stats → đọc `test.txt` → ghi `write.txt`.
3. Bật **USB MSC** (PLLQ=3, thư mục `USB_DEVICE/`, sửa `usbd_storage_if.c`).
4. Bổ sung **README**, **.gitignore**, tài liệu nhóm.

Driver `sd_spi.c` hiện tại **đủ tốt để giữ** — không cần thay bằng `user_diskio_spi.c` trừ khi có yêu cầu đồng bộ cấu trúc repo với dự án tham chiếu.

---

*Tài liệu được tạo từ phân tích so sánh codebase `SD_Card_Reader/` và `Nhung-Vao-Tim-Em/`.*
