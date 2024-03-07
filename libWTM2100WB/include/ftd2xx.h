/*++

Copyright (c) 2001-2021 Future Technology Devices International Limited

THIS SOFTWARE IS PROVIDED BY FUTURE TECHNOLOGY DEVICES INTERNATIONAL LIMITED "AS IS"
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
FUTURE TECHNOLOGY DEVICES INTERNATIONAL LIMITED BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT
OF SUBSTITUTE GOODS OR SERVICES LOSS OF USE, DATA, OR PROFITS OR BUSINESS INTERRUPTION)
HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR
TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE,
EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

FTDI DRIVERS MAY BE USED ONLY IN CONJUNCTION WITH PRODUCTS BASED ON FTDI PARTS.

FTDI DRIVERS MAY BE DISTRIBUTED IN ANY FORM AS LONG AS LICENSE INFORMATION IS NOT MODIFIED.

IF A CUSTOM VENDOR ID AND/OR PRODUCT ID OR DESCRIPTION STRING ARE USED, IT IS THE
RESPONSIBILITY OF THE PRODUCT MANUFACTURER TO MAINTAIN ANY CHANGES AND SUBSEQUENT WHQL
RE-CERTIFICATION AS A RESULT OF MAKING THESE CHANGES.


Module Name:

ftd2xx.h

Abstract:

Native USB device driver for FTDI FT232x, FT245x, FT2232x and FT4232x devices
FTD2XX library definitions

Environment:

kernel & user mode


--*/

/** @file ftd2xx.h
 */

#ifndef FTD2XX_H
#define FTD2XX_H

#ifdef _WIN32


#ifdef FTD2XX_EXPORTS
#define FTD2XX_API __declspec(dllexport)
#elif defined(FTD2XX_STATIC)
// Avoid decorations when linking statically to D2XX.
#define FTD2XX_API
// Static D2XX depends on these Windows libs:
#pragma comment(lib, "setupapi.lib")
#pragma comment(lib, "advapi32.lib")
#pragma comment(lib, "user32.lib")
#else
#define FTD2XX_API __declspec(dllimport)
#endif

#else // _WIN32
// Compiling on non-Windows platform.
#include "WinTypes.h"
// No decorations needed.
#define FTD2XX_API

#endif // _WIN32

#include <windows.h>
/** @name FT_HANDLE 
 * An opaque value used as a handle to an opened FT device.
 */
typedef PVOID	FT_HANDLE;

/** @{
 * @name FT_STATUS 
 * @details Return status values for API calls. 
 */
typedef ULONG	FT_STATUS;

enum {
	FT_OK,
	FT_INVALID_HANDLE,
	FT_DEVICE_NOT_FOUND,
	FT_DEVICE_NOT_OPENED,
	FT_IO_ERROR,
	FT_INSUFFICIENT_RESOURCES,
	FT_INVALID_PARAMETER,
	FT_INVALID_BAUD_RATE,
};

typedef ULONG	FT_DEVICE;


#ifdef __cplusplus
extern "C" {
#endif
	typedef struct _ft_device_list_info_node {
		ULONG Flags;
		ULONG Type;
		ULONG ID;
		DWORD LocId;
		char SerialNumber[16];
		char Description[64];
		FT_HANDLE ftHandle;
	} FT_DEVICE_LIST_INFO_NODE;

#ifdef __cplusplus
}
#endif


#endif	/* FTD2XX_H */
