import time, os, sys

from media.sensor import * #导入sensor模块，使用摄像头相关接口
from media.display import * #导入display模块，使用display相关接口
from media.media import * #导入media模块，使用meida相关接口
from machine import Pin, FPIOA, UART

THRESHOLD = (0, 100)  # 黑白图像的灰度阈值
BINARY_VISIBLE = True # 使用二值化图像你可以看到什么是线性回归。
                        # 这可能降低 FPS（每秒帧数）.

sensor = Sensor(width=1280, height=960) #构建摄像头对象，将摄像头长宽设置为4:3
sensor.reset() #复位和初始化摄像头
sensor.set_framesize(width=640, height=480) #设置帧大小，默认通道0
sensor.set_pixformat(Sensor.GRAYSCALE) #设置输出图像格式，默认通道0

Display.init(Display.ST7701, to_ide=True) #同时使用3.5寸mipi屏和IDE缓冲区显示图像，800x480分辨率
#Display.init(Display.VIRT, sensor.width(), sensor.height()) #只使用IDE缓冲区显示图像

MediaManager.init() #初始化media资源管理器

sensor.run() #启动sensor

clock = time.clock()
#初始化串口
# 初始化FPIOA（现场可编程I/O阵列）
fpioa = FPIOA()
fpioa.set_function(3, FPIOA.UART1_TXD)
fpioa.set_function(4, FPIOA.UART1_RXD)
uart = UART(UART.UART1, 9600)
while True:

    ################
    ## 这里编写代码 ##
    ################
    clock.tick()

    #image.binary([THRESHOLD])将灰度值在THRESHOLD范围变成了白色
    img = sensor.snapshot().binary([THRESHOLD]) if BINARY_VISIBLE else sensor.snapshot()

    # 返回一个类似 find_lines() 和find_line_segments()的对象.
    # 有以下函数使用方法： x1(), y1(), x2(), y2(), length(),
    # theta() (rotation in degrees), rho(), and magnitude().
    #
    # magnitude() 代表线性回归的指令，其值为(0, INF]。
    # 0表示一个圆，INF数值越大，表示线性拟合的效果越好。

    line = img.get_regression([(255,255) if BINARY_VISIBLE else THRESHOLD])
    line_error = 0  # 默认值：没有线就直行/停止

    if (line):

        img.draw_line(line.line(), color = 127,thickness=4)
        line_error = line.theta()  # 🔥 【唯一要传出的巡线参数】
        if line_error>90:
            line_error = 180-line_error
        elif line_error<90:
            line_error= 0-line_error
        print("传给PID的误差 =%03d" %line_error)
        uart.write("%03dE" % line_error)
    #显示图片，仅用于LCD居中方式显示
    Display.show_image(img, x=round((800-sensor.width())/2),y=round((480-sensor.height())/2))

#    print("FPS %f, mag = %s" % (clock.fps(), str(line.magnitude()) if (l80ine) else "N/A"))
