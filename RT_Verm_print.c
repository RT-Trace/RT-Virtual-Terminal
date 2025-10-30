#include "rt_Vterm.h"
// #include "rt_Vterm_Conf.h"
#include "RT_tunnel.h"
#include <rtthread.h>
#include <stdio.h>

/*********************************************************************
 *
 *       Defines, configurable
 *
 **********************************************************************
 */
/**< Default buffer size for RT_Vterm_printf (configurable) */
#ifndef RT_Vterm_PRINTF_BUFFER_SIZE
#define RT_Vterm_PRINTF_BUFFER_SIZE (64)
#endif

#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include <ctype.h>
#include <stdlib.h>

/**< Format flag: Left-justify the output (default is right-justify) */
#define FORMAT_FLAG_LEFT_JUSTIFY (1u << 0)
/**< Format flag: Pad with '0' instead of spaces */
#define FORMAT_FLAG_PAD_ZERO (1u << 1)
/**< Format flag: Always print sign (+ for positive, - for negative) */
#define FORMAT_FLAG_PRINT_SIGN (1u << 2)
/**< Format flag: Use alternate output format (e.g., 0x prefix for hex) */
#define FORMAT_FLAG_ALTERNATE (1u << 3)

/**
 * @struct RT_Vterm_PRINTF_DESC
 * @brief  Descriptor structure for managing printf buffer and output state
 */
typedef struct
{
    char    *pBuffer;     /**< Pointer to the temporary printf buffer */
    uint32_t BufferSize;  /**< Size of the temporary printf buffer */
    uint32_t Cnt;         /**< Current number of characters stored in the buffer */
    int      ReturnValue; /**< Total number of characters output (negative on error) */
} RT_Vterm_PRINTF_DESC;

/*****************************************************************************
 * @brief        Store a single character to the printf buffer (internal use)
 *
 * @param[in,out] p  Pointer to the printf descriptor structure
 * @param[in]     c  Character to store
 *
 * @note         1. Automatically adds '\r' before '\n' for line ending consistency
 *               2. Writes buffer to Vterm upstream tunnel when buffer is full
 *               3. Updates ReturnValue (increments on success, sets to -1 on error)
 *****************************************************************************/
static void _StoreChar(RT_Vterm_PRINTF_DESC *p, char c)
{
    uint32_t Cnt;

    // Add '\r' before '\n' to ensure correct line ending on all terminals
    if (c == '\n')
    {
        Cnt = p->Cnt;
        if ((Cnt + 1u) <= p->BufferSize)
        {
            *(p->pBuffer + Cnt) = '\r';
            p->Cnt              = Cnt + 1u;
            p->ReturnValue++;
        }
        // Flush buffer if full after adding '\r'
        if (p->Cnt == p->BufferSize)
        {
            if (RT_Vterm_Write(p->pBuffer, p->Cnt) != p->Cnt)
            {
                p->ReturnValue = -1; // Mark write error
            }
            else
            {
                p->Cnt = 0u; // Reset buffer counter after successful flush
            }
        }
    }

    // Store the target character
    Cnt = p->Cnt;
    if ((Cnt + 1u) <= p->BufferSize)
    {
        *(p->pBuffer + Cnt) = c;
        p->Cnt              = Cnt + 1u;
        p->ReturnValue++;
    }
    // Flush buffer if full after storing the character
    if (p->Cnt == p->BufferSize)
    {
        if (RT_Vterm_Write(p->pBuffer, p->Cnt) != p->Cnt)
        {
            p->ReturnValue = -1; // Mark write error
        }
        else
        {
            p->Cnt = 0u; // Reset buffer counter after successful flush
        }
    }
}

/*****************************************************************************
 * @brief        Print an unsigned integer to Vterm (internal use)
 *
 * @param[in,out] pBufferDesc  Pointer to the printf descriptor structure
 * @param[in]     v            Unsigned integer value to print
 * @param[in]     Base         Numeric base (e.g., 10 for decimal, 16 for hex)
 * @param[in]     NumDigits    Minimum number of digits to print (pads with 0 if needed)
 * @param[in]     FieldWidth   Total field width (pads with spaces/0 if value is shorter)
 * @param[in]     FormatFlags  Format control flags (left-justify, pad-zero, etc.)
 *
 * @note         Handles digit extraction, padding, and alignment per format flags
 *****************************************************************************/
static void _Printunsigned(RT_Vterm_PRINTF_DESC *pBufferDesc, uint32_t v, uint32_t Base, uint32_t NumDigits, uint32_t FieldWidth, uint32_t FormatFlags)
{
    // Lookup table for converting numeric values to ASCII characters (0-9, A-F)
    static const char _aV2C[16] = {'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'A', 'B', 'C', 'D', 'E', 'F'};
    uint32_t          Div;    // Divisor for extracting digits
    uint32_t          Digit;  // Current digit position (e.g., 1000 for 4th digit in decimal)
    uint32_t          Number; // Copy of input value (used for calculating digit count)
    uint32_t          Width;  // Actual number of digits in the input value
    char              c;      // Padding character (space or '0')

    Number = v;
    Digit  = 1u;

    // Step 1: Calculate the actual number of digits in the input value
    Width = 1u;
    while (Number >= Base)
    {
        Number = (Number / Base);
        Width++;
    }
    // Use user-specified minimum digits if larger than actual digit count
    if (NumDigits > Width)
    {
        Width = NumDigits;
    }

    // Step 2: Print leading padding (spaces or '0') if not left-justified
    if ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u)
    {
        if (FieldWidth != 0u)
        {
            // Choose padding character: '0' if pad-zero flag is set (and no min digits), else space
            if (((FormatFlags & FORMAT_FLAG_PAD_ZERO) == FORMAT_FLAG_PAD_ZERO) && (NumDigits == 0u))
            {
                c = '0';
            }
            else
            {
                c = ' ';
            }
            // Add padding until field width is satisfied
            while ((FieldWidth != 0u) && (Width < FieldWidth))
            {
                FieldWidth--;
                _StoreChar(pBufferDesc, c);
                if (pBufferDesc->ReturnValue < 0)
                {
                    break; // Exit on write error
                }
            }
        }
    }

    // Step 3: Calculate the highest digit position (e.g., 1000 for 4-digit decimal)
    if (pBufferDesc->ReturnValue >= 0)
    {
        while (1)
        {
            if (NumDigits > 1u)
            { // Ensure we loop at least NumDigits times (for leading zeros)
                NumDigits--;
            }
            else
            {
                Div = v / Digit;
                if (Div < Base)
                { // Stop when Digit is larger than the highest digit of v
                    break;
                }
            }
            Digit *= Base;
        }

        // Step 4: Extract and print each digit from highest to lowest
        do
        {
            Div = v / Digit;
            v -= Div * Digit; // Remove the current digit from v
            _StoreChar(pBufferDesc, _aV2C[Div]);
            if (pBufferDesc->ReturnValue < 0)
            {
                break; // Exit on write error
            }
            Digit /= Base; // Move to the next lower digit position
        } while (Digit);

        // Step 5: Print trailing spaces if left-justified
        if ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == FORMAT_FLAG_LEFT_JUSTIFY)
        {
            if (FieldWidth != 0u)
            {
                while ((FieldWidth != 0u) && (Width < FieldWidth))
                {
                    FieldWidth--;
                    _StoreChar(pBufferDesc, ' ');
                    if (pBufferDesc->ReturnValue < 0)
                    {
                        break; // Exit on write error
                    }
                }
            }
        }
    }
}

/*****************************************************************************
 * @brief        Print a signed integer to Vterm (internal use)
 *
 * @param[in,out] pBufferDesc  Pointer to the printf descriptor structure
 * @param[in]     v            Signed integer value to print
 * @param[in]     Base         Numeric base (e.g., 10 for decimal, 16 for hex)
 * @param[in]     NumDigits    Minimum number of digits to print (pads with 0 if needed)
 * @param[in]     FieldWidth   Total field width (pads with spaces/0 if value is shorter)
 * @param[in]     FormatFlags  Format control flags (left-justify, pad-zero, etc.)
 *
 * @note         1. Handles negative values by prepending '-'
 *               2. Handles sign printing (+ for positive) if FORMAT_FLAG_PRINT_SIGN is set
 *               3. Delegates unsigned digit printing to _Printunsigned
 *****************************************************************************/
static void _PrintInt(RT_Vterm_PRINTF_DESC *pBufferDesc, int v, uint32_t Base, uint32_t NumDigits, uint32_t FieldWidth, uint32_t FormatFlags)
{
    uint32_t Width;  // Actual number of digits in the input value
    int      Number; // Absolute value of the input (for digit count calculation)

    // Get absolute value of v (handles negative numbers)
    Number = (v < 0) ? -v : v;

    // Step 1: Calculate the actual number of digits in the input value
    Width = 1u;
    while (Number >= (int)Base)
    {
        Number = (Number / (int)Base);
        Width++;
    }
    // Use user-specified minimum digits if larger than actual digit count
    if (NumDigits > Width)
    {
        Width = NumDigits;
    }
    // Reserve 1 character width for sign (+/-) if needed
    if ((FieldWidth > 0u) && ((v < 0) || ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) == FORMAT_FLAG_PRINT_SIGN)))
    {
        FieldWidth--;
    }

    // Step 2: Print leading spaces (if not pad-zero or left-justified)
    if ((((FormatFlags & FORMAT_FLAG_PAD_ZERO) == 0u) || (NumDigits != 0u)) && ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u))
    {
        if (FieldWidth != 0u)
        {
            while ((FieldWidth != 0u) && (Width < FieldWidth))
            {
                FieldWidth--;
                _StoreChar(pBufferDesc, ' ');
                if (pBufferDesc->ReturnValue < 0)
                {
                    break; // Exit on write error
                }
            }
        }
    }

    // Step 3: Print sign (+/-) if needed
    if (pBufferDesc->ReturnValue >= 0)
    {
        if (v < 0)
        {
            v = -v; // Convert to positive for digit printing
            _StoreChar(pBufferDesc, '-');
        }
        else if ((FormatFlags & FORMAT_FLAG_PRINT_SIGN) == FORMAT_FLAG_PRINT_SIGN)
        {
            _StoreChar(pBufferDesc, '+'); // Print '+' for positive values
        }

        // Step 4: Print leading zeros (if pad-zero flag is set)
        if (pBufferDesc->ReturnValue >= 0)
        {
            if (((FormatFlags & FORMAT_FLAG_PAD_ZERO) == FORMAT_FLAG_PAD_ZERO) && ((FormatFlags & FORMAT_FLAG_LEFT_JUSTIFY) == 0u) && (NumDigits == 0u))
            {
                if (FieldWidth != 0u)
                {
                    while ((FieldWidth != 0u) && (Width < FieldWidth))
                    {
                        FieldWidth--;
                        _StoreChar(pBufferDesc, '0');
                        if (pBufferDesc->ReturnValue < 0)
                        {
                            break; // Exit on write error
                        }
                    }
                }
            }

            // Step 5: Print the unsigned part of the number (delegate to _Printunsigned)
            if (pBufferDesc->ReturnValue >= 0)
            {
                _Printunsigned(pBufferDesc, (uint32_t)v, Base, NumDigits, FieldWidth, FormatFlags);
            }
        }
    }
}

/*****************************************************************************
 * @brief        Formatted output to Vterm upstream tunnel (va_list version)
 *
 * @param[in]    sFormat     Format string (supports %c, %d, %u, %x/%X, %s, %p, %%)
 * @param[in]    pParamList  Pointer to va_list containing variable arguments
 *
 * @retval       int         Total number of characters printed (positive on success)
 *                           -1 if write error occurs
 *
 * @note         1. Uses a temporary buffer (size = RT_Vterm_PRINTF_BUFFER_SIZE) to reduce tunnel writes
 *               2. Automatically flushes buffer when full or at the end of output
 *               3. Handles line endings by converting '\n' to "\r\n"
 *****************************************************************************/
int RT_Vterm_vprintf(const char *sFormat, va_list *pParamList)
{
    char                 c;                                     // Current character in format string
    RT_Vterm_PRINTF_DESC BufferDesc;                            // Printf buffer descriptor
    int                  v;                                     // Temporary storage for variable arguments
    uint32_t             NumDigits;                             // Minimum digits (from format string: %.2d)
    uint32_t             FormatFlags;                           // Format flags (from format string: %-0+#)
    uint32_t             FieldWidth;                            // Field width (from format string: %5d)
    char                 acBuffer[RT_Vterm_PRINTF_BUFFER_SIZE]; // Temporary buffer

    // Initialize printf descriptor
    BufferDesc.pBuffer     = acBuffer;
    BufferDesc.BufferSize  = RT_Vterm_PRINTF_BUFFER_SIZE;
    BufferDesc.Cnt         = 0u;
    BufferDesc.ReturnValue = 0;

    // Parse format string character by character
    do
    {
        c = *sFormat;
        sFormat++;
        if (c == 0u)
        {
            break; // Exit if end of format string
        }

        // Handle format specifier (starts with '%')
        if (c == '%')
        {
            // Step 1: Parse format flags (-0+#)
            FormatFlags = 0u;
            v           = 1;
            do
            {
                c = *sFormat;
                switch (c)
                {
                case '-':
                    FormatFlags |= FORMAT_FLAG_LEFT_JUSTIFY;
                    sFormat++;
                    break;
                case '0':
                    FormatFlags |= FORMAT_FLAG_PAD_ZERO;
                    sFormat++;
                    break;
                case '+':
                    FormatFlags |= FORMAT_FLAG_PRINT_SIGN;
                    sFormat++;
                    break;
                case '#':
                    FormatFlags |= FORMAT_FLAG_ALTERNATE;
                    sFormat++;
                    break;
                default:
                    v = 0; // Exit loop if no more flags
                    break;
                }
            } while (v);

            // Step 2: Parse field width (e.g., %5d → FieldWidth=5)
            FieldWidth = 0u;
            do
            {
                c = *sFormat;
                if ((c < '0') || (c > '9'))
                {
                    break; // Exit if not a digit
                }
                sFormat++;
                FieldWidth = (FieldWidth * 10u) + ((uint32_t)c - '0');
            } while (1);

            // Step 3: Parse precision (e.g., %.2d → NumDigits=2)
            NumDigits = 0u;
            c         = *sFormat;
            if (c == '.')
            {
                sFormat++;
                do
                {
                    c = *sFormat;
                    if ((c < '0') || (c > '9'))
                    {
                        break; // Exit if not a digit
                    }
                    sFormat++;
                    NumDigits = NumDigits * 10u + ((uint32_t)c - '0');
                } while (1);
            }

            // Step 4: Skip length modifiers (l/h) (not supported in this implementation)
            c = *sFormat;
            do
            {
                if ((c == 'l') || (c == 'h'))
                {
                    sFormat++;
                    c = *sFormat;
                }
                else
                {
                    break;
                }
            } while (1);

            // Step 5: Handle format specifiers (c/d/u/x/X/s/p/%)
            switch (c)
            {
            case 'c': // Print single character
            {
                char c0;
                v  = va_arg(*pParamList, int); // va_arg returns int for char (promotion)
                c0 = (char)v;
                _StoreChar(&BufferDesc, c0);
                break;
            }
            case 'd': // Print signed decimal integer
                v = va_arg(*pParamList, int);
                _PrintInt(&BufferDesc, v, 10u, NumDigits, FieldWidth, FormatFlags);
                break;
            case 'u': // Print unsigned decimal integer
                v = va_arg(*pParamList, int);
                _Printunsigned(&BufferDesc, (uint32_t)v, 10u, NumDigits, FieldWidth, FormatFlags);
                break;
            case 'x': // Print unsigned hexadecimal (uppercase, same as 'X' here)
            case 'X':
                v = va_arg(*pParamList, int);
                _Printunsigned(&BufferDesc, (uint32_t)v, 16u, NumDigits, FieldWidth, FormatFlags);
                break;
            case 's': // Print null-terminated string
            {
                const char *s = va_arg(*pParamList, const char *);
                do
                {
                    c = *s;
                    s++;
                    if (c == '\0')
                    {
                        break; // Exit at end of string
                    }
                    _StoreChar(&BufferDesc, c);
                } while (BufferDesc.ReturnValue >= 0);
            }
            break;
            case 'p': // Print pointer (8-digit hexadecimal)
                v = va_arg(*pParamList, int);
                _Printunsigned(&BufferDesc, (uint32_t)v, 16u, 8u, 8u, 0u);
                break;
            case '%': // Print literal '%'
                _StoreChar(&BufferDesc, '%');
                break;
            default: // Ignore unknown specifiers
                break;
            }
            sFormat++; // Move past the specifier character
        }
        else
        {
            // Not a format specifier: store the character directly
            _StoreChar(&BufferDesc, c);
        }
    } while (BufferDesc.ReturnValue >= 0);

    // Flush remaining characters in the buffer (if any)
    if (BufferDesc.ReturnValue > 0)
    {
        if (BufferDesc.Cnt != 0u)
        {
            RT_Vterm_Write(acBuffer, BufferDesc.Cnt);
        }
        BufferDesc.ReturnValue += (int)BufferDesc.Cnt;
    }

    return BufferDesc.ReturnValue;
}

/*****************************************************************************
 * @brief        Formatted output to Vterm upstream tunnel (user-facing)
 *
 * @param[in]    sFormat  Format string (supports %c, %d, %u, %x/%X, %s, %p, %%)
 * @param[in]    ...      Variable arguments matching the format string
 *
 * @retval       int      Total number of characters printed (positive on success)
 *                        -1 if write error occurs
 *
 * @note         Wraps RT_Vterm_vprintf and handles variable argument list setup/teardown
 *****************************************************************************/
int RT_Vterm_printf(const char *sFormat, ...)
{
    int     r;         // Return value from RT_Vterm_vprintf
    va_list ParamList; // Variable argument list

    va_start(ParamList, sFormat); // Initialize argument list
    r = RT_Vterm_vprintf(sFormat, &ParamList);
    va_end(ParamList); // Clean up argument list

    return r;
}

/*************************** End of file ****************************/
