/**
 ******************************************************************************
 * @file    main.c
 * @brief   STM32H743 embedded vision demo:
 *
 *   OV5640 camera --DCMI/DMA--> framebuffer --CV--> ST7789 LCD
 *
 *   - ov5640_init():    power-on + SCCB register init (RGB565, QVGA 320x240)
 *   - dcmi_init():      DCMI + DMA1 snapshot capture of one frame
 *   - process_frame():  FAST-9 corner detection, overlay on the frame
 *   - st7789_draw_full(): push the result to the 320x240 TFT
 *
 * If the camera is not detected the LCD shows a solid red screen.
 ******************************************************************************
 */

#include <stdint.h>

#include "cv.h"
#include "dcmi.h"
#include "ov5640.h"
#include "st7789.h"

/* Capture/display frame (320x240 RGB565 = 150 KiB), 4-byte aligned for DMA */
static uint32_t s_framebuffer[DCMI_FRAME_BYTES / 4U] __attribute__((aligned(32)));

/* CV workspaces (320x240 grayscale = 76800 bytes each) */
static uint8_t s_gray[CV_W * CV_H];
static uint8_t s_smooth[CV_W * CV_H];

static cv_keypoint_t s_keypoints[CV_MAX_CORNERS];

#define FEATURE_THRESHOLD   20U

/**
 * @brief Called by the startup code before .data/.bss are initialized.
 * The H743 runs on the 64 MHz HSI after reset; nothing to configure here.
 */
void SystemInit(void)
{
}

/* FAST corner detection + overlay on the captured frame */
static void process_frame(void)
{
    uint16_t n, i;

    cv_rgb565_to_gray((const uint8_t *)s_framebuffer, s_gray);
    cv_smooth3(s_gray, s_smooth);
    n = cv_fast9(s_smooth, FEATURE_THRESHOLD, s_keypoints, CV_MAX_CORNERS);
    for (i = 0U; i < n; i++)
    {
        cv_draw_cross((uint8_t *)s_framebuffer,
                      s_keypoints[i].x, s_keypoints[i].y, 0xF800, 3);
    }
}

int main(void)
{
    st7789_init();

    if (!ov5640_init())
    {
        st7789_fill(0xF800);        /* camera not detected */
        for (;;)
        {
        }
    }

    ov5640_start_stream();
    dcmi_init();

    for (;;)
    {
        if (dcmi_capture_frame(s_framebuffer))
        {
            process_frame();
            st7789_draw_full((const uint8_t *)s_framebuffer);
        }
    }
}
