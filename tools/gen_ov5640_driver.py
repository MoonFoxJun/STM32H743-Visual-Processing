#!/usr/bin/env python3
"""Generate Src/ov5640.c with the extracted register table embedded.

Reads the table text (as printed by extract_ov5640_table.py) from the
first argument (default: <TEMP>/ov5640_table.txt) and writes a complete
bare-metal OV5640 driver.
"""
import os
import re
import sys


def main():
    table_file = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
        os.environ.get("TEMP", ""), "ov5640_table.txt")
    with open(table_file, encoding="utf-8") as f:
        text = f.read()
    m = re.search(r"// (\d+) registers", text)
    count = int(m.group(1)) if m else 0
    entries = re.findall(r"\{0x([0-9A-Fa-f]{4}), 0x([0-9A-Fa-f]{2})\}", text)
    if len(entries) != count:
        print(f"WARNING: header says {count}, parsed {len(entries)}")

    lines = []
    for i in range(0, len(entries), 4):
        row = ", ".join(
            "{0x%s, 0x%s}" % (a.upper(), v.upper()) for a, v in entries[i:i + 4])
        lines.append(f"    {row},")
    table_text = "\n".join(lines)

    c = f"""/**
 * @file ov5640.c
 * @brief OV5640 camera driver (bare-metal): SCCB control + register init.
 *
 * Init table: {len(entries)} registers, RGB565 QVGA 320x240, extracted from
 * the ST official stm32-ov5640 driver (ov5640.c + ov5640_reg.h) by
 * tools/extract_ov5640_table.py - do not edit by hand.
 */
#include "ov5640.h"

#include "sccb.h"

/* ------------------------------------------------------------------ */
/* Register init table (generated - do not edit)                       */
/* ------------------------------------------------------------------ */
static const uint16_t s_ov5640_init[][2] = {{
{table_text}
}};

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
{{
    SYSTICK_CTRL = 0U;
    SYSTICK_LOAD = (HSI_HZ / 1000UL) - 1UL;
    SYSTICK_VAL  = 0U;
    SYSTICK_CTRL = SYSTICK_CTRL_CLKSOURCE | SYSTICK_CTRL_ENABLE;
    while (ms-- > 0U)
    {{
        while ((SYSTICK_CTRL & SYSTICK_CTRL_COUNTFLAG) == 0U)
        {{
        }}
    }}
    SYSTICK_CTRL = 0U;
}}

static void ov5640_power_on(void)
{{
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
}}

/* XCLK on PA5 = TIM2_CH1 (AF1): 64 MHz / 3 = 21.33 MHz, 50% duty */
static void ov5640_start_xclk(void)
{{
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
}}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

int ov5640_init(void)
{{
    uint32_t i;
    uint8_t id_hi = 0U, id_lo = 0U;

    sccb_init();
    ov5640_start_xclk();
    ov5640_power_on();

    /* Verify the chip ID (0x300A = 0x56, 0x300B = 0x40) */
    if (!sccb_read_reg(OV5640_ADDR, 0x300A, &id_hi) ||
        !sccb_read_reg(OV5640_ADDR, 0x300B, &id_lo) ||
        id_hi != 0x56U || id_lo != 0x40U)
    {{
        return 0;
    }}

    /* Write the full init sequence */
    for (i = 0U; i < (sizeof(s_ov5640_init) / sizeof(s_ov5640_init[0])); i++)
    {{
        if (!sccb_write_reg(OV5640_ADDR, s_ov5640_init[i][0], (uint8_t)s_ov5640_init[i][1]))
        {{
            return 0;
        }}
    }}
    return 1;
}}

int ov5640_start_stream(void)
{{
    return sccb_write_reg(OV5640_ADDR, 0x3008, 0x02);
}}

int ov5640_stop_stream(void)
{{
    return sccb_write_reg(OV5640_ADDR, 0x3008, 0x42);
}}

int ov5640_read_reg(uint16_t reg, uint8_t *out)
{{
    return sccb_read_reg(OV5640_ADDR, reg, out);
}}
"""
    out_path = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                            "..", "Src", "ov5640.c")
    with open(out_path, "w", encoding="utf-8") as f:
        f.write(c)
    print(f"wrote {os.path.abspath(out_path)} ({len(entries)} entries)")


if __name__ == "__main__":
    main()
