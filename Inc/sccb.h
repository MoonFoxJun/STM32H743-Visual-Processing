/**
 * @file sccb.h
 * @brief SCCB (Serial Camera Control Bus) - bit-bang I2C for the OV5640.
 *
 * OV5640 specifics:
 *   - 8-bit slave address 0x78 (write) / 0x79 (read), 7-bit = 0x3C
 *   - 16-bit register address, 8-bit data
 *
 * Wiring (fixed by the board's camera FPC):
 *   SCCB_SDA -> PB9
 *   SCCB_SCL -> PB8
 */
#ifndef SCCB_H
#define SCCB_H

#include <stdint.h>

/**
 * @brief Configure PB8/PB9 as open-drain I2C lines (with pull-ups).
 */
void sccb_init(void);

/**
 * @brief Write one byte to a 16-bit register.
 * @param dev_addr 8-bit slave address (0x78 for OV5640)
 * @param reg      16-bit register address
 * @param val      data byte
 * @return 1 on success (all ACKs), 0 on NACK
 */
int sccb_write_reg(uint8_t dev_addr, uint16_t reg, uint8_t val);

/**
 * @brief Read one byte from a 16-bit register.
 * @param dev_addr 8-bit slave address (0x78 for OV5640)
 * @param reg      16-bit register address
 * @param out      receives the byte
 * @return 1 on success, 0 on NACK/timeout
 */
int sccb_read_reg(uint8_t dev_addr, uint16_t reg, uint8_t *out);

#endif /* SCCB_H */
