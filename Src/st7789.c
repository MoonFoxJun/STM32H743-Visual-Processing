/**
 * @file st7789.c
 * @brief ST7789 320x240 TFT driver for STM32H743 (bare-metal, bit-bang SPI).
 *
 * Bit-bang SPI on GPIOs (proven working on this panel; the hardware SPI1
 * path was removed because SPI1_SCK/SPI1_MOSI are only available on PA5/PA7,
 * which the camera now needs). If a faster refresh is wanted later, the LCD
 * can move to SPI2 (SCK=PB13, MOSI=PB15) - see README.
 *
 * LCD pin assignment (bit-bang, any GPIOs work):
 *   SCL  -> PB13   SDA  -> PA7
 *   CS   -> PB12   DC   -> PB14
 *   RES  -> PA0    BL   -> PA1
 *
 * Clocking: H743 boots on 64 MHz HSI, no PLL needed.
 */
#include "st7789.h"

/* ------------------------------------------------------------------ */
/* Peripheral bases (STM32H743)                                        */
/* ------------------------------------------------------------------ */
#define RCC_BASE        0x58024400UL
#define GPIOA_BASE      0x58020000UL
#define GPIOB_BASE      0x58020400UL

/* RCC registers (offsets per stm32h743xx.h RCC_TypeDef) */
#define RCC_AHB4ENR     (*(volatile uint32_t *)(RCC_BASE + 0xE0UL))  /* GPIO on AHB4 on H7! */

/* GPIOA / GPIOB registers */
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00UL))
#define GPIOA_OTYPER    (*(volatile uint32_t *)(GPIOA_BASE + 0x04UL))
#define GPIOA_OSPEEDR   (*(volatile uint32_t *)(GPIOA_BASE + 0x08UL))
#define GPIOA_PUPDR     (*(volatile uint32_t *)(GPIOA_BASE + 0x0CUL))
#define GPIOA_ODR       (*(volatile uint32_t *)(GPIOA_BASE + 0x14UL))
#define GPIOA_BSRR      (*(volatile uint32_t *)(GPIOA_BASE + 0x18UL))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20UL))

#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_OTYPER    (*(volatile uint32_t *)(GPIOB_BASE + 0x04UL))
#define GPIOB_OSPEEDR   (*(volatile uint32_t *)(GPIOB_BASE + 0x08UL))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14UL))
#define GPIOB_BSRR      (*(volatile uint32_t *)(GPIOB_BASE + 0x18UL))

/* SysTick for delays */
#define SYSTICK_BASE            0xE000E010UL
#define SYSTICK_CTRL            (*(volatile uint32_t *)(SYSTICK_BASE + 0x00UL))
#define SYSTICK_LOAD            (*(volatile uint32_t *)(SYSTICK_BASE + 0x04UL))
#define SYSTICK_VAL             (*(volatile uint32_t *)(SYSTICK_BASE + 0x08UL))
#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1UL << 16)

#define HSI_HZ                  64000000UL

/* ------------------------------------------------------------------ */
/* Pin assignment                                                      */
/* ------------------------------------------------------------------ */
/* GPIOA: SDA(PA7), RES(PA0), BL(PA1);  GPIOB: SCL(PB13), CS(PB12), DC(PB14)
 * (The camera owns PA4/PA5/PA6, so the LCD control lines moved to GPIOB.) */
#define PIN_MOSI    (1UL << 7)
#define PIN_RST     (1UL << 0)
#define PIN_BLK     (1UL << 1)
#define PIN_SCK     (1UL << 13)
#define PIN_CS      (1UL << 12)
#define PIN_DC      (1UL << 14)
#define BSRR_MOSI   GPIOA_BSRR
#define BSRR_SCK    GPIOB_BSRR
#define BSRR_CS     GPIOB_BSRR
#define BSRR_DC     GPIOB_BSRR

/* ------------------------------------------------------------------ */
/* Local helpers                                                       */
/* ------------------------------------------------------------------ */

static void delay_ms(uint32_t ms)
{
    /* 1 ms SysTick: reload HSI/1000 - 1, poll COUNTFLAG (no interrupt) */
    SYSTICK_CTRL = 0U;
    SYSTICK_LOAD = (HSI_HZ / 1000UL) - 1UL;
    SYSTICK_VAL  = 0U;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_ENABLE;
    while (ms-- > 0U)
    {
        while ((SYSTICK_CTRL & SYSTICK_CTRL_COUNTFLAG) == 0U)
        {
        }
    }
    SYSTICK_CTRL = 0U;
}

static void cs_high(void) { BSRR_CS = PIN_CS; }
static void cs_low(void)  { BSRR_CS = (PIN_CS << 16); }
static void dc_data(void) { BSRR_DC = PIN_DC; }
static void dc_cmd(void)  { BSRR_DC = (PIN_DC << 16); }

/**
 * Bit-bang SPI byte stream (mode 0: idle low, data valid on rising edge).
 */
static void spi_write(const uint8_t *data, uint32_t len)
{
    while (len-- > 0U)
    {
        uint8_t b = *data++;
        uint32_t mask = 0x80U;
        while (mask != 0U)
        {
            if ((b & mask) != 0U)
            {
                BSRR_MOSI = PIN_MOSI;           /* MOSI = 1 */
            }
            else
            {
                BSRR_MOSI = (PIN_MOSI << 16);   /* MOSI = 0 */
            }
            BSRR_SCK = PIN_SCK;                 /* SCK rising  */
            BSRR_SCK = (PIN_SCK << 16);         /* SCK falling */
            mask >>= 1U;
        }
    }
}

/* Write one command byte */
static void st7789_cmd(uint8_t cmd)
{
    cs_low();
    dc_cmd();
    spi_write(&cmd, 1U);
    cs_high();
}

/* Write one data byte */
static void st7789_data(uint8_t d)
{
    cs_low();
    dc_data();
    spi_write(&d, 1U);
    cs_high();
}

/* Write several data bytes (e.g. parameters or pixel data) */
static void st7789_data_buf(const uint8_t *d, uint32_t len)
{
    cs_low();
    dc_data();
    spi_write(d, len);
    cs_high();
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void st7789_init(void)
{
    /* 1. Enable GPIO clocks */
    RCC_AHB4ENR |= (1UL << 0);  /* GPIOA */
    RCC_AHB4ENR |= (1UL << 1);  /* GPIOB */

    /* 2a. GPIOA: PA7 (SDA), PA0 (RES), PA1 (BL) as push-pull outputs */
    GPIOA_MODER = (GPIOA_MODER & ~0x0000C00FUL) |
                  (1UL << 0)  |   /* PA0 output */
                  (1UL << 2)  |   /* PA1 output */
                  (1UL << 14);    /* PA7 output */
    GPIOA_OTYPER  &= ~0x00000083UL;              /* bits 0,1,7 (one per pin) */
    GPIOA_OSPEEDR |= 0x0000C00FUL;               /* PA0,PA1,PA7 -> bits 0..3,14..15 */
    GPIOA_PUPDR   &= ~0x0000C00FUL;
    /* CS/DC/RST/BLK idle high; MOSI idle low (SPI mode 0) */
    GPIOA_ODR     = (GPIOA_ODR & ~PIN_MOSI) | (PIN_RST | PIN_BLK);

    /* 2b. GPIOB: PB12 (CS), PB13 (SCL), PB14 (DC) as push-pull outputs */
    GPIOB_MODER = (GPIOB_MODER & ~0x3F000000UL) |
                  (1UL << 24) |   /* PB12 output */
                  (1UL << 26) |   /* PB13 output */
                  (1UL << 28);    /* PB14 output */
    GPIOB_OTYPER  &= ~0x00007000UL;              /* bits 12..14 (one per pin) */
    GPIOB_OSPEEDR |= 0x3F000000UL;               /* PB12..14 -> bits 24..29 */
    GPIOB_PUPDR   &= ~0x3F000000UL;
    /* CS/DC idle high; SCK idle low (SPI mode 0) - SCK must NOT start high
     * or the first byte's MSB is lost by the bit-bang code. */
    GPIOB_ODR     = (GPIOB_ODR & ~PIN_SCK) | (PIN_CS | PIN_DC);

    /* 3. Hardware reset of the panel */
    GPIOA_BSRR = (PIN_RST << 16);   /* RST low  */
    delay_ms(20);
    GPIOA_BSRR = PIN_RST;           /* RST high */
    delay_ms(150);

    /* 4. ST7789 initialization sequence
     *    (same commands/params as the ESP-IDF driver that drives this
     *     panel, plus standard display tuning)
     */
    st7789_cmd(0x11);               /* SLPOUT: sleep out */
    delay_ms(120);

    st7789_cmd(0x36);               /* MADCTL: memory data access control */
    st7789_data(ST7789_MADCTL);

    st7789_cmd(0x3A);               /* COLMOD: 16 bpp */
    st7789_data(0x55);

    st7789_cmd(0xB0);               /* RAMCTRL: lock big-endian pixel order */
    st7789_data(0x00);
    st7789_data(0xF0);

    st7789_cmd(0xB2);               /* PORCTRL: porch control */
    st7789_data(0x0C);
    st7789_data(0x0C);
    st7789_data(0x00);
    st7789_data(0x33);
    st7789_data(0x33);

    st7789_cmd(0xB7);               /* GCTRL: gate control */
    st7789_data(0x35);

    st7789_cmd(0xBB);               /* VCOMS: VCOM setting */
    st7789_data(0x19);

    st7789_cmd(0xC0);               /* LCMCTRL: LCM control */
    st7789_data(0x2C);

    st7789_cmd(0xC2);               /* VDVVRHEN */
    st7789_data(0x01);

    st7789_cmd(0xC3);               /* VRHS: VCOM/VCOMH set */
    st7789_data(0x12);

    st7789_cmd(0xC4);               /* VDVSET */
    st7789_data(0x20);

    st7789_cmd(0xC6);               /* FRCTRL2: frame rate */
    st7789_data(0x0F);

    st7789_cmd(0xD0);               /* PWCTRL1: power control */
    st7789_data(0xA4);
    st7789_data(0xA1);

    st7789_cmd(0xE0);               /* PVGAMCTRL: positive gamma */
    {
        static const uint8_t g[14] = {0xD0, 0x04, 0x0D, 0x11, 0x13, 0x2B, 0x3F,
                                      0x54, 0x4C, 0x18, 0x0D, 0x0B, 0x1F, 0x23};
        st7789_data_buf(g, sizeof(g));
    }

    st7789_cmd(0xE1);               /* NVGAMCTRL: negative gamma */
    {
        static const uint8_t g[14] = {0xD0, 0x04, 0x0C, 0x11, 0x13, 0x2C, 0x3F,
                                      0x44, 0x51, 0x2F, 0x1F, 0x1F, 0x20, 0x23};
        st7789_data_buf(g, sizeof(g));
    }

#if ST7789_INVON_ENABLE
    st7789_cmd(0x21);               /* INVON: invert colors (IPS panels) */
#endif

    st7789_cmd(0x29);               /* DISPON: display on */
    delay_ms(20);
}

void st7789_set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    st7789_cmd(0x2A);               /* CASET */
    st7789_data((uint8_t)(x0 >> 8));
    st7789_data((uint8_t)(x0 & 0xFF));
    st7789_data((uint8_t)(x1 >> 8));
    st7789_data((uint8_t)(x1 & 0xFF));

    st7789_cmd(0x2B);               /* RASET */
    st7789_data((uint8_t)(y0 >> 8));
    st7789_data((uint8_t)(y0 & 0xFF));
    st7789_data((uint8_t)(y1 >> 8));
    st7789_data((uint8_t)(y1 & 0xFF));

    st7789_cmd(0x2C);               /* RAMWR */
}

void st7789_write_buf(const uint8_t *data, uint32_t len)
{
    st7789_data_buf(data, len);
}

void st7789_draw_full(const uint8_t *rgb565)
{
    st7789_set_window(0U, 0U, ST7789_WIDTH - 1U, ST7789_HEIGHT - 1U);
    st7789_write_buf(rgb565, (uint32_t)ST7789_WIDTH * ST7789_HEIGHT * 2U);
}

void st7789_fill(uint16_t color)
{
    uint8_t hi = (uint8_t)(color >> 8);
    uint8_t lo = (uint8_t)(color & 0xFF);
    uint32_t n = (uint32_t)ST7789_WIDTH * ST7789_HEIGHT;

    st7789_set_window(0U, 0U, ST7789_WIDTH - 1U, ST7789_HEIGHT - 1U);
    cs_low();
    dc_data();
    while (n-- > 0U)
    {
        spi_write(&hi, 1U);
        spi_write(&lo, 1U);
    }
    cs_high();
}

void st7789_delay_ms(uint32_t ms)
{
    delay_ms(ms);
}
