#include "rt_Vterm.h"
#include <rtthread.h>

/**
 * @brief        Demonstrates basic Vterm read/write operations
 *
 * @note         1. Writes a message to the upstream buffer using RT_Vterm_Write
 *               2. Checks if there's data in the downstream buffer using RT_Vterm_HasData
 *               3. Reads data from downstream buffer if available
 *               4. Prints confirmation messages to the system console
 */
void vterm_example_basic(void)
{
    char msg[]   = "Vterm Basic Example!"; // Message to send to upstream
    char buf[32] = {0};                  // Buffer to store downstream data

    // Write message to upstream buffer (exclude null terminator)
    RT_Vterm_Write(msg, sizeof(msg) - 1);
    rt_kprintf("已写入上行: %s\n", msg);

    // Check if downstream buffer has data
    if (RT_Vterm_HasData())
    {
        // Read data from downstream buffer
        RT_Vterm_Read(buf, sizeof(buf));
        rt_kprintf("收到下行: %s\n", buf);
    }
}
MSH_CMD_EXPORT(vterm_example_basic, Vterm basic read / write example);

/**
 * @brief        Demonstrates various formatted output using RT_Vterm_printf
 *
 * @note         1. Shows different data types (integer, hexadecimal, string, float)
 *               2. Uses various format specifiers to illustrate RT_Vterm_printf capabilities
 *               3. Outputs results to Vterm upstream tunnel and confirms completion via system console
 */
void vterm_example_printf(void)
{
    int         num  = 123;
    unsigned    hex  = 0xABCD;
    const char *str  = "hello";
    float       fval = 3.1415f;

    RT_Vterm_printf("==== Vterm printf 示例 ====\n");
    RT_Vterm_printf("整数: %d\n", num);
    RT_Vterm_printf("十六进制: 0x%X\n", hex);
    RT_Vterm_printf("字符串: %s\n", str);
    RT_Vterm_printf("混合: num=%d, hex=0x%X, str=%s\n", num, hex, str);
    RT_Vterm_printf("请在调试器端查看输出\n");

    rt_kprintf("Vterm printf example done.\n");
}
MSH_CMD_EXPORT(vterm_example_printf, Vterm printf example);