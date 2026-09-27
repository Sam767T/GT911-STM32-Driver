/***************************************************************
 * FILENAME: GT911.c
 * DESCRIPTION:
 *   I2C driver for the GT911 capacitive touch controller.
 * AUTHOR: Sam T
 ***************************************************************/

#include "stm32u5xx_hal.h"  // Change if not using STM32U5 family
#include "GT911.h"
#include <stdbool.h>
#include <string.h>

#define GT911_ADDRESS_INT_LOW   (0x5D << 1)
#define GT911_ADDRESS_INT_HIGH  (0x14 << 1)
#define GT911_TIMEOUT_MS        10

#define GT911_REG_RESOLUTION    0x8048  // X then Y output maximum, 16-bit little-endian each
#define GT911_REG_PRODUCT_ID    0x8140  // "911"
#define GT911_REG_STATUS        0x814E
#define GT911_REG_POINTS        0x814F  // One 8-byte record per touch

#define GT911_STATUS_READY      0x80    // A new report is waiting. Clear it once the report is read.
#define GT911_STATUS_TOUCHES    0x0F
#define GT911_POINT_SIZE        8

static uint16_t address;
static uint16_t x_resolution, y_resolution;
static GT911_Point touches[GT911_MAX_TOUCHES];
static uint8_t touch_count;

static HAL_StatusTypeDef GT911_Read(uint16_t reg, uint8_t* buff, uint16_t size) {
    return HAL_I2C_Mem_Read(&GT911_I2C_PORT, address, reg, I2C_MEMADD_SIZE_16BIT, buff, size, GT911_TIMEOUT_MS);
}

static HAL_StatusTypeDef GT911_Write(uint16_t reg, uint8_t value) {
    return HAL_I2C_Mem_Write(&GT911_I2C_PORT, address, reg, I2C_MEMADD_SIZE_16BIT, &value, 1, GT911_TIMEOUT_MS);
}

static bool GT911_Responds(uint16_t addr) {
    return HAL_I2C_IsDeviceReady(&GT911_I2C_PORT, addr, 3, GT911_TIMEOUT_MS) == HAL_OK;
}

HAL_StatusTypeDef GT911_Init() {
    if (GT911_Responds(GT911_ADDRESS_INT_LOW)) {
        address = GT911_ADDRESS_INT_LOW;
    } else if (GT911_Responds(GT911_ADDRESS_INT_HIGH)) {
        address = GT911_ADDRESS_INT_HIGH;
    } else {
        return HAL_ERROR;
    }

    uint8_t id[4];
    if (GT911_Read(GT911_REG_PRODUCT_ID, id, sizeof(id)) != HAL_OK) return HAL_ERROR;
    if (memcmp(id, "911", 3) != 0) return HAL_ERROR;

    uint8_t resolution[4];
    if (GT911_Read(GT911_REG_RESOLUTION, resolution, sizeof(resolution)) != HAL_OK) return HAL_ERROR;
    x_resolution = resolution[0] | (resolution[1] << 8);
    y_resolution = resolution[2] | (resolution[3] << 8);
    if (x_resolution == 0 || y_resolution == 0) return HAL_ERROR;

    // Discard any report from before now
    touch_count = 0;
    return GT911_Write(GT911_REG_STATUS, 0);
}

static void GT911_MapToDisplay(GT911_Point* point) {
    uint16_t x = point->x, y = point->y;
    uint16_t width = x_resolution, height = y_resolution;

#if GT911_SWAP_XY
    x = point->y;
    y = point->x;
    width = y_resolution;
    height = x_resolution;
#endif
    if (x >= width) x = width - 1;
    if (y >= height) y = height - 1;
#if GT911_INVERT_X
    x = width - 1 - x;
#endif
#if GT911_INVERT_Y
    y = height - 1 - y;
#endif

    point->x = x;
    point->y = y;
}

uint8_t GT911_ReadTouches(GT911_Point points[GT911_MAX_TOUCHES]) {
    uint8_t status;

    // Keep the previous report if the GT911 hasn't made a new one, or can't be read
    if (GT911_Read(GT911_REG_STATUS, &status, 1) == HAL_OK && (status & GT911_STATUS_READY)) {
        uint8_t count = status & GT911_STATUS_TOUCHES;
        uint8_t raw[GT911_MAX_TOUCHES * GT911_POINT_SIZE];

        if (count > GT911_MAX_TOUCHES) count = GT911_MAX_TOUCHES;
        if (count == 0 || GT911_Read(GT911_REG_POINTS, raw, count * GT911_POINT_SIZE) == HAL_OK) {
            for (uint8_t i = 0; i < count; i++) {
                const uint8_t* record = &raw[i * GT911_POINT_SIZE];
                touches[i].id = record[0];
                touches[i].x = record[1] | (record[2] << 8);
                touches[i].y = record[3] | (record[4] << 8);
                touches[i].size = record[5] | (record[6] << 8);
                GT911_MapToDisplay(&touches[i]);
            }
            touch_count = count;
        }

        // Tell the GT911 the report has been read, so it can make the next one
        GT911_Write(GT911_REG_STATUS, 0);
    }

    memcpy(points, touches, touch_count * sizeof(GT911_Point));
    return touch_count;
}
