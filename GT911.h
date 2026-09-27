/***************************************************************
 * FILENAME: GT911.h
 * DESCRIPTION:
 *   I2C driver for the GT911 capacitive touch controller.
 * AUTHOR: Sam T
 ***************************************************************/

#ifndef __GT911_H__
#define __GT911_H__

#include "main.h"
#include <stdint.h>

/*** Redefine if necessary ***/
#define GT911_I2C_PORT hi2c2
extern I2C_HandleTypeDef GT911_I2C_PORT;

// The GT911 answers at 0x5D or 0x14, depending on the level of its INT pin as it came out of reset.
// GT911_Init looks for it at both. INT isn't otherwise needed, as touches are polled over I2C.

// Mapping from the GT911's coordinates to the display's, applied in this order.
// Set for the DFR0669 with the ILI9488 driver in landscape orientation.
#define GT911_SWAP_XY		1
#define GT911_INVERT_X		1
#define GT911_INVERT_Y		0

#define GT911_MAX_TOUCHES	5

typedef struct {
    uint8_t id;         // Track ID, which stays the same while a finger stays on the screen
    uint16_t x;
    uint16_t y;
    uint16_t size;
} GT911_Point;

HAL_StatusTypeDef GT911_Init(void);

// Returns how many fingers are on the screen as of the GT911's latest report, and fills points with
// them in display coordinates. The GT911 reports about every 10 ms, so call this at least that often
// to catch every touch.
uint8_t GT911_ReadTouches(GT911_Point points[GT911_MAX_TOUCHES]);

#endif // __GT911_H__
