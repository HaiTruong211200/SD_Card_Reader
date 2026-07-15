#include "uart_pc.h"
#include "storage_manager.h"
#include "cmsis_os.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#define UART_PC_LINE_MAX   256U
#define UART_PC_FILE_BUF   512U
#define UART_PC_TX_CHUNK   64U
#define UART_PC_RX_RING    1024U

static UART_HandleTypeDef *s_huart = NULL;

static char s_line[UART_PC_LINE_MAX];
static uint8_t s_file_buf[UART_PC_FILE_BUF];

static volatile uint8_t  s_rx_ring[UART_PC_RX_RING];
static volatile uint16_t s_rx_head = 0;
static volatile uint16_t s_rx_tail = 0;

static osMutexId_t s_storage_mutex = NULL;
static const osMutexAttr_t s_storage_mutex_attr = {
    .name = "storageMtx",
};

static int UartPc_LockStorage(uint32_t timeout_ms)
{
    if (s_storage_mutex == NULL)
    {
        return 1;
    }
    return (osMutexAcquire(s_storage_mutex, timeout_ms) == osOK) ? 1 : 0;
}

static void UartPc_UnlockStorage(void)
{
    if (s_storage_mutex != NULL)
    {
        (void)osMutexRelease(s_storage_mutex);
    }
}

int Storage_TryLock(uint32_t timeout_ms)
{
    return UartPc_LockStorage(timeout_ms);
}

void Storage_Unlock(void)
{
    UartPc_UnlockStorage();
}

void UartPc_OnRxIrq(void)
{
    USART_TypeDef *usart;
    uint8_t ch;
    uint16_t next;

    if (s_huart == NULL)
    {
        return;
    }

    usart = s_huart->Instance;

    if (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_ORE) != RESET)
    {
        __HAL_UART_CLEAR_OREFLAG(s_huart);
        (void)usart->DR;
    }
    if (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_FE) != RESET)
    {
        __HAL_UART_CLEAR_FEFLAG(s_huart);
        (void)usart->DR;
    }
    if (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_NE) != RESET)
    {
        __HAL_UART_CLEAR_NEFLAG(s_huart);
        (void)usart->DR;
    }
    if (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_PE) != RESET)
    {
        __HAL_UART_CLEAR_PEFLAG(s_huart);
        (void)usart->DR;
    }

    while (__HAL_UART_GET_FLAG(s_huart, UART_FLAG_RXNE) != RESET)
    {
        ch = (uint8_t)(usart->DR & 0xFFU);
        next = (uint16_t)((s_rx_head + 1U) % UART_PC_RX_RING);
        if (next != s_rx_tail)
        {
            s_rx_ring[s_rx_head] = ch;
            s_rx_head = next;
        }
    }
}

static int UartPc_RxPop(uint8_t *out)
{
    uint16_t tail;

    if (out == NULL)
    {
        return 0;
    }

    if (s_rx_head == s_rx_tail)
    {
        return 0;
    }

    tail = s_rx_tail;
    *out = s_rx_ring[tail];
    s_rx_tail = (uint16_t)((tail + 1U) % UART_PC_RX_RING);
    return 1;
}

static void UartPc_FlushRx(void)
{
    s_rx_tail = s_rx_head;

    if (s_huart != NULL)
    {
        __HAL_UART_CLEAR_OREFLAG(s_huart);
        (void)s_huart->Instance->DR;
    }
}

static void UartPc_WriteRaw(const uint8_t *data, uint16_t len)
{
    uint16_t offset = 0;

    if ((s_huart == NULL) || (data == NULL) || (len == 0U))
    {
        return;
    }

    while (offset < len)
    {
        uint16_t chunk = (uint16_t)(len - offset);
        if (chunk > UART_PC_TX_CHUNK)
        {
            chunk = UART_PC_TX_CHUNK;
        }

        (void)HAL_UART_Transmit(
            s_huart,
            (uint8_t *)&data[offset],
            chunk,
            200
        );
        offset = (uint16_t)(offset + chunk);
        osDelay(1);
    }
}

static void UartPc_Print(const char *text)
{
    if (text == NULL)
    {
        return;
    }
    UartPc_WriteRaw((const uint8_t *)text, (uint16_t)strlen(text));
}

static void UartPc_Println(const char *text)
{
    UartPc_Print(text);
    UartPc_Print("\r\n");
}

static void UartPc_Printf(const char *fmt, ...)
{
    char buf[160];
    va_list args;
    int n;

    va_start(args, fmt);
    n = vsnprintf(buf, sizeof(buf), fmt, args);
    va_end(args);

    if (n > 0)
    {
        if (n >= (int)sizeof(buf))
        {
            n = (int)sizeof(buf) - 1;
        }
        UartPc_WriteRaw((const uint8_t *)buf, (uint16_t)n);
    }
}

static void UartPc_SkipSpaces(char **p)
{
    while ((**p != '\0') && isspace((unsigned char)**p))
    {
        (*p)++;
    }
}

static int UartPc_Token(char **p, char *out, size_t out_size)
{
    size_t i = 0;

    UartPc_SkipSpaces(p);

    if (**p == '\0')
    {
        return 0;
    }

    while ((**p != '\0') && !isspace((unsigned char)**p))
    {
        if (i + 1U < out_size)
        {
            out[i++] = **p;
        }
        (*p)++;
    }

    out[i] = '\0';
    return 1;
}

static void UartPc_CmdHelp(void)
{
    UartPc_Println("OK Work4 UART+FATFS protocol");
    UartPc_Println("  HELP PING LIST INFO");
    UartPc_Println("  READ <file>");
    UartPc_Println("  WRITE <file> <text>");
    UartPc_Println("  APPEND <file> <text>");
    UartPc_Println("  DELETE <file>");
    UartPc_Println("END");
}

static void UartPc_CmdPing(void)
{
    UartPc_Println("OK PONG");
}

static void UartPc_CmdList(void)
{
    uint32_t i;

    if (!UartPc_LockStorage(2000))
    {
        UartPc_Println("ERR LIST busy");
        return;
    }

    if (!Storage_ListRoot())
    {
        UartPc_UnlockStorage();
        UartPc_Printf("ERR LIST fr=%d\r\n", (int)storage_fr);
        return;
    }

    UartPc_Printf("OK count=%lu\r\n", (unsigned long)storage_file_count);
    for (i = 0U; i < storage_file_count; i++)
    {
        UartPc_Println(storage_file_names[i]);
    }
    UartPc_UnlockStorage();
    UartPc_Println("END");
}

static void UartPc_CmdInfo(void)
{
    StorageInfo info;

    if (!UartPc_LockStorage(2000))
    {
        UartPc_Println("ERR INFO busy");
        return;
    }

    if (!Storage_ReadInfo(&info) || !info.ready)
    {
        UartPc_UnlockStorage();
        UartPc_Println("ERR INFO sd_not_ready");
        return;
    }

    UartPc_UnlockStorage();
    UartPc_Printf(
        "OK total_kb=%lu used_kb=%lu free_kb=%lu files=%lu\r\n",
        (unsigned long)(info.total_bytes / 1024ULL),
        (unsigned long)(info.used_bytes / 1024ULL),
        (unsigned long)(info.free_bytes / 1024ULL),
        (unsigned long)info.file_count
    );
}

static void UartPc_CmdRead(char *args)
{
    char name[STORAGE_NAME_LENGTH];
    UINT nread = 0;
    UINT offset;

    if (!UartPc_Token(&args, name, sizeof(name)))
    {
        UartPc_Println("ERR READ usage: READ <file>");
        return;
    }

    if (!UartPc_LockStorage(2000))
    {
        UartPc_Println("ERR READ busy");
        return;
    }

    if (!Storage_ReadFile(name, s_file_buf, sizeof(s_file_buf) - 1U, &nread))
    {
        UartPc_UnlockStorage();
        UartPc_Printf("ERR READ fr=%d\r\n", (int)storage_fr);
        return;
    }
    UartPc_UnlockStorage();

    s_file_buf[nread] = 0U;
    UartPc_Printf("OK size=%u\r\n", (unsigned)nread);

    offset = 0U;
    while (offset < nread)
    {
        UINT chunk = nread - offset;
        if (chunk > UART_PC_TX_CHUNK)
        {
            chunk = UART_PC_TX_CHUNK;
        }
        UartPc_WriteRaw(&s_file_buf[offset], (uint16_t)chunk);
        offset = (UINT)(offset + chunk);
    }

    UartPc_Print("\r\n");
    UartPc_Println("END");
}

static void UartPc_CmdWriteOrAppend(int append, char *args)
{
    char name[STORAGE_NAME_LENGTH];
    UINT written = 0;
    bool ok;

    if (!UartPc_Token(&args, name, sizeof(name)))
    {
        UartPc_Println(append
            ? "ERR APPEND usage: APPEND <file> <text>"
            : "ERR WRITE usage: WRITE <file> <text>");
        return;
    }

    UartPc_SkipSpaces(&args);
    if (*args == '\0')
    {
        UartPc_Println("ERR empty_payload");
        UartPc_Println("example: WRITE TEST.TXT hello");
        return;
    }

    if (!UartPc_LockStorage(2000))
    {
        UartPc_Println("ERR WRITE busy");
        return;
    }

    if (append)
    {
        ok = Storage_AppendFile(name, args, (UINT)strlen(args), &written);
    }
    else
    {
        ok = Storage_WriteFile(name, args, (UINT)strlen(args), &written);
    }
    UartPc_UnlockStorage();

    if (!ok)
    {
        UartPc_Printf("ERR WRITE fr=%d\r\n", (int)storage_fr);
        return;
    }

    UartPc_Printf("OK written=%u\r\n", (unsigned)written);
}

static void UartPc_CmdDelete(char *args)
{
    char name[STORAGE_NAME_LENGTH];

    if (!UartPc_Token(&args, name, sizeof(name)))
    {
        UartPc_Println("ERR DELETE usage: DELETE <file>");
        return;
    }

    if (!UartPc_LockStorage(2000))
    {
        UartPc_Println("ERR DELETE busy");
        return;
    }

    if (!Storage_DeleteFile(name))
    {
        UartPc_UnlockStorage();
        UartPc_Printf("ERR DELETE fr=%d\r\n", (int)storage_fr);
        return;
    }

    UartPc_UnlockStorage();
    UartPc_Println("OK");
}

static void UartPc_HandleLine(char *line)
{
    char cmd[16];
    char *p = line;
    size_t cmd_len;

    while ((*p != '\0') && isspace((unsigned char)*p))
    {
        p++;
    }
    if (*p == '\0')
    {
        return;
    }

    if (!UartPc_Token(&p, cmd, sizeof(cmd)))
    {
        return;
    }

    {
        char *c = cmd;
        while (*c != '\0')
        {
            *c = (char)toupper((unsigned char)*c);
            c++;
        }
    }

    cmd_len = strlen(cmd);

    /* Bỏ nhiễu 1 ký tự / mũi tên (W, L, W[A[A, ...) */
    if (cmd_len < 3U)
    {
        return;
    }

    if (strcmp(cmd, "HELP") == 0)
    {
        UartPc_CmdHelp();
    }
    else if (strcmp(cmd, "PING") == 0)
    {
        UartPc_CmdPing();
    }
    else if (strcmp(cmd, "LIST") == 0)
    {
        UartPc_CmdList();
    }
    else if (strcmp(cmd, "INFO") == 0)
    {
        UartPc_CmdInfo();
    }
    else if (strcmp(cmd, "READ") == 0)
    {
        UartPc_CmdRead(p);
    }
    else if (strcmp(cmd, "WRITE") == 0)
    {
        UartPc_CmdWriteOrAppend(0, p);
    }
    else if (strcmp(cmd, "APPEND") == 0)
    {
        UartPc_CmdWriteOrAppend(1, p);
    }
    else if (strcmp(cmd, "DELETE") == 0)
    {
        UartPc_CmdDelete(p);
    }
    else
    {
        UartPc_Printf("ERR unknown_cmd %s\r\n", cmd);
    }
}

/*
 * Đọc 1 dòng từ ring buffer.
 * Lọc ESC (mũi tên PuTTY: ESC [ A).
 */
static int UartPc_ReadLine(char *buf, size_t buflen)
{
    size_t idx = 0;
    uint8_t ch;
    int esc = 0;

    if ((buf == NULL) || (buflen < 2U))
    {
        return -1;
    }

    for (;;)
    {
        if (!UartPc_RxPop(&ch))
        {
            if (idx == 0U)
            {
                return 0;
            }
            osDelay(5);
            continue;
        }

        /* ESC sequence (arrow keys...) */
        if (ch == 0x1BU)
        {
            esc = 1;
            continue;
        }
        if (esc != 0)
        {
            if ((ch >= (uint8_t)'A' && ch <= (uint8_t)'Z') ||
                (ch >= (uint8_t)'a' && ch <= (uint8_t)'z') ||
                (ch == (uint8_t)'~'))
            {
                esc = 0;
            }
            continue;
        }

        if ((ch == (uint8_t)'\r') || (ch == (uint8_t)'\n'))
        {
            if (idx == 0U)
            {
                continue;
            }
            buf[idx] = '\0';
            return 1;
        }

        if ((ch == 0x08U) || (ch == 0x7FU))
        {
            if (idx > 0U)
            {
                idx--;
            }
            continue;
        }

        if (ch < 0x20U)
        {
            continue;
        }

        if (idx + 1U < buflen)
        {
            buf[idx++] = (char)ch;
        }
    }
}

void UartPc_Init(UART_HandleTypeDef *huart)
{
    s_huart = huart;
    s_rx_head = 0;
    s_rx_tail = 0;

    if (s_storage_mutex == NULL)
    {
        s_storage_mutex = osMutexNew(&s_storage_mutex_attr);
    }

    if (s_huart != NULL)
    {
        __HAL_UART_DISABLE_IT(s_huart, UART_IT_TXE);
        __HAL_UART_DISABLE_IT(s_huart, UART_IT_TC);
        /* Priority 12 < LTDC(9): không tranh VSync/TouchGFX */
        HAL_NVIC_SetPriority(USART1_IRQn, 12, 0);
        HAL_NVIC_EnableIRQ(USART1_IRQn);
        __HAL_UART_ENABLE_IT(s_huart, UART_IT_RXNE);
        UartPc_FlushRx();
    }
}

void UartPc_Task(void *argument)
{
    (void)argument;

    UartPc_FlushRx();
    UartPc_Println("");
    UartPc_Println("=== Work4 UART PC ready ===");
    UartPc_Println("PuTTY: Local echo=Force off, Local line editing=Force on");
    UartPc_Println("Type HELP then Enter. Do NOT use arrow keys.");
    UartPc_Print("> ");

    for (;;)
    {
        int got = UartPc_ReadLine(s_line, sizeof(s_line));

        if (got > 0)
        {
            UartPc_HandleLine(s_line);
            UartPc_FlushRx();
            UartPc_Print("> ");
        }
        else
        {
            osDelay(20);
        }
    }
}
