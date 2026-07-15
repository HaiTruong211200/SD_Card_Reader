# SD Card Reader — STM32F429 + FatFS + TouchGFX

## Giới thiệu

**Đề bài:** SD Card Reader

**Sản phẩm:**

1. Đọc/ghi thẻ nhớ micro SD trên STM32F429 với hệ thống file **FAT/FatFS**
2. Giao tiếp thẻ SD qua giao thức **SPI**
3. Hiển thị thông tin dung lượng thẻ trên màn hình LCD (TouchGFX)
4. Điều khiển đọc/ghi file từ máy tính qua **UART** (ST-Link VCP)

> *(Thêm hình ảnh sơ đồ / demo tại đây nếu có)*

---

## Tác giả

| STT | Họ tên | MSSV | Công việc |
| --: | ------ | ---- | --------- |
| 1   | *Điền sau* | *Điền sau* | *Điền sau* |
| 2   | *Điền sau* | *Điền sau* | *Điền sau* |
| 3   | *Điền sau* | *Điền sau* | *Điền sau* |

- **Tên nhóm:** *Điền sau*

---

## Môi trường hoạt động

### Phần cứng

| Thành phần | Mô tả |
| ---------- | ----- |
| **STM32F429I-DISC1** | Kit Discovery, MCU STM32F429ZIT6 |
| **Module micro SD SPI** | Bộ đọc thẻ giao diện SPI |
| **Micro SD card** | Định dạng FAT12/FAT16/FAT32 |
| **PC** | Giám sát UART, gửi lệnh đọc/ghi file |

### Phần mềm / thư viện

| Thành phần | Vai trò |
| ---------- | ------- |
| **STM32CubeIDE** | Phát triển, biên dịch, nạp firmware |
| **STM32CubeMX** | Cấu hình chân (file `.ioc`) |
| **FatFs** | Hệ thống tập tin FAT trên thẻ SD |
| **FreeRTOS (CMSIS-RTOS v2)** | Đa nhiệm: GUI + UART |
| **TouchGFX** | Giao diện LCD cảm ứng, hiển thị dung lượng thẻ |
| **HAL SPI / UART** | Driver lớp thấp SPI1 (SD) và USART1 (debug/PC) |
| **PuTTY / Hercules** hoặc `tools/uart_pc_cli.py` | Client trên PC |

---

## Sơ đồ kết nối (Schematic)

> **Lưu ý:** Trên Discovery F429, chân **PA6** dùng cho LTDC (LCD), nên **SPI1_MISO** được nối qua **PB4**, không dùng PA6 như một số module SD chuẩn.

| STM32F429I-DISC1 | Module micro SD SPI | Chức năng |
| ---------------- | ------------------- | --------- |
| **PG13** (`SD_CS`) | CS | Chip Select |
| **PA5** (SPI1_SCK) | SCK / CLK | Clock |
| **PB4** (SPI1_MISO) | MISO / DO | STM32 ← SD |
| **PA7** (SPI1_MOSI) | MOSI / DI | STM32 → SD |
| **5V** / **3.3V*** | VCC | Nguồn (*tuỳ module; ưu tiên đúng mức module*) |
| **GND** | GND | Mass |

**UART debug / PC (qua ST-Link VCP):**

| STM32 | Chức năng |
| ----- | --------- |
| USART1 TX (PA9) / RX (PA10) | 115200 8N1 — giao thức lệnh PC ↔ SD |

---

## Kiến trúc hệ thống

```
┌─────────────┐     SPI1      ┌──────────────┐
│  STM32F429  │◄─────────────►│  Micro SD     │
│  DISCOVERY  │  (CS=PG13)    │  + module SPI │
└──────┬──────┘               └──────────────┘
       │
       ├─ FatFs  ←→  user_diskio  ←→  sd_spi (CMD0/8/41/17/24…)
       │
       ├─ TouchGFX (GUI_Task)     → hiển thị total/used/free
       │
       └─ UART USART1 (defaultTask) → lệnh LIST/READ/WRITE/…
                ▲
                │ ST-Link VCP
           ┌────┴────┐
           │   PC    │
           └─────────┘
```

### Phần cứng — vai trò

| Thành phần | Vai trò |
| ---------- | ------- |
| STM32F429ZIT6 | MCU chính: SPI SD, FatFs, GUI, UART |
| Micro SD | Lưu trữ file (FAT) |
| Module đọc thẻ SPI | Chuyển mức / giao diện SPI ↔ socket thẻ |
| LCD ILI9341 + cảm ứng | Hiển thị trạng thái thẻ (TouchGFX) |
| PC | Gửi lệnh, ghi log qua UART |

### Phần mềm — vai trò

| Thành phần | File / vị trí chính | Vai trò |
| ---------- | ------------------- | ------- |
| Driver SPI SD | `STM32CubeIDE/Application/User/sd_spi.c` | Init card (CMD0/8/ACMD41/CMD58), đọc/ghi block |
| FatFs glue | `FATFS/Target/user_diskio.c` | Ánh xạ FatFs ↔ `SD_ReadBlocks` / `SD_WriteBlocks` |
| Storage API | `STM32CubeIDE/Application/User/storage_manager.c` | Mount, CRUD file, list, info |
| UART PC protocol | `STM32CubeIDE/Application/User/uart_pc.c` | Parse lệnh HELP/LIST/READ/WRITE/… |
| Main + RTOS | `Core/Src/main.c` | Init ngoại vi, FreeRTOS tasks |
| TouchGFX Model | `TouchGFX/gui/src/model/Model.cpp` | Đọc `StorageInfo` định kỳ, cập nhật UI |
| PC CLI | `tools/uart_pc_cli.py` | Client Python gửi lệnh qua COM |

### Luồng khởi động

1. `HAL_Init` → clock → GPIO / SPI1 / USART1 / LCD / SDRAM / FatFs / TouchGFX  
2. `SD_Init()` → `Storage_Mount()`  
3. FreeRTOS:
   - **GUI_Task** — TouchGFX (độ ưu tiên Normal)
   - **defaultTask** — sau ~2 s gọi `UartPc_Task` (độ ưu tiên Low)
4. Mutex shared FatFs giữa GUI và UART (`Storage_TryLock` / `Storage_Unlock`)

---

## Cấu trúc thư mục (tóm tắt)

```
SD_Card_Reader/
├── Core/                         # HAL, main, FreeRTOS config, header API
│   ├── Inc/                      # sd_spi.h, storage_manager.h, uart_pc.h, main.h
│   └── Src/                      # main.c, freertos.c, …
├── FATFS/
│   ├── App/                      # fatfs.c / fatfs.h (CubeMX)
│   └── Target/                   # user_diskio.c, ffconf.h
├── STM32CubeIDE/Application/User/
│   ├── sd_spi.c                  # Driver SPI thẻ SD
│   ├── storage_manager.c         # API file / mount / test
│   └── uart_pc.c                 # Giao thức UART với PC
├── TouchGFX/                     # UI TouchGFX (Screen1: dung lượng thẻ)
├── Middlewares/Third_Party/
│   ├── FatFs/
│   └── FreeRTOS/
├── tools/
│   └── uart_pc_cli.py            # Client lệnh UART trên PC
└── STM32F429I_DISCO_REV_D01.ioc  # Cấu hình CubeMX
```

---

## Đặc tả hàm chính

### `sd_spi` — driver khối SPI

**File:** [`Core/Inc/sd_spi.h`](Core/Inc/sd_spi.h), [`STM32CubeIDE/Application/User/sd_spi.c`](STM32CubeIDE/Application/User/sd_spi.c)

```c
/**
 * @brief  Khởi tạo thẻ SD qua SPI (idle clocks, CMD0, CMD8, ACMD41, CMD58…).
 * @retval true nếu thẻ sẵn sàng đọc/ghi block.
 */
bool SD_Init(void);

/**
 * @brief  Kiểm tra thẻ đã init thành công.
 */
bool SD_IsReady(void);

/**
 * @brief  Đọc một hoặc nhiều sector (512 byte) từ thẻ (CMD17…).
 * @param  sector  Địa chỉ LBA bắt đầu
 * @param  buffer  Bộ đệm đích
 * @param  count   Số sector
 */
bool SD_ReadBlocks(uint32_t sector, uint8_t *buffer, uint32_t count);

/**
 * @brief  Ghi một hoặc nhiều sector xuống thẻ (CMD24…).
 */
bool SD_WriteBlocks(uint32_t sector, const uint8_t *buffer, uint32_t count);

/**
 * @brief  Trả về kiểu thẻ: SDSC / SDHC / unknown.
 */
SD_CardType SD_GetCardType(void);
```

### `user_diskio` — lớp FatFs Disk I/O

**File:** [`FATFS/Target/user_diskio.c`](FATFS/Target/user_diskio.c)

```c
DSTATUS USER_initialize(BYTE pdrv);   /* Gọi SD_Init nếu chưa ready */
DSTATUS USER_status(BYTE pdrv);       /* STA_NOINIT nếu card không sẵn sàng */
DRESULT USER_read(BYTE pdrv, BYTE *buff, DWORD sector, UINT count);
DRESULT USER_write(BYTE pdrv, const BYTE *buff, DWORD sector, UINT count);
DRESULT USER_ioctl(BYTE pdrv, BYTE cmd, void *buff);  /* CTRL_SYNC, … */
```

### FatFs — API dùng trong project

**File:** `Middlewares/Third_Party/FatFs/src/ff.c`

```c
FRESULT f_mount(FATFS* fs, const TCHAR* path, BYTE opt);
FRESULT f_getfree(const TCHAR* path, DWORD* nclst, FATFS** fatfs);
FRESULT f_open(FIL* fp, const TCHAR* path, BYTE mode);
FRESULT f_read(FIL* fp, void* buff, UINT btr, UINT* br);
FRESULT f_write(FIL* fp, const void* buff, UINT btw, UINT* bw);
FRESULT f_close(FIL* fp);
FRESULT f_unlink(const TCHAR* path);
FRESULT f_opendir(DIR* dp, const TCHAR* path);
FRESULT f_readdir(DIR* dp, FILINFO* fno);
```

### `storage_manager` — lớp ứng dụng

**File:** [`Core/Inc/storage_manager.h`](Core/Inc/storage_manager.h)

```c
bool Storage_Mount(void);
bool Storage_GetInfo(void);
bool Storage_ReadInfo(StorageInfo *info);
bool Storage_ListRoot(void);

bool Storage_WriteFile(const char *path, const void *data, UINT len, UINT *written);
bool Storage_AppendFile(const char *path, const void *data, UINT len, UINT *written);
bool Storage_ReadFile(const char *path, void *buf, UINT buflen, UINT *read);
bool Storage_DeleteFile(const char *path);
bool Storage_FileExists(const char *path);

int  Storage_TryLock(uint32_t timeout_ms);  /* Mutex GUI ↔ UART */
void Storage_Unlock(void);
```

### `uart_pc` — giao thức lệnh PC

**File:** [`Core/Inc/uart_pc.h`](Core/Inc/uart_pc.h)

```c
void UartPc_Init(UART_HandleTypeDef *huart);
void UartPc_Task(void *argument);   /* Vòng lặp nhận dòng lệnh */
void UartPc_OnRxIrq(void);          /* ISR đẩy ký tự vào ring buffer */
```

---

## Giao thức UART (PC ↔ board)

- **Cổng:** COM ảo ST-Link (USART1)  
- **Cấu hình:** `115200 8N1`  
- **Khuyến nghị PuTTY:** Local echo = Force off, Local line editing = Force on  
- Đợi prompt `> ` rồi mới gõ lệnh tiếp theo

| Lệnh | Mô tả | Ví dụ phản hồi |
| ---- | ----- | -------------- |
| `HELP` | Danh sách lệnh | `OK Work4…` / `END` |
| `PING` | Kiểm tra kết nối | `OK PONG` |
| `LIST` | Liệt kê file ở thư mục gốc | `OK count=…` + tên file + `END` |
| `INFO` | Dung lượng thẻ | `OK total_kb=… used_kb=… free_kb=… files=…` |
| `READ <file>` | Đọc nội dung file (≤ ~511 byte buffer) | `OK size=…` + data + `END` |
| `WRITE <file> <text>` | Ghi đè / tạo file | `OK written=…` |
| `APPEND <file> <text>` | Nối thêm nội dung | `OK written=…` |
| `DELETE <file>` | Xóa file | `OK` |

### Client Python

```bash
pip install pyserial

python tools/uart_pc_cli.py COM5
python tools/uart_pc_cli.py COM5 LIST
python tools/uart_pc_cli.py COM5 WRITE demo.txt Hello from PC
python tools/uart_pc_cli.py COM5 READ demo.txt
python tools/uart_pc_cli.py COM5 INFO
```

*(Đổi `COM5` theo Device Manager trên Windows.)*

---

## Giao diện TouchGFX

- Màn hình **Screen1** hiển thị thông tin thẻ: tổng / đã dùng / còn trống, số file.  
- `Model::tick()` định kỳ gọi `Storage_ReadInfo()` (có mutex) rồi cập nhật View.  
- GUI và UART dùng chung FatFs qua `Storage_TryLock` để tránh xung đột truy cập.

---

## Cách build & nạp

1. Mở project bằng **STM32CubeIDE** (workspace chứa `STM32CubeIDE/`).  
2. Build cấu hình **Debug**.  
3. Nạp firmware qua ST-Link trên kit Discovery.  
4. Cắm module SD theo bảng chân ở trên, gắn thẻ đã format FAT.  
5. Mở terminal UART (`115200`) hoặc chạy `uart_pc_cli.py` để thử lệnh.

Có thể mở lại `.ioc` bằng CubeMX khi cần chỉnh SPI/UART/GPIO; giữ nguyên USER CODE trong `user_diskio.c` và các file User.

---

## Kết quả / Demo

- Demo sản phẩm: *(điền link Drive / video sau)*  
- Checklist thử nghiệm gợi ý:
  - [ ] `SD_Init` / mount FatFs thành công  
  - [ ] TouchGFX hiện đúng dung lượng thẻ  
  - [ ] `PING` / `INFO` / `LIST` qua UART  
  - [ ] `WRITE` → `READ` → `APPEND` → `DELETE`  
  - [ ] GUI vẫn cập nhật khi UART đang idle (mutex không treo)

---

## Ghi chú kỹ thuật

- SPI1 baud init mặc định thấp (prescaler 256) để ổn định lúc init card; có thể tăng tốc sau khi init tùy thiết kế driver.  
- Sector size: **512 byte**.  
- Không dùng USB Mass Storage trong phiên bản hiện tại — truy cập PC qua **UART + FatFs**.  
- Heap FreeRTOS / stack GUI cần đủ lớn; UART chạy chung `defaultTask` để tránh hết heap khi tách task riêng.

---

## License

Phần code ST HAL / Cube / TouchGFX tuân theo giấy phép tương ứng của STMicroelectronics.  
Phần ứng dụng nhóm: *(điền sau)*.
