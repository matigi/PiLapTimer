/*****************************************************************************
* | File       :   FT3168.cpp
* | Function   :   FT3168 Interface Functions (fixed signature + hardened Get_Point)
******************************************************************************/

#include "FT3168.h"
#include "DEV_Config.h"

FT3168_Struct FT3168;

// --- low-level I2C helpers (use DEV_ I2C wrappers) ---
static void FT3168_I2C_Write_Byte(uint8_t reg, uint8_t value) {
    DEV_I2C_Write_Byte(FT3168_I2C_ADDR, reg, value);
}

static uint8_t FT3168_I2C_Read_Byte(uint8_t reg) {
    return DEV_I2C_Read_Byte(FT3168_I2C_ADDR, reg);
}

static void FT3168_I2C_Read_nByte(uint8_t reg, uint8_t *pData, uint32_t len) {
    DEV_I2C_Read_nByte(FT3168_I2C_ADDR, reg, pData, len);
}

// --- reset (guard if Touch_RST_PIN == -1) ---
void FT3168_Reset() {
    if (Touch_RST_PIN < 0) {
        sleep_ms(50);
        return;
    }

    gpio_put(Touch_RST_PIN, 1);
    sleep_ms(20);
    gpio_put(Touch_RST_PIN, 0);
    sleep_ms(20);
    gpio_put(Touch_RST_PIN, 1);
    sleep_ms(50);
}

/**
 * IMPORTANT:
 * Your header calls for: uint16_t FT3168_ReadState(Value_Information info)
 * The enum values in Value_Information are used like "register selectors" by this library.
 * In Waveshare's variant, they are the register address (or compatible with being cast to uint8_t).
 */
uint16_t FT3168_ReadState(Value_Information info) {
    uint8_t buf[2] = {0, 0};
    FT3168_I2C_Read_nByte((uint8_t)info, buf, 2);
    return (uint16_t)((buf[0] << 8) | buf[1]);
}

void FT3168_Init(uint8_t mode) {
    FT3168_Reset();

    // Power on / init (matches your existing macro set)
    FT3168_I2C_Write_Byte(REG_POWER_MODE, 0x01);

    if (mode != FT3168_Point_Mode) {
        FT3168_I2C_Write_Byte(FT3168_RD_WR_DEVICE_GESTUREID_MODE, 0x01);
        FT3168.mode = FT3168_Gesture_Mode;
    } else {
        FT3168.mode = FT3168_Point_Mode;
    }

    sleep_ms(20);

    // Your header declares uint16_t FT3168_ReadID()
    uint16_t id = FT3168_ReadID();
    printf("FT3168Register_WhoAmI = %d\n", (int)id);
    if (id != 0x03) {
        printf("Invalid device ID: 0x%x\n", (unsigned)id);
        return;
    }
    printf("FT3168 initialized successfully\n");
}

uint16_t FT3168_ReadID() {
    // Your macro name set uses FT3168_RD_DEVICE_ID
    return (uint16_t)FT3168_I2C_Read_Byte(FT3168_RD_DEVICE_ID);
}

/**
 * HARDENED touch read:
 * - FT3168 supports 0..2 touches.
 * - 0xFF (or >2) is usually an I2C glitch, and previously it caused "stuck down forever".
 */
bool FT3168_Get_Point() {
    // Standard FT3x68/FT3168 register block
    // 0x02: TD_STATUS (touch points)
    // 0x03..0x06: P1_XH, P1_XL, P1_YH, P1_YL
    uint8_t buf[5] = {0};

    // Read 0x02..0x06 (5 bytes)
    DEV_I2C_Read_nByte(FT3168_I2C_ADDR, 0x02, buf, 5);

    uint8_t points = buf[0] & 0x0F;  // low nibble is number of touches

    // reject invalid / glitched reads
    if (points == 0 || points > 2) {
        return false;
    }

    uint8_t xh = buf[1];
    uint8_t xl = buf[2];
    uint8_t yh = buf[3];
    uint8_t yl = buf[4];

    // coords are 12-bit: high nibble in XH/YH is status, low nibble is MSBs of coord
    uint16_t x = (uint16_t)((xh & 0x0F) << 8) | xl;
    uint16_t y = (uint16_t)((yh & 0x0F) << 8) | yl;

    // reject obvious garbage
    if ((x == 4095 && y == 4095) || (x == 0xFFFF && y == 0xFFFF)) {
        return false;
    }

    FT3168.x_point = (int)x;
    FT3168.y_point = (int)y;
    return true;
}


uint8_t FT3168_Get_Gesture() {
    // Your macro set uses FT3168_GESTURE_ID
    return (uint8_t)FT3168_ReadState(FT3168_GESTURE_ID);
}
