/**
 * @file sccb.c
 * @brief SCCB (Serial Camera Control Bus) - bit-bang I2C for the OV5640.
 *
 * Open-drain outputs on PB8 (SCL) / PB9 (SDA) + internal pull-ups.
 * The OV5640 module usually has its own pull-ups as well.
 *
 * Timing: ~100 kHz bit rate (5 us half-period), 64 MHz HSI core clock.
 */
#include "sccb.h"

/* ------------------------------------------------------------------ */
/* Register bases (STM32H743)                                          */
/* ------------------------------------------------------------------ */
#define RCC_BASE        0x58024400UL
#define GPIOB_BASE      0x58020400UL
#define RCC_AHB4ENR     (*(volatile uint32_t *)(RCC_BASE + 0xE0UL))

#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_OTYPER    (*(volatile uint32_t *)(GPIOB_BASE + 0x04UL))
#define GPIOB_OSPEEDR   (*(volatile uint32_t *)(GPIOB_BASE + 0x08UL))
#define GPIOB_PUPDR     (*(volatile uint32_t *)(GPIOB_BASE + 0x0CUL))
#define GPIOB_IDR       (*(volatile uint32_t *)(GPIOB_BASE + 0x10UL))
#define GPIOB_ODR       (*(volatile uint32_t *)(GPIOB_BASE + 0x14UL))
#define GPIOB_BSRR      (*(volatile uint32_t *)(GPIOB_BASE + 0x18UL))

/* SysTick */
#define SYSTICK_BASE            0xE000E010UL
#define SYSTICK_CTRL            (*(volatile uint32_t *)(SYSTICK_BASE + 0x00UL))
#define SYSTICK_LOAD            (*(volatile uint32_t *)(SYSTICK_BASE + 0x04UL))
#define SYSTICK_VAL             (*(volatile uint32_t *)(SYSTICK_BASE + 0x08UL))
#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1UL << 16)

#define HSI_HZ                  64000000UL

#define OV5640_ADDR             0x78U   /* 8-bit write address */

#define PIN_SCL     (1UL << 8)   /* PB8 */
#define PIN_SDA     (1UL << 9)   /* PB9 */

/* ------------------------------------------------------------------ */
/* Helpers                                                             */
/* ------------------------------------------------------------------ */

static void delay_us(uint32_t us)
{
    SYSTICK_CTRL = 0U;
    SYSTICK_LOAD = (us * (HSI_HZ / 1000000UL)) - 1UL;
    SYSTICK_VAL  = 0U;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_ENABLE;
    while ((SYSTICK_CTRL & SYSTICK_CTRL_COUNTFLAG) == 0U)
    {
    }
    SYSTICK_CTRL = 0U;
}

/* Open-drain: "high" means release (line pulled up by resistors) */
/* PB8 = SCL push-pull output; PB9 = SDA push-pull output that is
 * tri-stated (switched to input) whenever the slave must drive it
 * (ACK sampling / data reads). This does NOT depend on external
 * pull-ups, so it works even if the camera module has none. */
static void sda_out(void) { GPIOB_MODER = (GPIOB_MODER & ~0x000C0000UL) | (0x00040000UL); }
static void sda_in(void)  { GPIOB_MODER &= ~0x000C0000UL; }
static void scl_high(void) { GPIOB_BSRR = PIN_SCL; }
static void scl_low(void)  { GPIOB_BSRR = (PIN_SCL << 16); }
static void sda_high(void) { sda_out(); GPIOB_BSRR = PIN_SDA; }
static void sda_low(void)  { sda_out(); GPIOB_BSRR = (PIN_SDA << 16); }
static uint32_t sda_read(void) { sda_in(); return (GPIOB_IDR & PIN_SDA) != 0U; }

#define HALF_PERIOD_US  5U   /* ~100 kHz */

static void sccb_start(void)
{
    sda_high();
    scl_high();
    delay_us(HALF_PERIOD_US);
    sda_low();                  /* SDA falls while SCL high = START */
    delay_us(HALF_PERIOD_US);
    scl_low();
}

static void sccb_stop(void)
{
    sda_low();
    scl_high();
    delay_us(HALF_PERIOD_US);
    sda_high();                 /* SDA rises while SCL high = STOP */
    delay_us(HALF_PERIOD_US);
}

/* Send one byte, return 1 if slave ACKed (SDA low at 9th clock) */
static int sccb_tx_byte(uint8_t b)
{
    uint32_t i;
    for (i = 0U; i < 8U; i++)
    {
        if ((b & 0x80U) != 0U)
        {
            sda_high();
        }
        else
        {
            sda_low();
        }
        b <<= 1U;
        delay_us(HALF_PERIOD_US);
        scl_high();
        delay_us(HALF_PERIOD_US);
        scl_low();
        delay_us(HALF_PERIOD_US);
    }
    /* 9th clock: release SDA (tri-state; pull-up holds it high unless the
     * slave ACKs low), sample ACK */
    sda_in();
    delay_us(HALF_PERIOD_US);
    scl_high();
    delay_us(HALF_PERIOD_US);
    {
        uint32_t ack = sda_read();
        scl_low();
        delay_us(HALF_PERIOD_US);
        return (int)(ack == 0U);
    }
}

/* Receive one byte, send ACK (0) or NACK (1) at the 9th clock */
static uint8_t sccb_rx_byte(int last)
{
    uint8_t b = 0U;
    uint32_t i;
    sda_in();                   /* release SDA (pull-up holds it high) */
    for (i = 0U; i < 8U; i++)
    {
        b <<= 1U;
        scl_high();
        delay_us(HALF_PERIOD_US);
        if (sda_read() != 0U)
        {
            b |= 1U;
        }
        scl_low();
        delay_us(HALF_PERIOD_US);
    }
    /* 9th clock: master drives ACK (low) or NACK (high) */
    if (last)
    {
        sda_high();             /* NACK: stop reading */
    }
    else
    {
        sda_low();              /* ACK: read more */
    }
    delay_us(HALF_PERIOD_US);
    scl_high();
    delay_us(HALF_PERIOD_US);
    scl_low();
    delay_us(HALF_PERIOD_US);
    sda_high();
    return b;
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void sccb_init(void)
{
    RCC_AHB4ENR |= (1UL << 1);  /* GPIOB */

    /* PB8/PB9: push-pull outputs, pull-up enabled (helps the tri-stated
     * SDA read), high speed. SDA is switched to input on demand. */
    GPIOB_MODER = (GPIOB_MODER & ~0x000F0000UL) | (0x00050000UL); /* pins 8,9 = 01 output */
    GPIOB_OTYPER &= ~(PIN_SCL | PIN_SDA);        /* push-pull */
    GPIOB_OSPEEDR |= ((PIN_SCL | PIN_SDA) << 8); /* pins 8,9 -> bits 16..19 */
    GPIOB_PUPDR |= ((PIN_SCL | PIN_SDA) << 8);   /* pull-up (2 bits per pin) */
    GPIOB_ODR |= (PIN_SCL | PIN_SDA);            /* idle high  */
}

int sccb_write_reg(uint8_t dev_addr, uint16_t reg, uint8_t val)
{
    int ok;

    sccb_start();
    ok = sccb_tx_byte(dev_addr & 0xFEU);        /* write bit */
    ok &= sccb_tx_byte((uint8_t)(reg >> 8));
    ok &= sccb_tx_byte((uint8_t)(reg & 0xFF));
    ok &= sccb_tx_byte(val);
    sccb_stop();

    return ok;
}

int sccb_read_reg(uint8_t dev_addr, uint16_t reg, uint8_t *out)
{
    int ok;

    if (out == 0)
    {
        return 0;
    }

    /* Phase 1: write the register address */
    sccb_start();
    ok = sccb_tx_byte(dev_addr & 0xFEU);        /* write bit */
    ok &= sccb_tx_byte((uint8_t)(reg >> 8));
    ok &= sccb_tx_byte((uint8_t)(reg & 0xFF));
    sccb_stop();

    if (!ok)
    {
        return 0;
    }

    /* Phase 2: repeated read */
    sccb_start();
    ok = sccb_tx_byte(dev_addr | 0x01U);        /* read bit  */
    if (!ok)
    {
        sccb_stop();
        return 0;
    }
    *out = sccb_rx_byte(1);                     /* NACK on last byte */
    sccb_stop();

    return 1;
}
