#include "rt_Vterm.h"
#include "shell.h"
#include "string.h"
#include <ctype.h>
#include <rtdevice.h>
#include <rtthread.h>
#include <stdio.h>

/**< Maximum number of registered Vterm commands */
#define Vterm_CMD_MAX 8
/**< Size of input buffer for Vterm command processing */
#define Vterm_INPUT_BUF_SIZE 128

/**< Table storing registered Vterm commands and their callbacks */
static vterm_cmd_entry_t vterm_cmd_table[Vterm_CMD_MAX];
/**< Current count of registered Vterm commands */
static int vterm_cmd_count = 0;

/**
 * @struct vterm_device
 * @brief  Virtual terminal device structure, extending RT-Thread device
 */
struct vterm_device
{
    struct rt_device parent;           /**< Inherited RT-Thread device base */
    rt_device_t      original_console; /**< Original console device (for restoration) */
    rt_device_t      original_finsh;   /**< Original FinSH device (for restoration) */
};

/**< Global instance of virtual terminal device */
static struct vterm_device vterm_dev;
extern struct finsh_shell *shell; /**< External reference to FinSH shell instance */

/****************************************/
/**
 * @brief        Open virtual terminal device
 *
 * @param[in]    dev     Pointer to RT-Thread device structure
 * @param[in]    oflag   Open flags (unused in this implementation)
 *
 * @retval       rt_err_t  RT_EOK always (success)
 *
 * @note         No special initialization needed for open operation
 */
static rt_err_t vterm_open(rt_device_t dev, rt_uint16_t oflag)
{
    return RT_EOK; // Vterm open requires no specific setup
}

/**
 * @brief        Write data to virtual terminal
 *
 * @param[in]    dev     Pointer to RT-Thread device structure
 * @param[in]    pos     Offset (unused for character devices)
 * @param[in]    buffer  Pointer to data buffer to write
 * @param[in]    size    Number of bytes to write
 *
 * @retval       rt_size_t  Number of bytes written (always equals input size)
 *
 * @note         1. Converts '\n' to "\r\n" for proper line endings
 *               2. Uses RT_Vterm_PutChar for actual data transmission
 */
static rt_size_t vterm_write(rt_device_t dev, rt_off_t pos,
                              const void *buffer, rt_size_t size)
{
    const char *ptr = (const char *)buffer;
    rt_size_t   i;

    // Write each character, converting newline to CRLF
    for (i = 0; i < size; i++)
    {
        if (ptr[i] == '\n')
        {
            RT_Vterm_PutChar('\r'); // Prepend carriage return before newline
        }
        RT_Vterm_PutChar(ptr[i]);
    }
    return size; // All bytes are written (logical success)
}

/**
 * @brief        Read data from virtual terminal
 *
 * @param[in]    dev     Pointer to RT-Thread device structure
 * @param[in]    pos     Offset (unused for character devices)
 * @param[out]   buffer  Pointer to buffer to store read data
 * @param[in]    size    Maximum number of bytes to read
 *
 * @retval       rt_ssize_t  Number of bytes actually read
 *
 * @note         Wraps RT_Vterm_Read for data retrieval from downstream buffer
 */
static rt_size_t vterm_read(rt_device_t dev, rt_off_t pos,
                             void *buffer, rt_size_t size)
{
    rt_size_t ret = (rt_size_t)RT_Vterm_Read(buffer, size);
    return ret;
}

/****************************************/
/**
 * @brief        Indicate received data to FinSH shell
 *
 * @param[in]    dev     Pointer to RT-Thread device structure (unused)
 * @param[in]    size    Size of received data (unused)
 *
 * @retval       rt_err_t  RT_EOK if shell semaphore is released successfully
 *
 * @note         Triggers FinSH processing by releasing its receive semaphore
 */
static rt_err_t finsh_rx_ind(rt_device_t dev, rt_size_t size)
{
    if (shell)
        rt_sem_release(&shell->rx_sem); // Notify shell of available data
    return RT_EOK;
}

/****************************************/
/**
 * @brief        Switch all I/O (console and FinSH) to virtual terminal
 *
 * @note         1. Saves original console and FinSH devices for later restoration
 *               2. Redirects console and FinSH output to "vterm_buffer" device
 *               3. Prints confirmation message via original console
 */
void vterm_console(void)
{
    /* Save original I/O devices */
    vterm_dev.original_console = rt_console_get_device();
    vterm_dev.original_finsh   = rt_device_find(finsh_get_device());

    /* Redirect console and FinSH to Vterm */
    rt_console_set_device("vterm");
    finsh_set_device("vterm");

    rt_kprintf("[OK] All I/O now via Vterm\n"); // Confirmation message
}
MSH_CMD_EXPORT(vterm_console, Switch ALL I / O to Vterm);

/****************************************/
/**
 * @brief        Restore original I/O devices (console and FinSH)
 *
 * @note         1. Restores console and FinSH to their original devices
 *               2. Does nothing if original devices are not saved
 *               3. Prints confirmation message via restored console
 */
void restore_original(void)
{
    if (vterm_dev.original_console)
    {
        rt_console_set_device(vterm_dev.original_console->parent.name);
    }
    if (vterm_dev.original_finsh)
    {
        finsh_set_device(vterm_dev.original_finsh->parent.name);
    }
    rt_kprintf("[OK] Restored original I/O devices\n"); // Confirmation message
}
MSH_CMD_EXPORT(restore_original, Restore original I / O devices);

/****************************************/
/**
 * @brief        Thread entry for polling Vterm data and notifying FinSH
 *
 * @param[in]    p  Unused (thread parameter placeholder)
 *
 * @note         1. Runs indefinitely, checking for downstream data every 10ms
 *               2. Triggers FinSH data processing via finsh_rx_ind when data is available
 */
static void vterm_check(void)
{
    if (RT_Vterm_HasData())                 // Check if downstream buffer has data
        finsh_rx_ind(&vterm_dev.parent, 1); // Notify FinSH of new data
}

/****************************************/
/**
 * @brief        Initialize Vterm buffer device and related components
 *
 * @retval       int  RT_EOK on success, error code otherwise
 *
 * @note         1. Registers "vterm_buffer" as a character device
 *               2. Sets up device operations (open, read, write)
 *               3. Creates and starts a polling thread for data detection
 */
int vterm_device_init(void)
{
    // Initialize Vterm device structure
    vterm_dev.parent.type  = RT_Device_Class_Char;
    vterm_dev.parent.open  = vterm_open;
    vterm_dev.parent.read  = vterm_read;
    vterm_dev.parent.write = vterm_write;

    // Register Vterm device with RT-Thread device manager
    rt_device_register(&vterm_dev.parent, "vterm", RT_DEVICE_FLAG_RDWR);

    rt_thread_idle_sethook(vterm_check);

    return RT_EOK;
}
MSH_CMD_EXPORT(vterm_device_init, Initialize Vterm buffer device);

/****************************************/
/**
 * @brief        Initialize Vterm buffer device and related components
 *
 * @retval       int  RT_EOK on success, error code otherwise
 *
 * @note         1. Registers "vterm_buffer" as a character device
 *               2. Sets up device operations (open, read, write)
 *               3. Creates and starts a polling thread for data detection
 */
int rt_hw_vterm_console_init(void)
{
    vterm_device_init();
    vterm_console();
    return RT_EOK;
}
MSH_CMD_EXPORT(rt_hw_vterm_console_init, Initialize Vterm buffer device);


/**
 * @brief        Register a new command with its callback to Vterm （without connect to finsh）
 *
 * @param[in]    cmd   Command string (max 31 characters)
 * @param[in]    cb    Callback function to execute when command is received
 *
 * @retval       int   0 on success, -1 if command table is full
 *
 * @note         1. Stores command in vterm_cmd_table
 *               2. Truncates command if longer than 31 characters
 *               3. Limits total commands to Vterm_CMD_MAX
 */
int vterm_cmd_register(const char *cmd, vterm_cmd_callback_t cb)
{
    if (vterm_cmd_count >= Vterm_CMD_MAX)
        return -1; // Command table is full

    // Copy command string with truncation protection
    strncpy(vterm_cmd_table[vterm_cmd_count].cmd, cmd, sizeof(vterm_cmd_table[0].cmd) - 1);
    vterm_cmd_table[vterm_cmd_count].cmd[sizeof(vterm_cmd_table[0].cmd) - 1] = '\0';
    vterm_cmd_table[vterm_cmd_count].callback                                = cb;
    vterm_cmd_count++;

    return 0;
}

/**
 * @brief        Thread entry for Vterm input assistance and command processing
 *
 * @param[in]    parameter  Unused (thread parameter placeholder)
 *
 * @note         1. Reads input characters from Vterm downstream buffer
 *               2. Handles line endings (CR/LF) and echo input characters
 *               3. Matches completed input (after CR/LF) with registered commands
 *               4. Executes corresponding callback if command match is found
 *               5. Runs indefinitely with 10ms delay between checks
 */
static void vterm_input_assist_thread(void *parameter)
{
    static char input_buf[Vterm_INPUT_BUF_SIZE]; // Buffer to accumulate input
    int         input_len = 0;                   // Current length of input
    char        ch;                              // Latest received character

    while (1)
    {
        if (RT_Vterm_HasData()) // Check for available input
        {
            ch = (char)RT_Vterm_GetKey(); // Read one character

            // Handle line termination (enter key)
            if (ch == '\r' || ch == '\n')
            {
                RT_Vterm_PutChar('\r'); // Echo CR for proper line ending
                RT_Vterm_PutChar('\n'); // Echo LF

                if (input_len > 0)
                {
                    input_buf[input_len] = '\0'; // Null-terminate input string

                    // Check input against registered commands
                    for (int i = 0; i < vterm_cmd_count; i++)
                    {
                        if (strcmp(input_buf, vterm_cmd_table[i].cmd) == 0)
                        {
                            if (vterm_cmd_table[i].callback)
                                vterm_cmd_table[i].callback(input_buf); // Execute callback
                        }
                    }
                    input_len = 0; // Reset input buffer for next command
                }
            }
            // Handle printable characters (with buffer limit)
            else if (isprint((unsigned char)ch) && input_len < Vterm_INPUT_BUF_SIZE - 1)
            {
                input_buf[input_len++] = ch; // Add to input buffer
                RT_Vterm_PutChar(ch);        // Echo character back
            }
        }
        rt_thread_mdelay(10); // Short delay to reduce CPU usage
    }
}

/**
 * @brief        Start Vterm input assistance thread
 *
 * @note         1. Creates "vterm_inassist" thread if not already created
 *               2. Thread handles input processing and command execution
 *               3. Thread configuration: stack=512, priority=20, timeslice=10
 */
void start_vterm_input_assist(void)
{
    static rt_thread_t tid = RT_NULL; // Static to prevent multiple thread creation
    if (tid == RT_NULL)
    {
        tid = rt_thread_create("vterm_inassist", vterm_input_assist_thread, RT_NULL, 512, 20, 10);
        if (tid)
            rt_thread_startup(tid);
    }
}
MSH_CMD_EXPORT(start_vterm_input_assist, Start Vterm input assist mode);
