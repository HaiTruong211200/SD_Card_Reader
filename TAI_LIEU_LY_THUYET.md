# Tài liệu lý thuyết dự án SD Card Reader

## 1. Tổng quan dự án

Dự án sử dụng kit **STM32F429I-DISC1** để:

1. Giao tiếp với thẻ microSD qua **SPI**.
2. Đọc và ghi dữ liệu theo từng sector 512 byte.
3. Dùng **FatFs** để thao tác với file và thư mục trên phân vùng FAT.
4. Hiển thị dung lượng thẻ trên LCD bằng **TouchGFX**.
5. Nhận lệnh quản lý file từ máy tính qua **UART**.
6. Chạy đồng thời giao diện và xử lý UART bằng **FreeRTOS**.

Luồng tổng quát:

```text
PC
 │ UART
 ▼
uart_pc.c
 │ gọi Storage API
 ▼
storage_manager.c
 │ gọi FatFs API
 ▼
FatFs
 │ gọi Disk I/O API
 ▼
user_diskio.c
 │ gọi driver SD
 ▼
sd_spi.c
 │ SPI1
 ▼
microSD
```

LCD cũng gọi `storage_manager.c` để lấy dung lượng và số file:

```text
TouchGFX Model → Storage_ReadInfo() → FatFs → SD → cập nhật View
```

---

## 2. API là gì?

### 2.1. Định nghĩa

**API** là viết tắt của **Application Programming Interface** — giao diện lập trình ứng dụng.

API là một tập hợp hàm, kiểu dữ liệu và quy tắc mà một tầng phần mềm công khai để tầng khác sử dụng. Người gọi chỉ cần biết:

- Tên hàm.
- Tham số đầu vào.
- Giá trị trả về.
- Ý nghĩa và điều kiện sử dụng.

Người gọi không bắt buộc phải biết chi tiết bên trong hàm được cài đặt như thế nào.

Trong dự án này, “API” chủ yếu là **API hàm C/C++**, không phải Web API hay REST API.

### 2.2. Ví dụ trong dự án

Storage API được khai báo trong `Core/Inc/storage_manager.h`:

```c
bool Storage_Mount(void);
bool Storage_ReadFile(const char *path, void *buf, UINT buflen, UINT *read);
bool Storage_WriteFile(const char *path, const void *data, UINT len, UINT *written);
bool Storage_DeleteFile(const char *path);
```

Tầng UART gọi các hàm này mà không cần biết FatFs gửi bao nhiêu lệnh SPI.

Driver SD cũng cung cấp API:

```c
bool SD_Init(void);
bool SD_ReadBlocks(uint32_t sector, uint8_t *buffer, uint32_t count);
bool SD_WriteBlocks(uint32_t sector, const uint8_t *buffer, uint32_t count);
```

### 2.3. Vì sao cần chia API?

- Giảm phụ thuộc giữa các tầng.
- Dễ đọc, bảo trì và kiểm thử.
- Có thể thay implementation mà ít ảnh hưởng code gọi.
- Mỗi tầng có một trách nhiệm rõ ràng.

Ví dụ: nếu đổi phần cứng từ SPI sang SDIO, phần lớn `storage_manager.c` vẫn có thể giữ nguyên nếu lớp Disk I/O bên dưới vẫn cung cấp đúng giao diện FatFs yêu cầu.

### 2.4. API và protocol khác nhau thế nào?

- **API**: cách hai module phần mềm gọi nhau, ví dụ `Storage_ReadFile(...)`.
- **Protocol**: quy tắc trao đổi dữ liệu giữa hai thiết bị hoặc hai đầu giao tiếp, ví dụ chuỗi `READ demo.txt\r\n` qua UART.

---

## 3. Driver, HAL và middleware

### 3.1. Driver

**Driver** là phần mềm điều khiển trực tiếp một thiết bị hoặc ngoại vi.

Trong dự án:

- `sd_spi.c` là driver thẻ SD ở chế độ SPI.
- Nó điều khiển chân CS, gửi command SD và truyền từng byte qua SPI.

### 3.2. HAL

**HAL** là viết tắt của **Hardware Abstraction Layer** — lớp trừu tượng phần cứng.

STM32 HAL cung cấp các hàm chung như:

```c
HAL_SPI_TransmitReceive(...);
HAL_UART_Transmit(...);
HAL_GPIO_WritePin(...);
HAL_Delay(...);
```

Nhờ HAL, code dễ đọc hơn so với việc tự thao tác trực tiếp từng thanh ghi. Đổi lại, HAL thường có thêm overhead và ít linh hoạt hơn code thanh ghi thuần.

### 3.3. Middleware

**Middleware** nằm giữa driver phần cứng và logic ứng dụng.

Trong dự án:

- **FatFs** là middleware hệ thống file.
- **FreeRTOS** là middleware hệ điều hành thời gian thực.
- **TouchGFX** là framework/middleware đồ họa.

Phân tầng:

```text
Ứng dụng:       storage_manager, uart_pc, GUI
Middleware:     FatFs, FreeRTOS, TouchGFX
Driver:         sd_spi, HAL driver
Phần cứng:      STM32, microSD, LCD
```

---

## 4. SPI

### 4.1. SPI là gì?

**SPI** là viết tắt của **Serial Peripheral Interface**. Đây là giao tiếp nối tiếp đồng bộ, thường dùng giữa một vi điều khiển master và một hoặc nhiều slave.

Dự án dùng STM32 làm **master**, thẻ SD làm **slave**.

### 4.2. Các dây tín hiệu

| Tín hiệu | Ý nghĩa | Chân trong dự án |
| --- | --- | --- |
| SCK | Clock do master tạo | PA5 |
| MOSI | Master Out, Slave In | PA7 |
| MISO | Master In, Slave Out | PB4 |
| CS | Chọn slave, active-low | PG13 |

PB4 được dùng làm MISO vì PA6 đang được phần LCD LTDC sử dụng.

### 4.3. Full-duplex

SPI là giao tiếp **full-duplex**: khi master gửi một bit trên MOSI, nó đồng thời nhận một bit trên MISO.

Muốn chỉ đọc dữ liệu từ SD, STM32 vẫn phải phát các byte giả, thường là `0xFF`, để tạo xung clock:

```c
received = SD_SPI_Transfer(0xFF);
```

### 4.4. CS có vai trò gì?

- `CS = LOW`: chọn thẻ, bắt đầu phiên giao tiếp.
- `CS = HIGH`: bỏ chọn thẻ.

Trong driver, CS được kéo thấp trước command và kéo cao sau khi hoàn thành.

### 4.5. SPI mode và tốc độ

SPI có bốn mode phụ thuộc vào CPOL và CPHA. Thẻ SD ở SPI mode thường dùng **mode 0**.

Khi khởi tạo, clock phải thấp để tương thích với thẻ. Dự án dùng prescaler lớn. Sau khi thẻ khởi tạo thành công, một thiết kế hoàn thiện có thể tăng clock để đọc/ghi nhanh hơn.

### 4.6. SPI so với UART

| SPI | UART |
| --- | --- |
| Đồng bộ, có clock | Không đồng bộ, không có dây clock |
| Thường dùng trong cùng bo mạch | Thường dùng kết nối thiết bị/PC |
| Full-duplex | Full-duplex với TX/RX riêng |
| Cần CS cho từng slave | Không cần CS |
| Tốc độ thường cao hơn | Cấu hình bằng baud rate |

---

## 5. Thẻ SD ở chế độ SPI

### 5.1. Command packet

Một lệnh SD ở SPI mode gồm 6 byte:

```text
[Command][Argument 4 byte][CRC]
```

Hàm `SD_SendCommand()` trong `sd_spi.c` đóng gói và gửi packet, sau đó chờ byte phản hồi R1.

### 5.2. Quy trình khởi tạo

`SD_Init()` thực hiện:

1. Đưa CS lên HIGH và phát ít nhất 74 xung clock.
2. **CMD0**: đưa thẻ vào Idle State.
3. **CMD8**: kiểm tra thẻ SD version 2 và mức điện áp.
4. **CMD55 + ACMD41**: yêu cầu thẻ hoàn tất khởi tạo.
5. **CMD58**: đọc OCR và bit CCS để phân biệt SDSC/SDHC.
6. Đặt `sd_initialized = true`.

Các phản hồi quan trọng:

- `R1 = 0x01`: thẻ đang ở Idle State.
- `R1 = 0x00`: lệnh thành công, thẻ sẵn sàng.

### 5.3. Một số command cần nhớ

| Command | Chức năng |
| --- | --- |
| CMD0 | Reset/đưa thẻ vào idle |
| CMD8 | Kiểm tra SD v2 và điện áp |
| CMD9 | Đọc CSD để tính dung lượng |
| CMD17 | Đọc một block |
| CMD24 | Ghi một block |
| CMD55 | Báo command kế tiếp là application command |
| ACMD41 | Bắt đầu/tiếp tục quá trình khởi tạo |
| CMD58 | Đọc thanh ghi OCR |

### 5.4. Sector và block

Trong dự án, một sector logic có kích thước **512 byte**.

`SD_ReadBlocks(sector, buffer, count)`:

1. Gửi CMD17 cho từng sector.
2. Chờ data token `0xFE`.
3. Đọc 512 byte.
4. Đọc bỏ 2 byte CRC.

`SD_WriteBlocks(...)`:

1. Gửi CMD24.
2. Gửi data token `0xFE`.
3. Gửi 512 byte.
4. Kiểm tra data response.
5. Chờ thẻ hết busy.

### 5.5. SDSC và SDHC

- **SDSC**: command address thường tính theo byte.
- **SDHC/SDXC**: command address tính theo số sector 512 byte.

Driver hiện truyền trực tiếp `sector + block` vào CMD17/CMD24, nên đường đọc/ghi thực tế phù hợp với SDHC. Nếu hỗ trợ SDSC đầy đủ, địa chỉ cần nhân 512.

### 5.6. Timeout

Không được chờ thẻ vô hạn. Driver dùng `HAL_GetTick()` để giới hạn thời gian chờ:

- Chờ thẻ thoát idle.
- Chờ data token.
- Chờ hoàn tất ghi.

Timeout giúp hệ thống không bị treo vĩnh viễn khi thẻ lỗi hoặc mất kết nối.

---

## 6. FAT và FatFs

### 6.1. FAT là gì?

**FAT** là viết tắt của **File Allocation Table** — bảng cấp phát file.

Đây là một định dạng hệ thống file. Nó tổ chức vùng lưu trữ thành các cluster và dùng một bảng để ghi cluster tiếp theo của mỗi file.

Các biến thể:

- FAT12.
- FAT16.
- FAT32.

Tên gọi phụ thuộc chủ yếu vào số bit dùng cho mỗi entry trong bảng FAT.

### 6.2. Cấu trúc khái quát của phân vùng FAT

```text
Boot sector / Reserved area
        ↓
FAT table
        ↓
Directory entries
        ↓
Data area chứa nội dung file
```

Một file có thể chiếm nhiều cluster không liên tiếp. Bảng FAT tạo thành một chuỗi để liên kết các cluster của file.

### 6.3. Sector và cluster khác nhau thế nào?

- **Sector**: đơn vị đọc/ghi ở tầng block device, dự án dùng 512 byte.
- **Cluster**: đơn vị cấp phát của hệ thống file, gồm một hoặc nhiều sector.

FatFs biến thao tác file thành thao tác sector; driver SD chỉ cần biết sector, không cần hiểu tên file hay thư mục.

### 6.4. FatFs là gì?

**FatFs** là thư viện C hiện thực hệ thống file FAT cho vi điều khiển.

FAT là **định dạng hệ thống file**; FatFs là **thư viện phần mềm** giúp đọc/ghi định dạng đó.

Một số FatFs API trong dự án:

| Hàm | Chức năng |
| --- | --- |
| `f_mount()` | Gắn volume vào FatFs |
| `f_open()` | Mở hoặc tạo file |
| `f_read()` | Đọc dữ liệu file |
| `f_write()` | Ghi dữ liệu file |
| `f_close()` | Đóng file |
| `f_unlink()` | Xóa file |
| `f_getfree()` | Tính dung lượng trống |
| `f_opendir()` | Mở thư mục |
| `f_readdir()` | Đọc từng entry thư mục |

### 6.5. Mount là gì?

**Mount** là quá trình liên kết một hệ thống file trên thiết bị lưu trữ với thư viện để ứng dụng có thể truy cập.

Khi mount, FatFs đọc metadata cần thiết và xác định volume có hợp lệ hay không.

`Storage_Mount()` gọi `f_mount(...)`. Mount không có nghĩa là copy toàn bộ dữ liệu vào RAM.

### 6.6. File object

FatFs dùng cấu trúc `FIL` để giữ trạng thái một file đang mở:

- Vị trí con trỏ hiện tại.
- Thông tin cluster.
- Chế độ mở.
- Buffer và trạng thái nội bộ.

Quy trình đúng:

```text
f_open → f_read hoặc f_write → f_close
```

Luôn đóng file để đồng bộ metadata và giải phóng file object.

### 6.7. Chế độ mở file

Ví dụ các flag thường dùng:

- `FA_READ`: mở để đọc.
- `FA_WRITE`: mở để ghi.
- `FA_CREATE_ALWAYS`: tạo mới hoặc ghi đè file cũ.
- `FA_OPEN_APPEND`: mở và ghi vào cuối file.

### 6.8. FRESULT

Các hàm FatFs trả về `FRESULT`, ví dụ:

- `FR_OK`: thành công.
- `FR_NO_FILE`: không tìm thấy file.
- `FR_NOT_READY`: thiết bị chưa sẵn sàng.
- `FR_DISK_ERR`: lỗi tầng lưu trữ.
- `FR_INVALID_NAME`: tên/path không hợp lệ.

Không nên chỉ kiểm tra dữ liệu đầu ra; cần kiểm tra cả mã lỗi.

---

## 7. Disk I/O glue layer

FatFs độc lập với phần cứng. Để dùng với một thiết bị cụ thể, dự án phải cung cấp các callback Disk I/O.

File `FATFS/Target/user_diskio.c` là lớp nối:

```text
FatFs USER_read()
    → SD_ReadBlocks()
        → SPI
```

Các hàm chính:

- `USER_initialize()`: bảo đảm thẻ đã được khởi tạo.
- `USER_status()`: trả trạng thái sẵn sàng.
- `USER_read()`: chuyển yêu cầu đọc sector xuống driver.
- `USER_write()`: chuyển yêu cầu ghi sector xuống driver.
- `USER_ioctl()`: xử lý lệnh điều khiển như đồng bộ.

Đây là ví dụ rõ về **abstraction**: FatFs không biết thiết bị bên dưới là SD qua SPI, SDIO, USB hay flash.

---

## 8. UART

### 8.1. UART là gì?

**UART** là viết tắt của **Universal Asynchronous Receiver/Transmitter**.

UART truyền nối tiếp không đồng bộ:

- Không có dây clock chung.
- Hai bên phải thống nhất baud rate và format frame.
- Có TX để truyền và RX để nhận.

Dự án dùng USART1:

- TX: PA9.
- RX: PA10.
- `115200 8N1`.

### 8.2. 115200 8N1 nghĩa là gì?

- `115200`: 115200 symbol/bit mỗi giây.
- `8`: 8 data bit.
- `N`: không có parity bit.
- `1`: 1 stop bit.

Một frame UART thường gồm:

```text
1 start bit + 8 data bit + 1 stop bit
```

Vì một byte cần khoảng 10 bit trên đường truyền, tốc độ dữ liệu hữu ích tối đa lý thuyết xấp xỉ 11.52 kB/s.

### 8.3. Giao thức lệnh của dự án

UART chỉ định nghĩa cách truyền byte. Dự án xây thêm một **application protocol** dạng text:

```text
PING
LIST
INFO
READ demo.txt
WRITE demo.txt Hello
APPEND demo.txt World
DELETE demo.txt
```

Mỗi dòng kết thúc bằng newline. Board parse chuỗi và trả response như `OK`, `ERROR`, dữ liệu và `END`.

### 8.4. UART không phải USB Mass Storage

PC không nhìn thấy thẻ như một ổ đĩa. PC chỉ gửi các lệnh text qua cổng COM ảo của ST-Link.

Python CLI trong `tools/uart_pc_cli.py` là client cho protocol này.

---

## 9. Interrupt và ISR

### 9.1. Interrupt là gì?

**Interrupt** (ngắt) cho phép phần cứng báo cho CPU khi có sự kiện. CPU tạm dừng luồng đang chạy, thực thi **ISR** rồi quay lại công việc trước đó.

Trong dự án, khi UART nhận byte:

```text
USART1 nhận byte
 → USART1_IRQHandler()
 → UartPc_OnRxIrq()
 → đưa byte vào ring buffer
```

### 9.2. Vì sao không xử lý toàn bộ command trong ISR?

ISR nên ngắn vì:

- ISR dài làm tăng độ trễ của các ngắt khác.
- Không nên gọi thao tác chậm như FatFs hay ghi SD trong ISR.
- Nhiều API RTOS không an toàn khi gọi trong ngắt.

Vì vậy ISR chỉ nhận byte và lưu nhanh vào buffer. Task xử lý command sau.

### 9.3. Polling và interrupt

- **Polling**: CPU liên tục hỏi “có dữ liệu chưa?”.
- **Interrupt**: ngoại vi chủ động báo khi dữ liệu tới.

Ngắt tiết kiệm thời gian CPU hơn khi sự kiện không xảy ra liên tục.

---

## 10. Ring buffer

### 10.1. Khái niệm

**Ring buffer** hay circular buffer là vùng đệm vòng có kích thước cố định.

Nó dùng hai chỉ số:

- `head`: vị trí ghi tiếp theo.
- `tail`: vị trí đọc tiếp theo.

Khi đến cuối mảng, chỉ số quay lại đầu bằng phép modulo.

### 10.2. Ring buffer trong dự án

`uart_pc.c` khai báo buffer RX 1024 byte:

```c
static volatile uint8_t  s_rx_ring[UART_PC_RX_RING];
static volatile uint16_t s_rx_head;
static volatile uint16_t s_rx_tail;
```

ISR là **producer**, UART task là **consumer**:

```text
ISR ghi byte tại head → tăng head
Task đọc byte tại tail → tăng tail
```

Nếu vị trí tiếp theo của `head` bằng `tail`, buffer đầy và byte mới bị bỏ qua.

### 10.3. Tại sao biến dùng volatile?

`volatile` báo cho compiler rằng giá trị có thể thay đổi ngoài luồng thực thi hiện tại, ví dụ do ISR thay đổi.

Nó ngăn compiler cache hoặc tối ưu bỏ các lần đọc cần thiết. Tuy nhiên, `volatile` không tự tạo atomicity hay thay thế mutex.

---

## 11. FreeRTOS, task và scheduler

### 11.1. RTOS là gì?

**RTOS** là Real-Time Operating System — hệ điều hành thời gian thực.

RTOS quản lý:

- Task.
- Lịch chạy.
- Priority.
- Delay.
- Mutex, semaphore, queue.

“Thời gian thực” không có nghĩa là luôn chạy nhanh nhất; nó nhấn mạnh khả năng đáp ứng trong thời hạn dự đoán được.

### 11.2. Task trong dự án

Dự án có hai luồng chính:

- `GUI_Task`: chạy TouchGFX, priority Normal.
- `defaultTask`: sau khoảng 2 giây chạy `UartPc_Task`, priority Low.

Scheduler quyết định task nào được sử dụng CPU dựa trên trạng thái và priority.

### 11.3. Delay

`osDelay()` đưa task hiện tại vào trạng thái chờ, cho task khác chạy. Nó khác với busy-wait vì không chiếm CPU trong toàn bộ thời gian chờ.

---

## 12. Race condition và mutex

### 12.1. Race condition

**Race condition** xảy ra khi nhiều task cùng truy cập tài nguyên chia sẻ và kết quả phụ thuộc vào thứ tự chạy.

Trong dự án, GUI và UART đều có thể gọi FatFs. Nếu cả hai truy cập thẻ cùng lúc:

- Trạng thái FatFs có thể bị xung đột.
- SPI transaction có thể xen kẽ.
- Dữ liệu hoặc metadata có thể hỏng.

### 12.2. Mutex

**Mutex** là khóa loại trừ tương hỗ. Chỉ một task được giữ mutex tại một thời điểm.

Luồng đúng:

```text
Task xin mutex
 → nếu thành công: truy cập FatFs
 → hoàn tất
 → nhả mutex
```

Các hàm dự án dùng:

```c
Storage_TryLock(timeout_ms);
Storage_Unlock();
```

### 12.3. Mutex khác semaphore

- **Mutex**: bảo vệ tài nguyên dùng chung, có khái niệm owner.
- **Semaphore**: báo hiệu sự kiện hoặc đếm số tài nguyên; không nhất thiết có owner.

Với một bus/thư viện chỉ được truy cập bởi một task tại một thời điểm, mutex là lựa chọn phù hợp.

### 12.4. Deadlock

**Deadlock** là tình trạng các task chờ khóa lẫn nhau và không task nào tiếp tục được.

Cách giảm nguy cơ:

- Luôn nhả mutex trên mọi đường return.
- Dùng timeout.
- Giữ lock trong thời gian ngắn.
- Dùng thứ tự lock nhất quán nếu có nhiều mutex.

---

## 13. TouchGFX và mô hình MVP

TouchGFX là framework GUI cho vi điều khiển STM32.

Dự án dùng cách tổ chức gần với **MVP — Model, View, Presenter**:

- **Model**: lấy dữ liệu hệ thống, ví dụ `Storage_ReadInfo()`.
- **Presenter**: truyền dữ liệu giữa Model và View.
- **View**: cập nhật text/widget trên LCD.

Luồng:

```text
Model::tick()
 → lấy StorageInfo
 → Presenter nhận thay đổi
 → Screen1View cập nhật total/used/free và số file
```

`Model::tick()` không nên đọc SD ở mọi frame. Dự án giới hạn tần suất, khoảng mỗi 60 tick, để giảm tải.

### Framebuffer

**Framebuffer** là vùng RAM chứa màu của từng pixel màn hình. LTDC đọc framebuffer và phát tín hiệu tới LCD. Dự án dùng RGB565:

- 5 bit đỏ.
- 6 bit xanh lá.
- 5 bit xanh dương.
- Tổng 16 bit/pixel.

Với màn hình 240 × 320:

```text
240 × 320 × 2 byte = 153600 byte cho một framebuffer
```

DMA2D hỗ trợ các thao tác đồ họa để giảm tải CPU.

---

## 14. Storage Manager và CRUD

`storage_manager.c` là tầng ứng dụng quản lý lưu trữ. Nó che giấu chi tiết FatFs khỏi UART và GUI.

**CRUD** là bốn nhóm thao tác dữ liệu:

- Create: tạo file.
- Read: đọc file.
- Update: ghi đè hoặc append.
- Delete: xóa file.

Ánh xạ trong dự án:

| CRUD | Hàm |
| --- | --- |
| Create/Update | `Storage_WriteFile()` |
| Read | `Storage_ReadFile()` |
| Update | `Storage_AppendFile()` |
| Delete | `Storage_DeleteFile()` |

`Storage_ReadInfo()` lấy:

- Tổng dung lượng.
- Dung lượng đã dùng.
- Dung lượng còn trống.
- Số file trong thư mục gốc.

---

## 15. Các loại bộ nhớ liên quan

### 15.1. Flash

Flash trong STM32 lưu firmware. Nội dung vẫn còn khi mất điện.

### 15.2. SRAM/SDRAM

RAM lưu:

- Stack.
- Heap.
- Biến runtime.
- Buffer UART/SD.
- Framebuffer GUI.

Kit dùng SDRAM ngoài cho dữ liệu đồ họa lớn.

### 15.3. Stack và heap

- **Stack**: biến cục bộ, tham số hàm, địa chỉ trả về; mỗi task RTOS có stack riêng.
- **Heap**: cấp phát động, FreeRTOS dùng heap để tạo task/mutex và các object.

Nếu stack task quá nhỏ có thể gây stack overflow. Nếu heap không đủ, việc tạo task hoặc mutex có thể thất bại.

### 15.4. Buffer

Buffer là vùng nhớ tạm để cân bằng tốc độ giữa các thành phần:

- UART RX ring buffer: 1024 byte.
- File buffer UART: 512 byte.
- Mỗi sector SD: 512 byte.
- Framebuffer LCD: lưu pixel.

---

## 16. Luồng đọc file hoàn chỉnh

Ví dụ PC gửi:

```text
READ demo.txt
```

Trình tự:

1. USART1 nhận từng byte.
2. ISR gọi `UartPc_OnRxIrq()` và đẩy byte vào ring buffer.
3. `UartPc_Task()` lấy byte, ghép thành một dòng.
4. Parser nhận diện command `READ` và path `demo.txt`.
5. Task lấy storage mutex.
6. Gọi `Storage_ReadFile()`.
7. `Storage_ReadFile()` gọi `f_open()`, `f_read()`, `f_close()`.
8. FatFs xác định cluster/sector của file.
9. FatFs gọi `USER_read()`.
10. `USER_read()` gọi `SD_ReadBlocks()`.
11. Driver gửi CMD17 qua SPI và nhận sector 512 byte.
12. Dữ liệu quay ngược lên buffer UART.
13. Board truyền response về PC.
14. Task nhả mutex.

Điểm cần nhớ: lệnh mức file ở trên được chuyển dần thành thao tác mức sector ở dưới.

---

## 17. Luồng ghi file hoàn chỉnh

Ví dụ:

```text
WRITE demo.txt Hello
```

Trình tự:

1. UART nhận và parse command.
2. Task lấy storage mutex.
3. `Storage_WriteFile()` mở file với chế độ tạo/ghi đè.
4. FatFs cập nhật directory entry và bảng FAT khi cần.
5. FatFs gọi `USER_write()` cho các sector liên quan.
6. `USER_write()` gọi `SD_WriteBlocks()`.
7. Driver gửi CMD24, token và 512 byte dữ liệu.
8. Driver chờ thẻ hoàn tất ghi.
9. FatFs đóng file và đồng bộ metadata.
10. Board trả số byte đã ghi và nhả mutex.

Ngay cả khi ứng dụng chỉ ghi vài byte, tầng hệ thống file có thể phải đọc/ghi cả sector và cập nhật metadata.

---

## 18. Khởi động hệ thống

Luồng khởi động trong `Core/Src/main.c`:

1. `HAL_Init()`.
2. Cấu hình system clock.
3. Khởi tạo GPIO và các peripheral.
4. Khởi tạo SDRAM, LTDC, DMA2D và TouchGFX.
5. Khởi tạo SPI1, USART1 và FatFs.
6. Gọi `SD_Init()`.
7. Gọi `Storage_Mount()`.
8. Khởi tạo kernel FreeRTOS.
9. Tạo `defaultTask` và `GUI_Task`.
10. Chạy scheduler.

Nếu scheduler đã chạy bình thường, `main()` không quay lại vòng xử lý ứng dụng truyền thống; công việc được thực hiện trong các task.

---

## 19. Những giới hạn kỹ thuật cần biết

1. Không có chân card-detect và không có luồng tự remount khi rút/cắm thẻ.
2. Nên cắm thẻ trước khi cấp nguồn hoặc reset.
3. Driver nhận diện SDSC nhưng địa chỉ đọc/ghi hiện dùng cách của SDHC.
4. `SD_Init()` yêu cầu CMD8 thành công nên không hỗ trợ đầy đủ thẻ SD v1 cũ.
5. Lệnh UART `READ` chỉ trả lượng dữ liệu vừa buffer khoảng 511 byte.
6. Danh sách thư mục gốc bị giới hạn số file.
7. Long File Name đang tắt trong cấu hình FatFs.
8. Giao tiếp PC là UART protocol, không phải USB Mass Storage.
9. Driver đọc/ghi nhiều sector bằng cách lặp CMD17/CMD24, chưa dùng multi-block command tối ưu hơn.
10. Chưa có bộ automated test độc lập; chủ yếu là test thủ công và hàm chẩn đoán.

---

## 20. Câu hỏi vấn đáp mẫu

### API trong dự án là gì?

Là các hàm công khai giữa các tầng. Ví dụ UART gọi `Storage_ReadFile()` thay vì gọi trực tiếp SPI. API giúp giảm phụ thuộc và che giấu implementation.

### FAT khác FatFs như thế nào?

FAT là định dạng hệ thống file; FatFs là thư viện C hiện thực việc đọc/ghi FAT trên hệ nhúng.

### Vì sao cần `user_diskio.c`?

FatFs không biết phần cứng cụ thể. `user_diskio.c` chuyển yêu cầu sector của FatFs thành lời gọi driver SD.

### Vì sao đọc một file lại cần CMD17?

FatFs ánh xạ vị trí file thành sector. Tầng driver dùng CMD17 để đọc sector đó từ thẻ SD qua SPI.

### Vì sao UART cần ring buffer?

Byte có thể đến khi task đang bận. ISR lưu nhanh byte vào ring buffer; task xử lý sau, tránh mất dữ liệu và tránh làm ISR quá dài.

### Vì sao GUI và UART cần mutex?

Cả hai dùng chung FatFs và SPI SD. Mutex ngăn transaction bị xen kẽ và tránh hỏng trạng thái/dữ liệu.

### SPI có gì khác UART?

SPI đồng bộ, có clock và CS, thường dùng trên bo mạch. UART không đồng bộ, dùng TX/RX và baud rate, phù hợp giao tiếp với PC.

### Mount là gì?

Là gắn volume FAT trên thẻ vào FatFs để ứng dụng có thể thao tác file. Mount không copy toàn bộ dữ liệu vào RAM.

### Sector và cluster khác nhau thế nào?

Sector là đơn vị block device; cluster là đơn vị cấp phát của FAT và gồm một hoặc nhiều sector.

### FreeRTOS giải quyết vấn đề gì?

Nó cho GUI và UART chạy như các task riêng, cung cấp scheduler, delay và mutex để điều phối tài nguyên.

### Tại sao không ghi SD trực tiếp từ ISR?

Ghi SD chậm và có thể block. ISR phải ngắn; nó chỉ nhận byte, còn task thực hiện parse và truy cập file.

### Dự án có phải đầu đọc thẻ USB không?

Không. PC điều khiển file bằng protocol lệnh text qua UART/ST-Link VCP.

---

## 21. Danh sách file nên đọc

Theo thứ tự từ trên xuống dưới:

1. `README.md` — mục tiêu, kiến trúc và cách demo.
2. `Core/Src/main.c` — khởi động và tạo task.
3. `STM32CubeIDE/Application/User/uart_pc.c` — UART, interrupt, ring buffer và command.
4. `STM32CubeIDE/Application/User/storage_manager.c` — API quản lý file.
5. `FATFS/Target/user_diskio.c` — cầu nối FatFs với SD driver.
6. `STM32CubeIDE/Application/User/sd_spi.c` — command SD và truyền sector.
7. `TouchGFX/gui/src/model/Model.cpp` — lấy thông tin thẻ.
8. `TouchGFX/gui/src/screen1_screen/Screen1Presenter.cpp` — truyền dữ liệu UI.
9. `TouchGFX/gui/src/screen1_screen/Screen1View.cpp` — hiển thị lên LCD.

Khi đọc mỗi hàm, hãy tự trả lời bốn câu:

1. Ai gọi hàm này?
2. Input là gì?
3. Hàm gọi tầng nào bên dưới?
4. Output hoặc lỗi được trả về như thế nào?

