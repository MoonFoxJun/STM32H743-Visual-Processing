/**
 * @file st7789.h
 * @brief ST7789 320x240 TFT driver for STM32H743 (bare-metal, SPI).
 *
 * Wiring (bit-bang SPI - SCK/CS/DC moved off PA4/PA5/PA6 because the
 * OV5640 camera FPC needs those pins; change in st7789.c if yours differs):
 *   Module pin   SCL    -> PB13   (SPI clock, bit-bang)
 *   Module pin   SDA    -> PA7    (SPI data,  bit-bang)
 *   Module pin   CS     -> PB12   (chip select, GPIO)
 *   Module pin   DC     -> PB14   (data/command, GPIO)
 *   Module pin   RES    -> PA0    (reset, GPIO)
 *   Module pin   BL     -> PA1    (backlight, GPIO, high = on)
 *
 * SPI runs bit-bang on GPIOs, mode 0 (CPOL=0, CPHA=0), MSB first.
 */
#ifndef ST7789_H
#define ST7789_H

#include <stdint.h>

#define ST7789_WIDTH    320U
#define ST7789_HEIGHT   240U

/**
 * Display memory orientation (MADCTL, register 0x36).
 * 0xA0 = MY | MV (mirror Y + swap XY): verified against the ESP32 project
 * that drives this same panel ("esp_lcd_panel_swap_xy(true)" + 
 * "esp_lcd_panel_mirror(false, true)"). If the picture is rotated/mirrored,
 * try: 0x00 / 0x60 / 0xC0 / 0xA0, optionally adding 0x40 (MX) / 0x80 (MY).
 */
#define ST7789_MADCTL   0xA0U

/**
 * Most IPS ST7789 modules need the display-inversion command (0x21) to show
 * correct colors. If colors look "negative", toggle this to 0.
 */
#define ST7789_INVON_ENABLE 1U

/**
 * @brief Initialize GPIO, SPI1 and the ST7789 controller.
 *        Call once at startup, before any drawing.
 */
void st7789_init(void);

/**
 * @brief Set the RAM write window (CASET/RASET) and issue RAMWR.
 *        Afterwards raw pixel bytes stream to the panel row by row.
 */
void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);

/**
 * @brief Stream raw bytes to the display (already inside a window).
 *        Byte order is MSB-first, e.g. for 16 bpp pixels: hi, lo, hi, lo...
 * @param data  pointer to bytes
 * @param len   number of bytes
 */
void st7789_write_buf(const uint8_t *data, uint32_t len);

/**
 * @brief Fill the whole screen with one RGB565 color.
 * @param color RGB565 (bits 15..11 red, 10..5 green, 4..0 blue)
 */
void st7789_fill(uint16_t color);

/**
 * @brief Draw a full-screen 320x240 RGB565 image (MSB-first bytes).
 * @param rgb565 pointer to 320*240*2 bytes
 */
void st7789_draw_full(const uint8_t *rgb565);

/**
 * @brief Blocking millisecond delay (SysTick based).
 */
void st7789_delay_ms(uint32_t ms);

#endif /* ST7789_H */
