#include "RT_Vterm.h"
#include <finsh.h>
#include <rtthread.h>
#include <string.h>

#include "RT_tunnel.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>

/**< Virtual Terminal device name */
#define VTERM_DEVICE_NAME "VTerm"

#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#define MAX(a, b) (((a) > (b)) ? (a) : (b))

/**< Tunnel instance for upstream data transmission  */
RT_tunnel_t up_tunnel = RT_NULL;
/**< Tunnel instance for downstream data transmission */
RT_tunnel_t down_tunnel = RT_NULL;

/*****************************************************************************
 * @brief        Initialize the Virtual Terminal module
 *
 * @details      This function acquires free tunnel instances for upstream and
 *               downstream data transmission, and configures their operation modes.
 *               Upstream tunnel is set to write mode, downstream to read mode.
 *
 * @retval       int         RT_EOK on success (0), error code otherwise
 *
 * @note         Initialized after tunnel system but before device drivers
 *****************************************************************************/
int RT_Vterm_Init(void)
{
    up_tunnel   = Get_Free_Tunnel();
    down_tunnel = Get_Free_Tunnel();

    if(!up_tunnel||!down_tunnel)
    {
        LOG_ERROR("no enough tunnel");
        return -RT_ENOMEM;
    }

    Set_Tunnel_Operation(up_tunnel, tunnel_write);
    Set_Tunnel_Operation(down_tunnel, tunnel_read);
    up_tunnel->ID   = 0x56545455; // VTTU
    down_tunnel->ID = 0x56545444; // VTTD

    return RT_EOK;
}
// Ensure initialization after tunnel system and before device drivers
// INIT_PREV_EXPORT(RT_Vterm_Init);
MSH_CMD_EXPORT(RT_Vterm_Init, RT_Vterm_Init)

/*****************************************************************************
 * @brief        Write data to upstream tunnel buffer
 *
 * @param[in]    pBuffer     Pointer to data buffer to be written
 * @param[in]    NumBytes    Number of bytes to write
 *
 * @retval       uint32_t    Actual number of bytes written, 0 if tunnel is invalid
 *
 * @note         Thread-safe operation with internal locking
 *****************************************************************************/
uint32_t RT_Vterm_Write(const void *pBuffer, uint32_t NumBytes)
{
    if (up_tunnel)
        return up_tunnel->write(up_tunnel, (void *)pBuffer, NumBytes);
    return 0;
}

/*****************************************************************************
 * @brief        Write data to downstream tunnel buffer
 *
 * @param[in]    pBuffer     Pointer to data buffer to be written
 * @param[in]    NumBytes    Number of bytes to write
 *
 * @retval       uint32_t    Actual number of bytes written, 0 if tunnel is invalid
 *
 * @note         Thread-safe operation with internal locking
 *****************************************************************************/
uint32_t RT_Vterm_WriteDownBuffer(const void *pBuffer, uint32_t NumBytes)
{
    if (down_tunnel)
        return down_tunnel->write(down_tunnel, (void *)pBuffer, NumBytes);
    return 0;
}

/*****************************************************************************
 * @brief        Read data from upstream tunnel buffer
 *
 * @param[out]   pData       Pointer to buffer to store read data
 * @param[in]    BufferSize  Maximum number of bytes to read
 *
 * @retval       uint32_t    Actual number of bytes read, 0 if tunnel is invalid
 *
 * @note         Thread-safe operation with internal locking
 *****************************************************************************/
uint32_t RT_Vterm_ReadUpBuffer(void *pData, uint32_t BufferSize)
{
    if (up_tunnel)
        return up_tunnel->read(up_tunnel, pData, BufferSize);
    return 0;
}

/*****************************************************************************
 * @brief        Read data from downstream tunnel buffer
 *
 * @param[out]   pData       Pointer to buffer to store read data
 * @param[in]    BufferSize  Maximum number of bytes to read
 *
 * @retval       uint32_t    Actual number of bytes read, 0 if tunnel is invalid
 *
 * @note         Thread-safe operation with internal locking
 *****************************************************************************/
uint32_t RT_Vterm_Read(void *pData, uint32_t BufferSize)
{
    if (down_tunnel)
        return down_tunnel->read(down_tunnel, pData, BufferSize);
    return 0;
}

/**
 * @brief  Check used bytes in downstream tunnel buffer
 * @retval uint32_t  Number of available bytes in downstream buffer, 0 if tunnel invalid
 */
uint32_t RT_Vterm_HasData(void)
{
    if (down_tunnel)
        return Get_Tunnel_Buffer_Used(down_tunnel);
    return 0;
}

/**
 * @brief  Check used bytes in upstream tunnel buffer
 * @retval uint32_t  Number of available bytes in upstream buffer, 0 if tunnel invalid
 */
uint32_t RT_Vterm_HasDataUp(void)
{
    if (up_tunnel)
        return Get_Tunnel_Buffer_Used(up_tunnel);
    return 0;
}

/**
 * @brief  Check if there's available data in downstream buffer
 * @retval int       1 if data exists, 0 otherwise
 */
int RT_Vterm_HasKey(void)
{
    return RT_Vterm_HasData() > 0 ? 1 : 0;
}

/*****************************************************************************
 * @brief        Block until data is available in downstream buffer, then read 1 byte
 *
 * @retval       int         The read byte (as unsigned char) when available
 *
 * @note         This function blocks with 1ms delay between checks
 *               Never returns -1 (infinite loop until data arrives)
 *****************************************************************************/
int RT_Vterm_WaitKey(void)
{
    int  r;
    char c;
    do
    {
        r = RT_Vterm_Read(&c, 1);
        if (r == 1)
            return (unsigned char)c;
        rt_thread_mdelay(1);
    } while (1);
}

/**
 * @brief  Read one byte from downstream buffer (non-blocking)
 * @retval int  The read byte (as unsigned char) if available, -1 otherwise
 */
int RT_Vterm_GetKey(void)
{
    char c;
    int  r = RT_Vterm_Read(&c, 1);
    if (r == 1)
        return (unsigned char)c;
    return -1;
}

/**
 * @brief  Write a single character to upstream buffer
 * @param[in] c  Character to write
 * @retval uint32_t  1 if successful, 0 otherwise
 */
uint32_t RT_Vterm_PutChar(char c)
{
    return RT_Vterm_Write(&c, 1);
}

/**
 * @brief  Write a single character to upstream buffer (direct call)
 * @param[in] c  Character to write
 * @retval uint32_t  1 if successful, 0 otherwise
 */
uint32_t RT_Vterm_PutCharSkip(char c)
{
    if (up_tunnel)
        return up_tunnel->write(up_tunnel, &c, 1);
    return 0;
}

/**
 * @brief  Get available write space in upstream buffer
 * @retval uint32_t  Number of free bytes in upstream buffer, 0 if tunnel invalid
 */
uint32_t RT_Vterm_GetAvailWriteSpace(void)
{
    if (up_tunnel)
        return Get_Tunnel_Buffer_Free(up_tunnel);
    return 0;
}
