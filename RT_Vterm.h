#ifndef RT_Vterm_H
#define RT_Vterm_H

#include "RT_tunnel.h"
#include <stdarg.h>
#include <stdlib.h>

/**< tunnel handle for upstream data transmission */
extern RT_tunnel_t up_tunnel;
/**< tunnel handle for downstream data transmission */
extern RT_tunnel_t down_tunnel;

/**
 * @brief        Write data to upstream buffer
 * @param[in]    pBuffer     Pointer to data buffer
 * @param[in]    NumBytes    Number of bytes to write
 * @retval       uint32_t    Actual bytes written
 */
uint32_t RT_Vterm_Write(const void *pBuffer, uint32_t NumBytes);

/**
 * @brief        Write data to downstream buffer
 * @param[in]    pBuffer     Pointer to data buffer
 * @param[in]    NumBytes    Number of bytes to write
 * @retval       uint32_t    Actual bytes written
 */
uint32_t RT_Vterm_WriteDownBuffer(const void *pBuffer, uint32_t NumBytes);

/**
 * @brief        Read data from upstream buffer
 * @param[out]   pData       Buffer to store read data
 * @param[in]    BufferSize  Maximum bytes to read
 * @retval       uint32_t    Actual bytes read
 */
uint32_t RT_Vterm_ReadUpBuffer(void *pData, uint32_t BufferSize);

/**
 * @brief        Read data from downstream buffer
 * @param[out]   pData       Buffer to store read data
 * @param[in]    BufferSize  Maximum bytes to read
 * @retval       uint32_t    Actual bytes read
 */
uint32_t RT_Vterm_Read(void *pData, uint32_t BufferSize);

/**
 * @brief        Write a single character to upstream buffer
 * @param[in]    c           Character to write
 * @retval       uint32_t    1 if successful, 0 otherwise
 */
uint32_t RT_Vterm_PutChar(char c);

/**
 * @brief        Write a single character to upstream buffer (direct operation)
 * @param[in]    c           Character to write
 * @retval       uint32_t    1 if successful, 0 otherwise
 */
uint32_t RT_Vterm_PutCharSkip(char c);

/**
 * @brief  Check available data in downstream buffer
 * @retval uint32_t  Number of used bytes in downstream buffer
 */
uint32_t RT_Vterm_HasData(void);

/**
 * @brief  Check available data in upstream buffer
 * @retval uint32_t  Number of used bytes in upstream buffer
 */
uint32_t RT_Vterm_HasDataUp(void);

/**
 * @brief  Check if there's available key data in downstream buffer
 * @retval int       1 if data exists, 0 otherwise
 */
int RT_Vterm_HasKey(void);

/**
 * @brief  Block until a key is available in downstream buffer, then return it
 * @retval int       The received character (as unsigned char)
 */
int RT_Vterm_WaitKey(void);

/**
 * @brief  Read a key from downstream buffer (non-blocking)
 * @retval int       The received character (as unsigned char) if available, -1 otherwise
 */
int RT_Vterm_GetKey(void);

/**
 * @brief  Get available write space in upstream buffer
 * @retval uint32_t  Number of free bytes in upstream buffer
 */
uint32_t RT_Vterm_GetAvailWriteSpace(void);

/**
 * @brief        Formatted output to virtual terminal (upstream)
 * @param[in]    sFormat     Format string
 * @param[in]    ...         Variable arguments
 * @retval       int         Number of characters written, negative on error
 */
int RT_Vterm_printf(const char *sFormat, ...);

/**
 * @brief        Formatted output with va_list to virtual terminal (upstream)
 * @param[in]    sFormat     Format string
 * @param[in]    pParamList  Pointer to va_list
 * @retval       int         Number of characters written, negative on error
 */
int RT_Vterm_vprintf(const char *sFormat, va_list *pParamList);

/**
 * @brief  Callback function type for virtual terminal commands
 * @param[in] input  Command input string
 */
typedef void (*vterm_cmd_callback_t)(const char *input);

/**
 * @struct vterm_cmd_entry_t
 * @brief  Structure for virtual terminal command registration
 */
typedef struct
{
    char                 cmd[32];  /**< Command string (max 31 characters) */
    vterm_cmd_callback_t callback; /**< Corresponding callback function */
} vterm_cmd_entry_t;

/**
 * @brief  Switch to virtual terminal mode
 */
void vterm_console(void);

/**
 * @brief  Restore original terminal mode
 */
void restore_original(void);

/**
 * @brief  Initialize virtual terminal
 * @retval int  0 on success, negative error code otherwise
 */
int RT_Vterm_Init(void);

/**< Default mode: Do not block, skip output when buffer is full */
#define RT_Vterm_MODE_NO_BLOCK_SKIP (0)
/**< Mode: Do not block, write partial data when buffer space is insufficient */
#define RT_Vterm_MODE_NO_BLOCK_TRIM (1)
/**< Mode: Block execution if buffer is full until space is available */
#define RT_Vterm_MODE_BLOCK_IF_FIFO_FULL (2)
/**< Mode: Overwrite old data when buffer is full */
#define RT_Vterm_MODE_OVERWRITE (3)

#endif

/*************************** End of file ****************************/