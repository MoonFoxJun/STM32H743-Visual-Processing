/**
 * @file ov5640.h
 * @brief OV5640 camera driver (SCCB control + register init).
 *
 * Wiring (board camera FPC, fixed):
 *   SCCB_SDA -> PB9      PWDN -> PD14 (low = powered on)
 *   SCCB_SCL -> PB8      RESET-> PC4  (active-low pulse)
 *   XCLK     -> PA5      (TIM2_CH1, ~21 MHz)
 *
 * Sensor output: RGB565, QVGA 320x240, 8-bit DVP bus to the DCMI.
 */
#ifndef OV5640_H
#define OV5640_H

#include <stdint.h>

#define OV5640_WIDTH    320U
#define OV5640_HEIGHT   240U
#define OV5640_FRAME_BYTES (OV5640_WIDTH * OV5640_HEIGHT * 2U)  /* RGB565 */

/**
 * @brief Power on the sensor and write the full initialization sequence.
 * @return 1 on success, 0 on failure
 *
 * On failure, ov5640_diag says why:
 *   1 = SCCB no response reading the chip ID (check wiring/power/XCLK)
 *   2 = SCCB OK but ID mismatch (wrong sensor or SCCB address)
 *   3 = register write NACK during init
 */
int ov5640_init(void);

/** Failure stage of the last ov5640_init() call (0 = success). */
extern uint8_t ov5640_diag;

/** Chip ID bytes read from 0x300A/0x300B (for diagnostics). */
extern uint8_t ov5640_id_hi;
extern uint8_t ov5640_id_lo;

/**
 * @brief Start the video stream (0x3008 = 0x02).
 * @return 1 on success, 0 on failure
 */
int ov5640_start_stream(void);

/**
 * @brief Stop the video stream (0x3008 = 0x42).
 * @return 1 on success, 0 on failure
 */
int ov5640_stop_stream(void);

/**
 * @brief Read a sensor register (used for diagnostics).
 * @return 1 on success, 0 on failure
 */
int ov5640_read_reg(uint16_t reg, uint8_t *out);

#endif /* OV5640_H */
