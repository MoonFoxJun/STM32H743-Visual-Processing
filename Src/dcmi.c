/**
 * @file dcmi.c
 * @brief DCMI camera interface driver (STM32H743, bare-metal).
 *
 * Capture scheme: DCMI snapshot mode (CM=0) + one-shot DMA1 Stream0.
 *   - DCMI captures one frame between CAPTURE set and end-of-frame.
 *   - DMA moves DCMI_DR -> memory. The DCMI packs 4 x 8-bit samples per
 *     32-bit read of DR, so the DMA is configured PSIZE=MSIZE=WORD and
 *     NDTR = frame_bytes / 4 = 38400 (fits the 16-bit NDTR).
 *   - DMAMUX1 channel 0 routes request 75 (DCMI) to DMA1 Stream0.
 *
 * PIXCLK edge: ST's OV5640 examples use falling-edge capture (PCKPOL=1).
 * If the image looks shifted/garbled, flip DCMI_PCKPOL.
 */
#include "dcmi.h"

/* ------------------------------------------------------------------ */
/* Register bases (STM32H743)                                          */
/* ------------------------------------------------------------------ */
#define RCC_BASE        0x58024400UL
#define GPIOA_BASE      0x58020000UL
#define GPIOB_BASE      0x58020400UL
#define GPIOC_BASE      0x58020800UL
#define GPIOD_BASE      0x58020C00UL
#define GPIOE_BASE      0x58021000UL
#define DCMI_BASE       0x48020000UL
#define DMA1_BASE       0x40020000UL
#define DMAMUX1_BASE    0x40020800UL

#define RCC_AHB1ENR     (*(volatile uint32_t *)(RCC_BASE + 0xD8UL))
#define RCC_AHB2ENR     (*(volatile uint32_t *)(RCC_BASE + 0xDCUL))
#define RCC_AHB4ENR     (*(volatile uint32_t *)(RCC_BASE + 0xE0UL))

/* GPIO registers (MODER/OTYPER/OSPEEDR/PUPDR/BSRR/AFRL/AFRH pattern) */
#define GPIOA_MODER     (*(volatile uint32_t *)(GPIOA_BASE + 0x00UL))
#define GPIOA_AFRL      (*(volatile uint32_t *)(GPIOA_BASE + 0x20UL))
#define GPIOB_MODER     (*(volatile uint32_t *)(GPIOB_BASE + 0x00UL))
#define GPIOB_AFRL      (*(volatile uint32_t *)(GPIOB_BASE + 0x20UL))
#define GPIOC_MODER     (*(volatile uint32_t *)(GPIOC_BASE + 0x00UL))
#define GPIOC_AFRL      (*(volatile uint32_t *)(GPIOC_BASE + 0x20UL))
#define GPIOD_MODER     (*(volatile uint32_t *)(GPIOD_BASE + 0x00UL))
#define GPIOD_AFRL      (*(volatile uint32_t *)(GPIOD_BASE + 0x20UL))
#define GPIOE_MODER     (*(volatile uint32_t *)(GPIOE_BASE + 0x00UL))
#define GPIOE_AFRL      (*(volatile uint32_t *)(GPIOE_BASE + 0x20UL))
#define GPIOE_AFRH      (*(volatile uint32_t *)(GPIOE_BASE + 0x24UL))

/* DCMI registers */
#define DCMI_CR         (*(volatile uint32_t *)(DCMI_BASE + 0x00UL))
#define DCMI_SR         (*(volatile uint32_t *)(DCMI_BASE + 0x04UL))
#define DCMI_RISR       (*(volatile uint32_t *)(DCMI_BASE + 0x08UL))
#define DCMI_ICR        (*(volatile uint32_t *)(DCMI_BASE + 0x14UL))
#define DCMI_DR         (*(volatile uint32_t *)(DCMI_BASE + 0x28UL))

#define DCMI_CR_CAPTURE     (1UL << 0)
#define DCMI_CR_CM          (1UL << 1)   /* 0 = snapshot, 1 = continuous */
#define DCMI_CR_PCKPOL      (1UL << 5)   /* 1 = capture on falling edge  */
#define DCMI_CR_HSPOL       (1UL << 6)   /* 1 = HSYNC active high        */
#define DCMI_CR_VSPOL       (1UL << 7)   /* 1 = VSYNC active high        */
#define DCMI_CR_ENABLE      (1UL << 14)

#define DCMI_RISR_FRAME     (1UL << 0)
#define DCMI_RISR_OVR       (1UL << 1)
#define DCMI_RISR_ERR       (1UL << 2)
#define DCMI_ICR_FRAME_C    (1UL << 0)
#define DCMI_ICR_OVR_C      (1UL << 1)
#define DCMI_ICR_ERR_C      (1UL << 2)

/* DMA1 Stream 0 */
#define DMA1_LISR       (*(volatile uint32_t *)(DMA1_BASE + 0x00UL))
#define DMA1_IFCR       (*(volatile uint32_t *)(DMA1_BASE + 0x04UL))
#define DMA1_S0CR       (*(volatile uint32_t *)(DMA1_BASE + 0x10UL))
#define DMA1_S0NDTR     (*(volatile uint32_t *)(DMA1_BASE + 0x14UL))
#define DMA1_S0PAR      (*(volatile uint32_t *)(DMA1_BASE + 0x18UL))
#define DMA1_S0M0AR     (*(volatile uint32_t *)(DMA1_BASE + 0x1CUL))
#define DMA1_S0FCR      (*(volatile uint32_t *)(DMA1_BASE + 0x24UL))

#define DMA_SxCR_EN         (1UL << 0)
#define DMA_SxCR_TEIE       (1UL << 2)
#define DMA_SxCR_TCIE       (1UL << 4)
#define DMA_SxCR_DIR_0      (1UL << 6)   /* 01 = peripheral to memory */
#define DMA_SxCR_CIRC       (1UL << 8)
#define DMA_SxCR_PINC       (1UL << 9)
#define DMA_SxCR_MINC       (1UL << 10)
#define DMA_SxCR_PSIZE_1    (1UL << 12)  /* 10 = 32-bit */
#define DMA_SxCR_MSIZE_1    (1UL << 14)  /* 10 = 32-bit */
#define DMA_SxCR_PL_0       (1UL << 16)  /* 11 = high priority */
#define DMA_SxCR_PL_1       (1UL << 17)
#define DMA_LISR_TCIF0      (1UL << 5)
#define DMA_IFCR_CTCIF0     (1UL << 5)
#define DMA_IFCR_CGIF0      (0x3DUL << 0)

/* DMAMUX1 channel 0 request register */
#define DMAMUX1_CH0_CCR  (*(volatile uint32_t *)(DMAMUX1_BASE + 0x00UL))
#define DMAMUX_CxCR_DMAREQ_ID   (0xFFUL << 0)
#define DMA_REQUEST_DCMI        75U

#define FRAME_WORDS     (DCMI_FRAME_BYTES / 4U)   /* 38400 */

/**
 * PIXCLK sampling edge. 1 = falling (ST OV5640 examples); flip to 0 if the
 * image looks shifted/garbled.
 */
#define DCMI_PCKPOL     DCMI_CR_PCKPOL

/* ------------------------------------------------------------------ */
/* GPIO helpers                                                        */
/* ------------------------------------------------------------------ */

/* Set pin n (0-15) of a port to alternate function f (0-15) */
static void gpio_set_af(volatile uint32_t *moder, volatile uint32_t *afr,
                        uint32_t pin, uint32_t af)
{
    uint32_t shift = pin * 2U;
    *moder = (*moder & ~(0x3UL << shift)) | (0x2UL << shift);
    if (pin < 8U)
    {
        *afr = (*afr & ~(0xFUL << (pin * 4U))) | (af << (pin * 4U));
    }
    else
    {
        volatile uint32_t *afrh = afr + 1U;
        pin -= 8U;
        *afrh = (*afrh & ~(0xFUL << (pin * 4U))) | (af << (pin * 4U));
    }
}

/* ------------------------------------------------------------------ */
/* Public API                                                          */
/* ------------------------------------------------------------------ */

void dcmi_init(void)
{
    /* 1. Clocks: GPIOA-E, DMA1, DCMI */
    RCC_AHB4ENR |= (1UL << 0) | (1UL << 1) | (1UL << 2) | (1UL << 3) | (1UL << 4);
    RCC_AHB1ENR |= (1UL << 0);              /* DMA1EN   */
    RCC_AHB2ENR |= (1UL << 0);              /* DCMIEN   */

    /* 2. DCMI pins, all AF13:
     *    PA4 HSYNC, PA6 PIXCLK | PB7 VSYNC
     *    PC6 D0, PC7 D1 | PE0 D2, PE1 D3, PE4 D4, PE5 D6, PE6 D7 | PD3 D5
     */
    gpio_set_af(&GPIOA_MODER, &GPIOA_AFRL, 4U, 13U);
    gpio_set_af(&GPIOA_MODER, &GPIOA_AFRL, 6U, 13U);
    gpio_set_af(&GPIOB_MODER, &GPIOB_AFRL, 7U, 13U);
    gpio_set_af(&GPIOC_MODER, &GPIOC_AFRL, 6U, 13U);
    gpio_set_af(&GPIOC_MODER, &GPIOC_AFRL, 7U, 13U);
    gpio_set_af(&GPIOD_MODER, &GPIOD_AFRL, 3U, 13U);
    gpio_set_af(&GPIOE_MODER, &GPIOE_AFRL, 0U, 13U);
    gpio_set_af(&GPIOE_MODER, &GPIOE_AFRL, 1U, 13U);
    gpio_set_af(&GPIOE_MODER, &GPIOE_AFRL, 4U, 13U);
    gpio_set_af(&GPIOE_MODER, &GPIOE_AFRL, 5U, 13U);
    gpio_set_af(&GPIOE_MODER, &GPIOE_AFRL, 6U, 13U);

    /* 3. DCMI: 8-bit (EDM=00), snapshot (CM=0), full frame (FCRC=00),
     *    hardware sync, polarities per OV5640.
     */
    DCMI_CR = DCMI_CR_ENABLE | DCMI_PCKPOL | DCMI_CR_HSPOL | DCMI_CR_VSPOL;

    /* 4. DMA1 Stream0 -> DCMI_DR, 32-bit, periph-to-memory, direct mode */
    DMA1_S0PAR  = (uint32_t)&DCMI_DR;
    DMA1_S0CR   = DMA_SxCR_DIR_0 | DMA_SxCR_MINC | DMA_SxCR_PSIZE_1 |
                  DMA_SxCR_MSIZE_1 | DMA_SxCR_PL_0 | DMA_SxCR_PL_1 |
                  DMA_SxCR_TCIE | DMA_SxCR_TEIE;
    DMA1_S0FCR  = 0U;                       /* direct mode (FIFO disabled) */

    /* 5. DMAMUX1 channel 0 = DCMI request (75) */
    DMAMUX1_CH0_CCR = (DMA_REQUEST_DCMI & DMAMUX_CxCR_DMAREQ_ID);
}

int dcmi_capture_frame(uint32_t *fb)
{
    uint32_t timeout;

    /* Disarm previous capture */
    DCMI_CR &= ~DCMI_CR_CAPTURE;
    DMA1_S0CR &= ~DMA_SxCR_EN;
    DMA1_IFCR = DMA_IFCR_CGIF0;             /* clear all stream0 flags */
    DCMI_ICR  = DCMI_ICR_FRAME_C | DCMI_ICR_OVR_C | DCMI_ICR_ERR_C;

    /* Arm DMA + capture */
    DMA1_S0NDTR = FRAME_WORDS;
    DMA1_S0M0AR = (uint32_t)fb;
    DMA1_S0CR |= DMA_SxCR_EN;
    DCMI_CR |= DCMI_CR_CAPTURE;

    /* Wait for DMA transfer complete (frame ~10-30 ms @ QVGA) */
    timeout = 10000000UL;                   /* ~0.5 s at 64 MHz */
    while ((DMA1_LISR & DMA_LISR_TCIF0) == 0U)
    {
        if (--timeout == 0U)
        {
            DMA1_S0CR &= ~DMA_SxCR_EN;
            DCMI_CR &= ~DCMI_CR_CAPTURE;
            return 0;
        }
    }

    /* Stop */
    DMA1_S0CR &= ~DMA_SxCR_EN;
    DCMI_CR &= ~DCMI_CR_CAPTURE;
    DMA1_IFCR = DMA_IFCR_CTCIF0;
    return 1;
}
