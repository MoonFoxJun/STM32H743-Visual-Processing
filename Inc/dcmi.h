/**
 * @file dcmi.h
 * @brief DCMI camera interface driver (STM32H743, bare-metal).
 *
 * Captures one QVGA RGB565 frame (320x240, 153600 bytes) from the OV5640
 * into a caller-provided buffer, using DMA1 Stream0 + DMAMUX1 request 75.
 *
 * DCMI pins (board FPC, fixed, all AF13):
 *   HSYNC=PA4  VSYNC=PB7  PIXCLK=PA6  D0-D7 = PC6 PC7 PE0 PE1 PE4 PD3 PE5 PE6
 */
#ifndef DCMI_H
#define DCMI_H

#include <stdint.h>

#define DCMI_WIDTH      320U
#define DCMI_HEIGHT     240U
#define DCMI_FRAME_BYTES (DCMI_WIDTH * DCMI_HEIGHT * 2U)

/**
 * @brief Configure DCMI GPIOs, clocks, DCMI and the DMA (idempotent).
 */
void dcmi_init(void);

/**
 * @brief Capture one snapshot frame into the given buffer.
 * @param fb pointer to a DCMI_FRAME_BYTES-sized buffer, 4-byte aligned
 * @return 1 on success, 0 on timeout
 */
int dcmi_capture_frame(uint32_t *fb);

#endif /* DCMI_H */
