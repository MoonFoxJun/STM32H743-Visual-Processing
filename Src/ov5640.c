/**
 * @file ov5640.c
 * @brief OV5640 camera driver (bare-metal): SCCB control + register init.
 *
 * Init table: 246 registers, RGB565 QVGA 320x240, extracted from
 * the ST official stm32-ov5640 driver (ov5640.c + ov5640_reg.h) by
 * tools/extract_ov5640_table.py - do not edit by hand.
 */
#include "ov5640.h"

#include "sccb.h"

/* ------------------------------------------------------------------ */
/* Register init table (generated - do not edit)                       */
/* ------------------------------------------------------------------ */
static const uint16_t s_ov5640_init[][2] = {
    {0x3103, 0x11}, {0x3008, 0x82}, {0x3103, 0x03}, {0x3630, 0x36},
    {0x3631, 0x0E}, {0x3632, 0xE2}, {0x3633, 0x12}, {0x3621, 0xE0},
    {0x3704, 0xA0}, {0x3703, 0x5A}, {0x3715, 0x78}, {0x3717, 0x01},
    {0x370B, 0x60}, {0x3705, 0x1A}, {0x3905, 0x02}, {0x3906, 0x10},
    {0x3901, 0x0A}, {0x3731, 0x12}, {0x3600, 0x08}, {0x3601, 0x33},
    {0x302D, 0x60}, {0x3620, 0x52}, {0x371B, 0x20}, {0x471C, 0x50},
    {0x3A13, 0x43}, {0x3A18, 0x00}, {0x3A19, 0xF8}, {0x3635, 0x13},
    {0x3636, 0x03}, {0x3634, 0x40}, {0x3622, 0x01}, {0x3C01, 0x34},
    {0x3C04, 0x28}, {0x3C05, 0x98}, {0x3C06, 0x00}, {0x3C07, 0x00},
    {0x3C08, 0x01}, {0x3C09, 0x2C}, {0x3C0A, 0x9C}, {0x3C0B, 0x40},
    {0x3820, 0x06}, {0x3821, 0x00}, {0x3814, 0x31}, {0x3815, 0x31},
    {0x3800, 0x00}, {0x3801, 0x00}, {0x3802, 0x00}, {0x3803, 0x04},
    {0x3804, 0x0A}, {0x3805, 0x3F}, {0x3806, 0x07}, {0x3807, 0x9B},
    {0x3808, 0x01}, {0x3809, 0x40}, {0x380A, 0x00}, {0x380B, 0xF0},
    {0x380C, 0x07}, {0x380D, 0x90}, {0x380E, 0x04}, {0x380F, 0x40},
    {0x3810, 0x00}, {0x3811, 0x10}, {0x3812, 0x00}, {0x3813, 0x06},
    {0x3618, 0x00}, {0x3612, 0x29}, {0x3708, 0x64}, {0x3709, 0x52},
    {0x370C, 0x03}, {0x3A02, 0x03}, {0x3A03, 0xD8}, {0x3A08, 0x01},
    {0x3A09, 0x27}, {0x3A0A, 0x00}, {0x3A0B, 0xF6}, {0x3A0E, 0x03},
    {0x3A0D, 0x04}, {0x3A14, 0x03}, {0x3A15, 0xD8}, {0x4001, 0x02},
    {0x4004, 0x02}, {0x3000, 0x00}, {0x3002, 0x1C}, {0x3004, 0xFF},
    {0x3006, 0xC3}, {0x300E, 0x58}, {0x302E, 0x00}, {0x4740, 0x22},
    {0x4300, 0x6F}, {0x501F, 0x01}, {0x4713, 0x03}, {0x4407, 0x04},
    {0x440E, 0x00}, {0x460B, 0x35}, {0x460C, 0x23}, {0x4837, 0x22},
    {0x3824, 0x02}, {0x5000, 0xA7}, {0x5001, 0xA3}, {0x5180, 0xFF},
    {0x5181, 0xF2}, {0x5182, 0x00}, {0x5183, 0x14}, {0x5184, 0x25},
    {0x5185, 0x24}, {0x5186, 0x09}, {0x5187, 0x09}, {0x5188, 0x09},
    {0x5189, 0x75}, {0x518A, 0x54}, {0x518B, 0xE0}, {0x518C, 0xB2},
    {0x518D, 0x42}, {0x518E, 0x3D}, {0x518F, 0x56}, {0x5190, 0x46},
    {0x5191, 0xF8}, {0x5192, 0x04}, {0x5193, 0x70}, {0x5194, 0xF0},
    {0x5195, 0xF0}, {0x5196, 0x03}, {0x5197, 0x01}, {0x5198, 0x04},
    {0x5199, 0x12}, {0x519A, 0x04}, {0x519B, 0x00}, {0x519C, 0x06},
    {0x519D, 0x82}, {0x519E, 0x38}, {0x5381, 0x1E}, {0x5382, 0x5B},
    {0x5383, 0x08}, {0x5384, 0x0A}, {0x5385, 0x7E}, {0x5386, 0x88},
    {0x5387, 0x7C}, {0x5388, 0x6C}, {0x5389, 0x10}, {0x538A, 0x01},
    {0x538B, 0x98}, {0x5300, 0x08}, {0x5301, 0x30}, {0x5302, 0x10},
    {0x5303, 0x00}, {0x5304, 0x08}, {0x5305, 0x30}, {0x5306, 0x08},
    {0x5307, 0x16}, {0x5308, 0x08}, {0x5309, 0x30}, {0x530A, 0x04},
    {0x530B, 0x06}, {0x5480, 0x01}, {0x5481, 0x08}, {0x5482, 0x14},
    {0x5483, 0x28}, {0x5484, 0x51}, {0x5485, 0x65}, {0x5486, 0x71},
    {0x5487, 0x7D}, {0x5488, 0x87}, {0x5489, 0x91}, {0x548A, 0x9A},
    {0x548B, 0xAA}, {0x548C, 0xB8}, {0x548D, 0xCD}, {0x548E, 0xDD},
    {0x548F, 0xEA}, {0x5490, 0x1D}, {0x5580, 0x02}, {0x5583, 0x40},
    {0x5584, 0x10}, {0x5589, 0x10}, {0x558A, 0x00}, {0x558B, 0xF8},
    {0x5800, 0x23}, {0x5801, 0x14}, {0x5802, 0x0F}, {0x5803, 0x0F},
    {0x5804, 0x12}, {0x5805, 0x26}, {0x5806, 0x0C}, {0x5807, 0x08},
    {0x5808, 0x05}, {0x5809, 0x05}, {0x580A, 0x08}, {0x580B, 0x0D},
    {0x580C, 0x08}, {0x580D, 0x03}, {0x580E, 0x00}, {0x580F, 0x00},
    {0x5810, 0x03}, {0x5811, 0x09}, {0x5812, 0x07}, {0x5813, 0x03},
    {0x5814, 0x00}, {0x5815, 0x01}, {0x5816, 0x03}, {0x5817, 0x08},
    {0x5818, 0x0D}, {0x5819, 0x08}, {0x581A, 0x05}, {0x581B, 0x06},
    {0x581C, 0x08}, {0x581D, 0x0E}, {0x581E, 0x29}, {0x581F, 0x17},
    {0x5820, 0x11}, {0x5821, 0x11}, {0x5822, 0x15}, {0x5823, 0x28},
    {0x5824, 0x46}, {0x5825, 0x26}, {0x5826, 0x08}, {0x5827, 0x26},
    {0x5828, 0x64}, {0x5829, 0x26}, {0x582A, 0x24}, {0x582B, 0x22},
    {0x582C, 0x24}, {0x582D, 0x24}, {0x582E, 0x06}, {0x582F, 0x22},
    {0x5830, 0x40}, {0x5831, 0x42}, {0x5832, 0x24}, {0x5833, 0x26},
    {0x5834, 0x24}, {0x5835, 0x22}, {0x5836, 0x22}, {0x5837, 0x26},
    {0x5838, 0x44}, {0x5839, 0x24}, {0x583A, 0x26}, {0x583B, 0x28},
    {0x583C, 0x42}, {0x583D, 0xCE}, {0x5025, 0x00}, {0x3A0F, 0x30},
    {0x3A10, 0x28}, {0x3A1B, 0x30}, {0x3A1E, 0x26}, {0x3A11, 0x60},
    {0x3A1F, 0x14}, {0x3008, 0x02},
};

/* ------------------------------------------------------------------ */
/* Register bases (STM32H743)                                          */
/* ------------------------------------------------------------------ */
#define RCC_BASE        0x58024400UL
#define GPIOB_BASE      0x58020400UL
#define GPIOC_BASE      0x58020800UL
#define GPIOD_BASE      0x58020C00UL
#define TIM2_BASE       0x40000000UL

#define RCC_AHB4ENR     (*(volatile uint32_t *)(RCC_BASE + 0xE0UL))
#define RCC_APB1LENR    (*(volatile uint32_t *)(RCC_BASE + 0xE8UL))

#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00UL))
#define GPIOC_OTYPER    (*(volatile uint32_t *)(GPIOC_BASE + 0x04UL))
#define GPIOC_OSPEEDR   (*(volatile uint32_t *)(GPIOC_BASE + 0x08UL))
#define GPIOC_PUPDR     (*(volatile uint32_t *)(GPIOC_BASE + 0x0CUL))
#define GPIOC_ODR       (*(volatile uint32_t *)(GPIOC_BASE + 0x14UL))
#define GPIOC_BSRR      (*(volatile uint32_t *)(GPIOC_BASE + 0x18UL))

#define GPIOD_MODER     (*(volatile uint32_t *)(GPIOD_BASE + 0x00UL))
#define GPIOD_OTYPER    (*(volatile uint32_t *)(GPIOD_BASE + 0x04UL))
#define GPIOD_OSPEEDR   (*(volatile uint32_t *)(GPIOD_BASE + 0x08UL))
#define GPIOD_PUPDR     (*(volatile uint32_t *)(GPIOD_BASE + 0x0CUL))
#define GPIOD_ODR       (*(volatile uint32_t *)(GPIOD_BASE + 0x14UL))
#define GPIOD_BSRR      (*(volatile uint32_t *)(GPIOD_BASE + 0x18UL))

#define GPIOA_MODER     (*(volatile uint32_t *)(0x58020000UL + 0x00UL))
#define GPIOA_AFRL      (*(volatile uint32_t *)(0x58020000UL + 0x20UL))

#define TIM2_CR1        (*(volatile uint32_t *)(TIM2_BASE + 0x00UL))
#define TIM2_CCMR1      (*(volatile uint32_t *)(TIM2_BASE + 0x18UL))
#define TIM2_CCER       (*(volatile uint32_t *)(TIM2_BASE + 0x20UL))
#define TIM2_PSC        (*(volatile uint32_t *)(TIM2_BASE + 0x28UL))
#define TIM2_ARR        (*(volatile uint32_t *)(TIM2_BASE + 0x2CUL))
#define TIM2_CCR1       (*(volatile uint32_t *)(TIM2_BASE + 0x34UL))

/* SysTick */
#define SYSTICK_BASE            0xE000E010UL
#define SYSTICK_CTRL            (*(volatile uint32_t *)(SYSTICK_BASE + 0x00UL))
#define SYSTICK_LOAD            (*(volatile uint32_t *)(SYSTICK_BASE + 0x04UL))
#define SYSTICK_VAL             (*(volatile uint32_t *)(SYSTICK_BASE + 0x08UL))
#define SYSTICK_CTRL_ENABLE     (1UL << 0)
#define SYSTICK_CTRL_CLKSOURCE  (1UL << 2)
#define SYSTICK_CTRL_COUNTFLAG  (1UL << 16)

#define HSI_HZ                  64000000UL

#define OV5640_ADDR             0x78U

#define PIN_RESET   (1UL << 4)   /* PC4 - camera reset (active low) */
#define PIN_PWDN    (1UL << 14)  /* PD14 - power down (low = on)    */

/* ------------------------------------------------------------------ */
/* Local helpers                                                       */
/* ------------------------------------------------------------------ */

static void delay_ms(uint32_t ms)
{
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

static void ov5640_power_on(void)
{
    /* PD14 = PWDN: low = powered on */
    RCC_AHB4ENR |= (1UL << 3);                      /* GPIOD */
    GPIOD_MODER  = (GPIOD_MODER & ~(0x3UL << 28)) | (0x1UL << 28); /* PD14 out */
    GPIOD_OTYPER &= ~PIN_PWDN;
    GPIOD_OSPEEDR |= (0x3UL << 28);                 /* PD14 -> bits 28..29 */
    GPIOD_PUPDR  &= ~(0x3UL << 28);
    GPIOD_BSRR   = PIN_PWDN;                        /* PWDN high first      */
    delay_ms(10);
    GPIOD_BSRR   = (PIN_PWDN << 16);                /* PWDN low = power on  */
    delay_ms(10);

    /* PC4 = RESET: active-low pulse */
    RCC_AHB4ENR |= (1UL << 2);                      /* GPIOC */
    GPIOC_MODER  = (GPIOC_MODER & ~(0x3UL << 8)) | (0x1UL << 8);  /* PC4 out */
    GPIOC_OTYPER &= ~PIN_RESET;
    GPIOC_OSPEEDR |= (0x3UL << 8);                  /* PC4 -> bits 8..9 */
    GPIOC_PUPDR  &= ~(0x3UL << 8);
    GPIOC_ODR    |= PIN_RESET;                      /* reset high (idle) */
    GPIOC_BSRR   = (PIN_RESET << 16);               /* reset low  */
    delay_ms(1);
    GPIOC_BSRR   = PIN_RESET;                       /* reset high */
    delay_ms(100);                                  /* let the sensor settle */
}

/* XCLK on PA5 = TIM2_CH1 (AF1): 64 MHz / 3 = 21.33 MHz, 50% duty */
static void ov5640_start_xclk(void)
{
    RCC_AHB4ENR  |= (1UL << 0);                     /* GPIOA */
    RCC_APB1LENR |= (1UL << 0);                     /* TIM2  */

    GPIOA_MODER = (GPIOA_MODER & ~(0x3UL << 10)) | (0x2UL << 10); /* PA5 AF */
    GPIOA_AFRL  = (GPIOA_AFRL & ~(0xFUL << 20)) | (0x1UL << 20);  /* AF1  */

    TIM2_PSC  = 0U;         /* /1 */
    TIM2_ARR  = 2U;         /* period 3 ticks -> 64/3 MHz */
    TIM2_CCR1 = 1U;         /* 50% duty */
    TIM2_CCMR1 = (0x6UL << 4);   /* OC1M = PWM mode 1, CC1S = output */
    TIM2_CCER  = 0x1UL;          /* CC1E */
    TIM2_CR1  |= 0x1UL;          /* CEN */
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

uint8_t ov5640_diag  = 0U;
uint8_t ov5640_id_hi = 0U;
uint8_t ov5640_id_lo = 0U;

/* SCCB address: auto-detected between 0x78 and 0x7A (address strapping
 * differs between OV5640 modules). */
static uint8_t s_ov5640_addr = OV5640_ADDR;

int ov5640_init(void)
{
    static const uint8_t addrs[2] = {0x78U, 0x7AU};
    uint32_t a, i;

    ov5640_diag = 0U;
    s_ov5640_addr = 0U;

    sccb_init();
    ov5640_start_xclk();
    ov5640_power_on();

    /* Verify the chip ID (0x300A = 0x56, 0x300B = 0x40), trying both
     * possible SCCB addresses */
    for (a = 0U; a < 2U; a++)
    {
        s_ov5640_addr = addrs[a];
        if (sccb_read_reg(s_ov5640_addr, 0x300A, &ov5640_id_hi) &&
            sccb_read_reg(s_ov5640_addr, 0x300B, &ov5640_id_lo))
        {
            break;
        }
        ov5640_id_hi = 0U;
        ov5640_id_lo = 0U;
    }
    if (a >= 2U)
    {
        ov5640_diag = 1U;   /* SCCB no response on 0x78 or 0x7A */
        return 0;
    }
    if (ov5640_id_hi != 0x56U || ov5640_id_lo != 0x40U)
    {
        ov5640_diag = 2U;   /* ID mismatch */
        return 0;
    }

    /* Write the full init sequence */
    for (i = 0U; i < (sizeof(s_ov5640_init) / sizeof(s_ov5640_init[0])); i++)
    {
        if (!sccb_write_reg(s_ov5640_addr, s_ov5640_init[i][0], (uint8_t)s_ov5640_init[i][1]))
        {
            ov5640_diag = 3U;   /* init table write NACK */
            return 0;
        }
    }
    return 1;
}

int ov5640_start_stream(void)
{
    return sccb_write_reg(s_ov5640_addr, 0x3008, 0x02);
}

int ov5640_stop_stream(void)
{
    return sccb_write_reg(s_ov5640_addr, 0x3008, 0x42);
}

int ov5640_read_reg(uint16_t reg, uint8_t *out)
{
    return sccb_read_reg(s_ov5640_addr, reg, out);
}
