# STM32H743 嵌入式视觉处理系统

![License](https://img.shields.io/badge/license-MIT-blue.svg)
![MCU](https://img.shields.io/badge/MCU-STM32H743VGT6-03234B)
![Language](https://img.shields.io/badge/language-C99-555555)
![Bare-metal](https://img.shields.io/badge/bare--metal-no_HAL%2FRTOS-orange)

基于 **STM32H743VGT6**（Cortex-M7）的裸机嵌入式视觉系统：
**OV5640 摄像头 → DCMI+DMA 采集 → 实时特征点检测（FAST/BRIEF）→ ST7789 TFT 显示**。

全部驱动与算法均为**寄存器级裸机实现**（无 HAL、无 CMSIS、无 RTOS、无第三方库），
代码量小、结构清晰、易于阅读，是嵌入式视觉与图像算法的完整参考工程。

## ✨ 项目亮点

| 方向 | 内容 |
| ---- | ---- |
| **裸机驱动** | GPIO / SPI / I2C(SCCB) / DCMI / DMA / TIM / SysTick 全部寄存器级实现 |
| **摄像头** | OV5640 完整初始化（246 寄存器，取自 ST 官方驱动）、SCCB 位操作通信、RGB565 QVGA 输出 |
| **图像采集** | DCMI 硬件同步 + DMA1 单帧快照，32 位突发搬运，帧缓冲 4 字节对齐 |
| **视觉算法** | 全定点、无浮点、无堆：FAST-9 角点 + NMS + BRIEF 描述子 + 暴力匹配（Lowe 比率检验） |
| **显示** | ST7789 320×240，bit-bang SPI，完整初始化序列（MADCTL/COLMOD/RAMCTRL/伽马/反色） |
| **工程规范** | Python 1:1 镜像验证算法正确性、自动生成寄存器表、CMake 构建、Git 版本管理 |

## 系统框图

```
                 ┌─────────────────────────────────────────────────┐
  OV5640         │  STM32H743VGT6  (Cortex-M7 @ 64 MHz HSI)        │
  摄像头 ──DVP──▶│  DCMI ──DMA1──▶ SRAM 帧缓冲 (320x240 RGB565)     │
  (RGB565,       │       │                                          │
   QVGA 320x240) │       ▼                                          │
                 │  CV 流水线: 灰度 → 3×3 平滑 → FAST-9 角点 → 叠加   │
                 │       │                                          │
                 │       ▼                                          │
                 │  ST7789 驱动 (bit-bang SPI) ──▶ 320x240 TFT 显示  │
                 └─────────────────────────────────────────────────┘
```

## 视觉算法流水线

```
RGB565 → 灰度(BT.601 定点) → 3×3 均值平滑 → FAST-9 角点 + 非极大值抑制
      → BRIEF 描述子(256 bit) → 暴力匹配 + Lowe 比率检验 → 叠加绘制
```

- 全定点运算，无浮点、无动态内存分配
- 算法经 `tools/cv_reference.py`（1:1 Python 镜像）在 PC 上验证：
  363/363 特征匹配与已知位移一致

## 目录结构

```
Inc/
  st7789.h    LCD 驱动 API
  sccb.h      SCCB（I2C 位操作）API
  ov5640.h    OV5640 摄像头 API
  dcmi.h      DCMI 采集 API
  cv.h        视觉算法库 API（FAST/BRIEF/匹配/绘制）
Src/
  main.c      系统入口：初始化 → 采集 → 处理 → 显示 循环
  st7789.c    ST7789 裸机驱动（bit-bang SPI）
  sccb.c      SCCB 位操作驱动
  ov5640.c    OV5640 驱动（含初始化寄存器表）
  dcmi.c      DCMI + DMA 采集驱动
  cv.c        视觉算法库（全定点）
  startup_stm32h743xx.S  启动文件（ST 生成）
cmake/        构建配置
tools/
  convert_image.py          图片 → RGB565 C 数组
  cv_reference.py           CV 算法 Python 镜像验证
  extract_ov5640_table.py   从 ST 官方驱动提取寄存器表
  gen_ov5640_driver.py      生成 ov5640.c
```

## 硬件要求

| 部件 | 型号 |
| ---- | ---- |
| MCU | STM32H743VGT6（单核 Cortex-M7，64 MHz HSI） |
| 摄像头 | OV5640（SCCB 控制，DVP 8 位并口，RGB565 QVGA） |
| 显示屏 | ST7789 320×240（4 线 SPI） |

## 接线

**LCD（bit-bang SPI）**

| 屏幕引脚 | STM32 |
| -------- | ----- |
| SCL | PB13 |
| SDA | PA7 |
| CS | PB12 |
| DC | PB14 |
| RES | PA0 |
| BL | PA1 |

**摄像头 OV5640（板载 FPC）**

| 功能 | STM32 |
| ---- | ----- |
| SCCB_SCL / SDA | PB8 / PB9 |
| PWDN / RESET | PD14 / PC4 |
| XCLK | PA5（TIM2 输出 21.3 MHz） |
| HSYNC / VSYNC / PIXCLK | PA4 / PB7 / PA6 |
| D0–D7 | PC6 PC7 PE0 PE1 PE4 PD3 PE5 PE6 |

## 构建 & 烧录

VS Code + STM32CubeIDE for VS Code 扩展（自带 arm-none-eabi-gcc）：

```bat
cmake --preset Debug
cmake --build build/Debug
```

用 STM32CubeProgrammer 或 ST-Link 烧录 `build/Debug/Visual-Processing.elf`。

## 运行效果

上电后：
1. 摄像头初始化（SCCB 写 246 个寄存器，约 2 s）
2. 采集循环：DCMI 快照一帧 → FAST-9 检测角点 → 红色十字叠加 → 推送屏幕
   （bit-bang 刷新约 2~3 s/帧）
3. 摄像头未检测到时屏幕显示纯红

## 性能与优化方向

- 64 MHz HSI 下单帧 CV 处理约 **250 ms**
  （灰度 42 ms / 平滑 24 ms / FAST 50–80 ms / BRIEF 24 ms / 匹配 80 ms），
  当前瓶颈为 bit-bang LCD 刷新
- 后续优化：PLL 升频至 480 MHz、LCD 改用 SPI2 外设（约 75 ms/帧）、
  角点滑窗优化、DMA 双缓冲流水线、双目视差测距

## License

[MIT](LICENSE)
