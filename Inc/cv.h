/**
 * @file cv.h
 * @brief Lightweight computer vision library for the STM32H743.
 *
 * Classic feature-matching pipeline, sized for 320x240 QVGA:
 *   RGB565 -> grayscale -> 3x3 smooth -> FAST-9 corner detection
 *   -> NMS -> BRIEF descriptors (256 bit) -> brute-force Hamming match
 *   with Lowe's ratio test.
 *
 * All fixed-point, no float, no heap. The implementation is mirrored 1:1 by
 * tools/cv_reference.py (used to validate the algorithm on a PC).
 */
#ifndef CV_H
#define CV_H

#include <stdint.h>

#define CV_W               320U
#define CV_H               240U
#define CV_MAX_CORNERS     400U

/** Corner/feature keypoint */
typedef struct
{
    uint16_t x;
    uint16_t y;
    uint16_t score;
} cv_keypoint_t;

/** BRIEF descriptor: 256 bits = 8 x uint32 */
#define CV_BRIEF_WORDS     8U

/**
 * @brief RGB565 (MSB-first bytes, as stored for the ST7789) -> 8-bit gray.
 * @param rgb565 320*240*2 bytes
 * @param gray   320*240 bytes
 */
void cv_rgb565_to_gray(const uint8_t *rgb565, uint8_t *gray);

/**
 * @brief 3x3 box blur (sum/8, fixed point).
 * @param src 320*240 bytes, @param dst 320*240 bytes (may be src)
 */
void cv_smooth3(const uint8_t *src, uint8_t *dst);

/**
 * @brief FAST-9 corner detection with non-maximum suppression.
 * @param gray      320*240 grayscale
 * @param threshold intensity threshold (e.g. 20)
 * @param kp        output keypoints (x, y, FAST score)
 * @param max       capacity of kp
 * @return number of corners found
 */
uint16_t cv_fast9(const uint8_t *gray, uint8_t threshold,
                  cv_keypoint_t *kp, uint16_t max);

/**
 * @brief BRIEF descriptors (256 bit) for the keypoints.
 * @param smooth 3x3-smoothed grayscale (BRIEF is computed on the smoothed
 *               image for robustness)
 * @param kp     keypoints
 * @param n      number of keypoints
 * @param desc   n * CV_BRIEF_WORDS uint32s
 */
void cv_brief(const uint8_t *smooth, const cv_keypoint_t *kp, uint16_t n,
              uint32_t *desc);

/**
 * @brief Brute-force matching with Lowe's ratio test.
 * @return number of accepted matches; matches_a[i]/matches_b[i] hold the
 *         matched keypoint indices (parallel arrays).
 */
uint16_t cv_match_bf(const uint32_t *desc_a, uint16_t na,
                     const uint32_t *desc_b, uint16_t nb,
                     uint16_t *matches_a, uint16_t *matches_b);

/** @brief Hamming distance between two 256-bit descriptors. */
uint16_t cv_desc_distance(const uint32_t *a, const uint32_t *b);

/**
 * @brief Shift a grayscale image by (dx, dy), zero-filling the borders
 *        (dst[x,y] = src[x-dx, y-dy]).
 */
void cv_shift_gray(const uint8_t *src, uint8_t *dst, int16_t dx, int16_t dy);

/** @brief Draw a cross marker on an RGB565 frame (MSB-first bytes). */
void cv_draw_cross(uint8_t *rgb565, uint16_t x, uint16_t y,
                   uint16_t color, uint8_t r);

/** @brief Draw a line on an RGB565 frame (Bresenham). */
void cv_draw_line(uint8_t *rgb565, uint16_t x0, uint16_t y0,
                  uint16_t x1, uint16_t y1, uint16_t color);

#endif /* CV_H */
