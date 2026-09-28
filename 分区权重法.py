# 导入必要的库
import time, os, gc, sys, math
import utime
from media.sensor import *
from media.display import *
from media.media import *
from machine import Pin, FPIOA, UART

# 全局常量配置部分
# 控制是否启用LCD显示屏
LCD_ENABLED = True
# 检测图像的宽度
DETECT_WIDTH = 800
# 检测图像的高度
DETECT_HEIGHT = 480
# 学习颜色阈值时的感兴趣区域，格式为 (x, y, width, height)
LEARNING_ROI = (0, 231, 800, 50)
# 按键所连接的引脚编号
KEY_PIN = 53
# 学习颜色阈值时收集的帧数
LEARNING_FRAME_COUNT = 30
# 巡线检测时将图像水平分割的区域数量,分为8个小块
LINE_SECTION_COUNT = 8
# 巡线检测起始行的比例
START_Y_RATIO = 3 / 8
# 巡线检测结束行的比例
END_Y_RATIO = 5 / 8
# 检测色块时的像素数量阈值
BLOB_PIXELS_THRESHOLD = 1006
# 检测色块时的面积阈值
BLOB_AREA_THRESHOLD = 100
# 8个区域权重值
LINE_WEIGHTS = [-4.0, -3.0, -2.0, -1.0, 1.0, 2.0, 3.0, 4.0]  # 8个区域的权重值

def calculate_line_error(detection_result):
    """
    计算巡线误差（基于权重）
    :param detection_result: 巡线检测结果列表，如 ['0','0','1','1','1','0','0','0']
    :return: 巡线误差值（范围大约在-4.0到4.0之间）
    """
    weighted_sum = 0.0  # 权重总和
    line_count = 0      # 检测到线的区域数量

    # 遍历8个区域
    for i in range(LINE_SECTION_COUNT):
        if detection_result[i] == '1':  # 如果该区域检测到线
            weighted_sum += LINE_WEIGHTS[i]
            line_count += 1

    # 计算加权平均误差（避免除以0）
    if line_count > 0:
        line_error = weighted_sum / line_count
    else:
        line_error = 0.0  # 没有检测到线时误差为0

    return line_error

def init_sensor_and_key():
    """
    初始化摄像头和按键
    :return: 初始化后的摄像头对象和按键对象
    """
    # 初始化FPIOA（现场可编程I/O阵列）
    fpioa = FPIOA()
    # 将指定的引脚配置为GPIO21功能
    fpioa.set_function(KEY_PIN, FPIOA.GPIO53)

    # 初始化按键，设置为输入模式，并启下拉电阻
    key = Pin(KEY_PIN, Pin.IN, Pin.PULL_DOWN)

    #初始化串口
    fpioa.set_function(3, FPIOA.UART1_TXD)
    fpioa.set_function(4, FPIOA.UART1_RXD)
    uart = UART(UART.UART1, 9600)
    # 初始化摄像头，设置图像的宽度和高度
    sensor = Sensor(width=DETECT_WIDTH, height=DETECT_HEIGHT)
    # 重置摄像头
    sensor.reset()
    # 设置摄像头的帧大小
    sensor.set_framesize(width=DETECT_WIDTH, height=DETECT_HEIGHT)
    # 设置摄像头的像素格式为RGB565
    sensor.set_pixformat(Sensor.RGB565)

    # 根据LCD_ENABLED的值选择不同的显示初始化方式
    if LCD_ENABLED:
        # 通过01Studio 3.5寸mipi显示屏显示图像
        Display.init(Display.ST7701, to_ide=True)
    else:
        # 只使用IDE缓冲区显示图像
        Display.init(Display.VIRT, sensor.width(), sensor.height())

    return sensor, key, uart

def learn_color_threshold(sensor):
    """
    学习颜色阈值
    :param sensor: 摄像头对象
    :return: 学习得到的LAB空间颜色阈值
    """
    print("开始学习颜色阈值...")
    # 初始化LAB空间的颜色阈值
    threshold = [50, 50, 0, 0, 0, 0]

    # 收集指定帧数的颜色数据
    for _ in range(LEARNING_FRAME_COUNT):
        # 拍摄一帧图像
        img = sensor.snapshot()
        # 在图像上绘制学习区域的矩形框
        img.draw_rectangle(LEARNING_ROI, color=(0, 255, 0), thickness=2)
        # 显示图像
        Display.show_image(img)

        # 获取学习区域的直方图
        hist = img.get_histogram(roi=LEARNING_ROI)
        # 获取直方图中1%分位数的颜色值
        lo = hist.get_percentile(0.1)
        # 获取直方图中99%分位数的颜色值
        hi = hist.get_percentile(0.9)

        # 更新颜色阈值
        threshold[0] = (threshold[0] + lo.l_value()) // 2
        threshold[1] = (threshold[1] + hi.l_value()) // 2
        threshold[2] = (threshold[2] + lo.a_value()) // 2
        threshold[3] = (threshold[3] + hi.a_value()) // 2
        threshold[4] = (threshold[4] + lo.b_value()) // 2
        threshold[5] = (threshold[5] + hi.b_value()) // 2

    print("颜色阈值学习完成:", threshold)
    return threshold

def line_detection(img, threshold):
    """
    巡线检测
    :param img: 待检测的图像
    :param threshold: 颜色阈值
    :return: 巡线检测结果列表和路口类型
    """
    # 获取图像的宽度和高度
    width = img.width()
    height = img.height()

    # 计算巡线检测的起始行和结束行,巡线区域上下线高度获取
    start_y = int(height * START_Y_RATIO)
    end_y = int(height * END_Y_RATIO)
    # 计算每个分割区域的宽度,分为8份
    section_width = width // LINE_SECTION_COUNT

    # 初始化巡线检测结果列表,定义一个8字节数组,用来储存循迹反馈结果
    detection_result = ['0'] * LINE_SECTION_COUNT

    # 遍历每个分割区域
    for i in range(LINE_SECTION_COUNT):
        # 定义当前区域的感兴趣区域
        roi = (i * section_width, start_y, section_width, end_y - start_y)
        # 在图像上绘制当前区域的矩形框
        img.draw_rectangle(roi, color=(0, 255, 0), thickness=2)

        # 在当前区域内查找符合颜色阈值的色块
        blobs = img.find_blobs([threshold], roi=roi, pixels_threshold=BLOB_PIXELS_THRESHOLD,
                               area_threshold=BLOB_AREA_THRESHOLD, merge=True)

        # 如果找到色块
        if blobs:
            # 找到最大的色块
            largest_blob = max(blobs, key=lambda b: b.pixels())
            # 在最大色块的中心绘制一个圆圈
            img.draw_circle(largest_blob.cx(), largest_blob.cy(), 10, color=(0, 255, 0), thickness=2)
            # 更新检测结果列表
            detection_result[i] = '1'

    # 判断是否需要左转
    left_turn = ('1' in detection_result[3:4]) and (detection_result[0] == '1' or detection_result[1] == '1')
    # 判断是否需要右转
    right_turn = ('1' in detection_result[3:4]) and (detection_result[6] == '1' or detection_result[7] == '1')

    # 初始化路口类型
    intersection_type = None
    # 判断是否为T字路口
    if left_turn and right_turn:
        intersection_type = "T字路口"
    # 判断是否为左转路口
    elif left_turn:
        intersection_type = "左转路口"
    # 判断是否为右转路口
    elif right_turn:
        intersection_type = "右转路口"

    return detection_result, intersection_type

def print_status(img, line_pattern_list=None, intersection_type=None):
    """
    在图像上显示状态信息
    :param img: 图像对象
    :param line_pattern_list: 巡线检测结果列表
    :param intersection_type: 路口类型
    """
    # 初始化状态信息字符串
    status = "状态: "
    # 如果有巡线检测结果
    if line_pattern_list:
        # 将巡线检测结果列表转换为字符串
        line_pattern = ''.join(line_pattern_list)
        # 更新状态信息字符串
        status += f"巡线[{line_pattern}] "
    # 如果有路口类型信息
    if intersection_type:
        # 更新状态信息字符串
        status += f"路口类型[{intersection_type}]"
    # 打印状态信息
    print(status)

def main():
    """
    主函数，程序的入口点
    """
    # 初始化传感器和按键
    sensor, key, uart = init_sensor_and_key()
    # 初始化媒体管理器
    MediaManager.init()
    # 启动摄像头
    sensor.run()

    # 初始化颜色阈值
    threshold = None
    # 初始化巡线启用标志
    line_following_enabled = False

    while True:
        # 拍摄一帧图像
        img = sensor.snapshot()

        # 检测按键是否被按下
        if key.value() == 1:
            # 消抖处理
            time.sleep_ms(10)
            if key.value() == 0:
                # 学习颜色阈值
                threshold = learn_color_threshold(sensor)
                # 启用巡线功能
                line_following_enabled = True
                # 等待按键释放
                while not key.value():
                    pass
                continue

        # 如果已经学习到颜色阈值并且巡线功能已启用
        if threshold and line_following_enabled:
            # 进行巡线检测
            line_pattern_list, intersection_type = line_detection(img, threshold)
            # 打印状态信息
#            print_status(img, line_pattern_list, intersection_type)

            #计算巡线误差
            line_error = calculate_line_error(line_pattern_list)
#            print(f"巡线误差: {line_error:.2f}")  # 打印误差值，保留2位小数
            # 将误差乘以100并转为整数
            error_value = int(line_error * 20)

            # 通过串口1发送误差数据
            # 格式："E123\r\n"（示例，误差1.23时）
            print("Turn Angle: %03d" % error_value)  # 打印误差值，保留2位小数
            uart.write("%03dE"%error_value)
        else:
            # 在图像上绘制学习区域的矩形框
            img.draw_rectangle(LEARNING_ROI, color=(255, 0, 0), thickness=2)
            # 在图像上显示提示信息
            img.draw_string(10, 10, "按键开始学习", color=(255, 0, 0), scale=2)

        # 显示图像
        Display.show_image(img)

if __name__ == "__main__":
    # 调用主函数
    main()
