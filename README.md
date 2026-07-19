# SD Card Reader — STM32F429 + FatFS + TouchGFX

## Giới thiệu

**Đề bài:** SD Card Reader

**Sản phẩm:**

1. Đọc/ghi thẻ nhớ micro SD trên STM32F429 với hệ thống file **FAT/FatFS**
2. Giao tiếp thẻ SD qua giao thức **SPI**
3. Hiển thị thông tin dung lượng thẻ trên màn hình LCD (TouchGFX)
4. Điều khiển đọc/ghi file từ máy tính qua **UART** (ST-Link VCP)



---

## Tác giả

- **Tên nhóm:** Nhúng lẩu IOT

| STT | Họ tên | MSSV | Công việc |
| --: | ------ | ---- | --------- |
| 1   | Trương Ngọc Hải | 20225309 | Kết nối phần cứng, giao tiếp SPI và driver thẻ SD, kiểm thử và hoàn thiện tài liệu |
| 2   | Từ Minh Tuân | 20225422 | Giao tiếp UART với PC, tích hợp FatFs và quản lý tệp, kiểm thử và hoàn thiện tài liệu |
| 3   | Nguyễn Huy Mạnh | 20225143 | Thiết kế giao diện TouchGFX và hiển thị thông tin thẻ trên LCD |
| 4   | Cao Văn Bảo | 20225166 | Tích hợp FreeRTOS, đồng bộ tài nguyên |

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
| **PuTTY / Hercules** | Client trên PC |

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

### Ý nghĩa các tín hiệu và lệnh SD qua SPI

> **Phân biệt:** `CS`, `SCK`, `MOSI`, `MISO` là các tín hiệu vật lý của SPI; `CMD0`, `CMD8`, `CMD17`... là lệnh của giao thức thẻ SD được truyền qua SPI.

| Tín hiệu / thuật ngữ | Ý nghĩa | Mục đích trong dự án |
| -------------------- | ------- | -------------------- |
| **CS** (Chip Select) | Tín hiệu chọn thiết bị, tác động mức thấp | `CS = LOW` để bắt đầu trao đổi với thẻ; `CS = HIGH` để kết thúc và bỏ chọn thẻ |
| **SCK / CLK** | Xung nhịp do STM32 tạo ra | Đồng bộ từng bit dữ liệu giữa STM32 và thẻ SD |
| **MOSI / DI** | Master Out, Slave In | Truyền command, địa chỉ sector và dữ liệu từ STM32 đến thẻ |
| **MISO / DO** | Master In, Slave Out | Nhận response, token và dữ liệu từ thẻ về STM32 |
| **0xFF** | Byte giả (dummy byte) | Tạo thêm xung clock khi STM32 cần chờ hoặc đọc dữ liệu từ thẻ |
| **R1** | Byte phản hồi trạng thái của thẻ | Cho biết lệnh thành công hay thẻ đang idle/lỗi; thường `0x01` là idle và `0x00` là sẵn sàng |
| **Data token 0xFE** | Dấu hiệu bắt đầu một khối dữ liệu | Báo rằng 512 byte dữ liệu sector sắp được truyền |
| **CRC** | Mã kiểm tra lỗi | CMD0 và CMD8 cần CRC hợp lệ khi khởi tạo; mỗi block dữ liệu có thêm 2 byte CRC |

#### Các command được sử dụng

| Command | Tên / ý nghĩa | Mục đích |
| ------- | ------------- | -------- |
| **CMD0** | GO_IDLE_STATE | Reset và đưa thẻ về trạng thái Idle để bắt đầu quy trình khởi tạo |
| **CMD8** | SEND_IF_COND | Kiểm tra thẻ hỗ trợ SD version 2 và dải điện áp; mẫu `0x1AA` giúp xác nhận đường truyền |
| **CMD55** | APP_CMD | Báo cho thẻ rằng command tiếp theo là một application command |
| **ACMD41** | SD_SEND_OP_COND | Được gửi lặp sau CMD55 để yêu cầu thẻ hoàn tất khởi tạo; bit HCS đề nghị hỗ trợ thẻ dung lượng cao |
| **CMD58** | READ_OCR | Đọc thanh ghi OCR; bit CCS dùng để phân biệt SDHC/SDXC với SDSC |
| **CMD9** | SEND_CSD | Đọc thanh ghi CSD để lấy thông tin và tính dung lượng thẻ |
| **CMD17** | READ_SINGLE_BLOCK | Đọc một sector 512 byte từ địa chỉ được yêu cầu |
| **CMD24** | WRITE_SINGLE_BLOCK | Ghi một sector 512 byte vào địa chỉ được yêu cầu |

#### Trình tự hoạt động

**Khởi tạo thẻ:**

1. Đưa `CS` lên HIGH và phát ít nhất 74 xung clock để thẻ vào chế độ SPI.
2. Kéo `CS` xuống LOW, gửi **CMD0** để đưa thẻ vào Idle.
3. Gửi **CMD8** để kiểm tra SD v2 và điện áp hoạt động.
4. Lặp **CMD55 + ACMD41** đến khi phản hồi R1 bằng `0x00`, nghĩa là thẻ đã sẵn sàng.
5. Gửi **CMD58** để đọc OCR và phân loại thẻ SDHC/SDXC hoặc SDSC.

**Đọc sector:** gửi **CMD17** → chờ token `0xFE` → nhận 512 byte → đọc bỏ 2 byte CRC.

**Ghi sector:** gửi **CMD24** → gửi token `0xFE` → gửi 512 byte → gửi 2 byte CRC → kiểm tra data response và chờ thẻ hết busy.

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

[▶ Xem video demo sản phẩm trên Google Drive](https://drive.google.com/file/d/1wk_BiRm1yo9poMiqXb6vDgiFu_37Wtcm/view)

---

## Ghi chú kỹ thuật

- SPI1 baud init mặc định thấp (prescaler 256) để ổn định lúc init card; có thể tăng tốc sau khi init tùy thiết kế driver.  
- Sector size: **512 byte**.  
- Không dùng USB Mass Storage trong phiên bản hiện tại — truy cập PC qua **UART + FatFs**.  
- Heap FreeRTOS / stack GUI cần đủ lớn; UART chạy chung `defaultTask` để tránh hết heap khi tách task riêng.

### Hạn chế hiện tại

- Quy trình khởi tạo yêu cầu **CMD8** thành công nên chưa hỗ trợ đầy đủ thẻ SD version 1 cũ.
- Driver nhận diện được SDSC/SDHC nhưng địa chỉ đọc/ghi hiện phù hợp chủ yếu với **SDHC/SDXC**; SDSC cần chuyển địa chỉ sector sang địa chỉ byte.
- Tốc độ SPI vẫn dùng prescaler thấp để ưu tiên ổn định khi khởi tạo, chưa chuyển sang tốc độ cao sau khi thẻ sẵn sàng.
- Đọc/ghi nhiều sector được thực hiện bằng cách lặp **CMD17/CMD24**, chưa dùng lệnh multi-block **CMD18/CMD25** hoặc DMA.
- Chưa hỗ trợ phát hiện cắm/rút thẻ và tự động mount lại; nên lắp thẻ trước khi cấp nguồn hoặc reset.
- Giao tiếp với PC chỉ là giao thức text qua UART, chưa hoạt động như một thiết bị **USB Mass Storage**.
- Lệnh UART `READ` chỉ trả về khoảng 511 byte mỗi lần; danh sách thư mục và số lượng file hiển thị còn giới hạn.
- FatFs đang tắt **Long File Name**, do đó tên file chỉ hỗ trợ định dạng 8.3.
- Việc truy cập thẻ còn mang tính blocking; GUI và UART phải dùng chung mutex nên một thao tác lâu có thể làm tác vụ còn lại phải chờ.
- Chưa có bộ kiểm thử tự động; việc đánh giá hiện chủ yếu dựa trên kiểm thử thủ công và các hàm chẩn đoán.

### Hướng phát triển

- Hoàn thiện hỗ trợ SD v1/v2, SDSC, SDHC và SDXC với cách tính địa chỉ đúng cho từng loại thẻ.
- Tăng SPI clock sau khi khởi tạo; áp dụng DMA và **CMD18/CMD25** để nâng tốc độ đọc/ghi nhiều sector.
- Bổ sung chân Card Detect, xử lý cắm/rút nóng và cơ chế tự động unmount/mount an toàn.
- Mở rộng giao thức UART theo cơ chế phân mảnh dữ liệu, phân trang danh sách file, checksum và phản hồi lỗi chi tiết.
- Bật Long File Name, hỗ trợ thư mục con và các thao tác file có kích thước lớn.
- Xây dựng storage task riêng sử dụng queue để giảm thời gian blocking và điều phối truy cập từ GUI/UART tốt hơn.
- Phát triển chế độ USB Mass Storage để máy tính nhận thẻ như một ổ đĩa di động.
- Bổ sung thống kê tốc độ, nhật ký lỗi, cơ chế phục hồi timeout và bộ kiểm thử tự động cho driver SD, FatFs và UART.

---

## Tài liệu tham khảo

- SD Association, [*SD Specifications — Part 1: Physical Layer Simplified Specification, Version 2.00*](Part1_Physical_Layer_Simplified_Specification_Ver2.00.pdf).

---

## License

Phần code ST HAL / Cube / TouchGFX tuân theo giấy phép tương ứng của STMicroelectronics.  

