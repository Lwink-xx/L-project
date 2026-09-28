import sensor, image, time # 导入 sensor（摄像头控制）、image（图像处理）、time（延时/计时）模块
import math                # 导入数学函数库，用于三角函数计算
from pyb import UART
import pyb                 # 从 pyb（板级支持库）导入 UART 串口类，并导入整个 pyb 模块（用于延时等

# 矩形识别阶段摄像头参数设置
def setup_rectangle_detection():        # 设置摄像头参数用于矩形检测阶段
    sensor.set_pixformat(sensor.RGB565) # 设置像素格式为 RGB565（彩色，每个像素16位
    sensor.set_framesize(sensor.QQVGA)  # 使用QQVGA分辨率进行矩形识别，分辨率较低，提高处理速度
    sensor.skip_frames(time=2000)       # 跳过前 2000 毫秒（2秒）的图像帧，让摄像头自动调整曝光、白平衡等稳定下来
    sensor.set_hmirror(True)            # 设置水平镜像
    sensor.set_vflip(True)              # 设置垂直翻转

# 激光识别阶段摄像头参数设置
def setup_laser_detection():            # 设置摄像头参数用于激光（色块）检测阶段
    sensor.reset()                      # 完全重置摄像头传感器，清除之前所有设置
    sensor.set_pixformat(sensor.RGB565) # 同样设为 RGB565 彩色格式
    sensor.set_framesize(sensor.QVGA)   # 设置为 QVGA 分辨率（320×240），比 QQVGA 大一倍，便于定位光斑细节
    sensor.set_brightness(-3)           # 设置亮度亮度减 3（范围 -3~+3），使图像整体偏暗，有利于突出亮色光斑
    sensor.set_contrast(3)              # 对比度，对比度加 3，增强颜色差异
    sensor.set_auto_gain(False)         # 关闭自动增益（AGC），避免环境变化时亮度自动跳变
    sensor.set_auto_whitebal(False)     # 关闭自动白平衡，保持颜色稳定
    sensor.set_auto_exposure(False, exposure_us = int(8000)) # 关闭自动曝光，并手动设置曝光时间为 8000 微秒（8 毫秒），减少光斑拖影
    sensor.set_hmirror(True)            # 设置水平镜像
    sensor.set_vflip(True)              # 设置垂直翻转
# 向内收缩的值，可根据实际情况调整
shrink_value = 2                        # 定义收缩值（像素）：检测到的矩形向内缩进 2 个像素，得到一个内框

# 定义红色的颜色阈值
# 这里的阈值可能需要根据实际情况进行调整
red_threshold = (20, 79, -1, 58, -22, 24)

# 定义绿色的颜色阈值
# 这里的阈值可能需要根据实际情况进行调整
#green_threshold = (8, 99, -48, 82, -17, 97)
green_threshold = (30, 100, -64, -22, -16, 40)

# 合并红色和绿色的阈值
#thresholds = [red_threshold,red_threshold1,green_threshold]

# 最大循环次数
MAX_LOOPS = 10                  # 最大稳定循环次数：矩形角点需要连续稳定 10 帧才认为是可靠的
# 角点变化的最大允许差值
MAX_DIFFERENCE = 5              # 角点坐标变化的最大允许差值（像素），超过此值认为不稳定
# 记录稳定的原始角点
stable_original_corners = None  # 存储稳定后的原始矩形四个角点（元组列表）
# 记录稳定的缩小后角点
stable_shrunk_corners = None    # 存储稳定后的收缩内框四个角点
# 连续稳定的次数
stable_count = 0                # 当前已连续稳定的帧数计数器
# 标记是否开始激光识别
start_laser_detection = False   # 是否已经切换到激光检测阶段（开始检测红/绿色块）
# 标记是否已找到矩形
rectangle_found = False         # 是否已经成功找到并稳定了矩形（用于控制循环分支）
# 内框点位发送
inrectangle_send = False        # 内框坐标是否已经发送给上位机（只发送一次的标志）

usart3 = UART(3,115200)         # 使用串口3
usart3.init(115200, bits=8, parity=None, stop=1)  # 初始化串口3

# 初始化摄像头为矩形识别参数
sensor.reset()                  # 在进入主循环前先重置摄像头（采用矩形检测阶段的参数）
setup_rectangle_detection()     # 调用前面定义的函数，配置为矩形检测模式

clock = time.clock()            # 创建一个时钟对象，用于计算帧率（FPS）

while True:
    clock.tick()                # 开始计时一帧的开始
    img = sensor.snapshot()     # 拍摄一帧图像，返回 image 对象
    # img = img.gaussian(1)     # 高斯模糊（核大小为1），可能用于降噪，此处未启用
    if not rectangle_found:     # 矩形检测阶段
        # 矩形识别阶段
        # 寻找矩形
        # 在图像中寻找矩形。threshold 是矩形检测的阈值（评分），值越小越敏感，但误检也多。20545 是经验值
        rects = img.find_rects(threshold=20545) #20545
        for r in rects:         # 遍历所有找到的矩形（通常只处理第一个，代码中 break 了）
                                # 获取矩形的四个角点坐标，格式为 [(x0,y0), (x1,y1), (x2,y2), (x3,y3)]，顺序不定
            original_corners = r.corners()
            print(r)            # 在串口终端打印矩形信息（调试用）
            # 选择第一个角点作为旋转参考点
                                # 取第一个角点作为旋转参考点
            rotate_point = original_corners[0]
            # 计算旋转角度
                                # 计算矩形第一条边（角点0→角点1）与水平轴的夹角。atan2(dy, dx) 返回弧度。加负号是因为后面旋转时方向需要匹配
            theta = -math.atan2(original_corners[1][1] - original_corners[0][1], original_corners[1][0] - original_corners[0][0])
            # 旋转角点
            rotated_corners = []# 用于存储旋转后的四个角点（水平放置的矩形）
                                # 将每个角点绕 rotate_point 旋转 theta 角度。公式是二维旋转的逆时针变换（theta 为负时实际是顺时针）
            for corner in original_corners:
                x, y = corner
                rotated_x = (x - rotate_point[0]) * math.cos(theta) - (y - rotate_point[1]) * math.sin(theta) + rotate_point[0]
                rotated_y = (x - rotate_point[0]) * math.sin(theta) + (y - rotate_point[1]) * math.cos(theta) + rotate_point[1]
                rotated_corners.append((rotated_x, rotated_y))
            # 计算旋转后第一个角点和第三个角点的角度
                                # 计算旋转后第0个角点与第2个角点连线的角度。用于判断矩形的朝向（判断哪一对角是对角线）
            theta_p02 = math.atan2(rotated_corners[2][1] - rotated_corners[0][1], rotated_corners[2][0] - rotated_corners[0][0])
            # 收缩旋转后的角点
                                # 存储收缩后的角点（仍在旋转后的水平坐标系中）
            shrunk_rotated_corners = []
            if theta_p02 > 0:   # 如果对角线角度大于0，说明矩形旋转后的姿态为“左下-右上”方向（具体取决于坐标轴）。根据这个选择收缩方向
                                # 当对角线角度>0时：角点0向右下收缩，角点1向左下，角点2向左上，角点3向右上。这样得到一个内缩的矩形
                shrunk_rotated_corners.append((rotated_corners[0][0] + shrink_value, rotated_corners[0][1] + shrink_value))
                shrunk_rotated_corners.append((rotated_corners[1][0] - shrink_value, rotated_corners[1][1] + shrink_value))
                shrunk_rotated_corners.append((rotated_corners[2][0] - shrink_value, rotated_corners[2][1] - shrink_value))
                shrunk_rotated_corners.append((rotated_corners[3][0] + shrink_value, rotated_corners[3][1] - shrink_value))
            else:               # 当对角线角度≤0时，采用另一种收缩方向（基本是上下互换）
                shrunk_rotated_corners.append((rotated_corners[0][0] + shrink_value, rotated_corners[0][1] - shrink_value))
                shrunk_rotated_corners.append((rotated_corners[1][0] - shrink_value, rotated_corners[1][1] - shrink_value))
                shrunk_rotated_corners.append((rotated_corners[2][0] - shrink_value, rotated_corners[2][1] + shrink_value))
                shrunk_rotated_corners.append((rotated_corners[3][0] + shrink_value, rotated_corners[3][1] + shrink_value))

            # 反向旋转收缩后的角点
            shrunk_corners = [] # 用于存储反旋转后的收缩角点（回到原始图像坐标系）
                                # 反向旋转（角度取 -theta）把收缩后的水平矩形变回原倾斜角度，并取整作为像素坐标
            for corner in shrunk_rotated_corners:
                x, y = corner
                new_x = (x - rotate_point[0]) * math.cos(-theta) - (y - rotate_point[1]) * math.sin(-theta) + rotate_point[0]
                new_y = (x - rotate_point[0]) * math.sin(-theta) + (y - rotate_point[1]) * math.cos(-theta) + rotate_point[1]
                shrunk_corners.append((int(new_x), int(new_y)))
                                # 如果是第一帧检测到矩形，尚无稳定参考值
            if stable_original_corners is None:
                                # 保存当前角点作为参考，稳定计数设为1
                stable_original_corners = original_corners
                stable_shrunk_corners = shrunk_corners
                stable_count = 1
            else:               # 否则，比较当前帧与稳定参考值是否一致
                all_stable_original = True
                all_stable_shrunk = True
                                # 遍历4个角点，每个角点的x和y坐标分别与稳定值比较，若任一差值大于5像素，则认为不稳定
                for i in range(4):
                    for j in range(2):
                        if abs(original_corners[i][j] - stable_original_corners[i][j]) > MAX_DIFFERENCE:
                            all_stable_original = False
                        if abs(shrunk_corners[i][j] - stable_shrunk_corners[i][j]) > MAX_DIFFERENCE:
                            all_stable_shrunk = False
                    if not all_stable_original or not all_stable_shrunk:
                        break
                                # 如果当前角点与稳定参考值的差异都在允许范围内，增加稳定计数
                if all_stable_original and all_stable_shrunk:
                    stable_count += 1
                else:           # 否则（不稳定），重置参考值为当前帧，计数重新从1开始
                    stable_original_corners = original_corners
                    stable_shrunk_corners = shrunk_corners
                    stable_count = 1
                                # 如果连续10帧都稳定，则确认矩形已锁定
            if stable_count == MAX_LOOPS:
                # 角点稳定，开始激光识别
                                # 设置标志位，切换到激光检测模式，并调用函数重新配置摄像头参数
                rectangle_found = True
                start_laser_detection = True
                setup_laser_detection()
                # 将QQVGA下的坐标转换为QVGA下的坐标
                                # 因为矩形检测阶段使用的是 QQVGA（160x120），而激光检测阶段要用 QVGA（320x240），所以坐标需要乘以2，才能在新分辨率下对应相同物理位置
                stable_original_corners = [(x * 2, y * 2) for x, y in stable_original_corners]
                stable_shrunk_corners = [(x * 2, y * 2) for x, y in stable_shrunk_corners]
                                # 跳出 for r in rects 循环，不再处理后续矩形（只取第一个）
                break

    if start_laser_detection:

        # 激光识别阶段
        # 激光识别部分
                                # 查找绿色色块（符合绿色阈值的连续区域）。x_stride=1 表示逐列扫描；pixels_threshold=2 色块至少2个像素；area_threshold=2 面积至少2；merge=1 合并相邻色块
        green_blobs = img.find_blobs([green_threshold], x_stride=1, pixels_threshold=2, area_threshold=2, merge=1)
                                # 查找红色色块，像素阈值更低（1个像素即可），适合捕捉小光斑
        red_blobs = img.find_blobs([red_threshold], x_stride=1, pixels_threshold=1, area_threshold=2, merge=1)
                                # 遍历每个红色色块，且确保存在
        for red_blob in red_blobs:
             if red_blob:
                                # 格式化字符串：例如 RDx120y088，cx() 和 cy() 返回色块中心坐标（整型，最大320和240）。%03d 表示三位数字，不足补零
                data = "RDx%03dy%03d" % (red_blob.cx(), red_blob.cy())
                usart3.write(data)
                pyb.delay(5)  # 添加短暂延时,延时5毫秒，避免发送过快导致缓冲区溢出
                #在图像上绘制矩形框标记绿色色块,在图像上绘制红色矩形框标记该色块（调试显示用，颜色红色）
                img.draw_rectangle(red_blob.rect(), color=(255, 0, 0))
                # 在图像上绘制十字标记绿色色块的中心
                img.draw_cross(red_blob.cx(), red_blob.cy(), color=(255, 0, 0))

                                # 绿色色块处理类似，发送 GDx...y... 格式，并在终端打印坐标
        for green_blob in green_blobs:
            if green_blob:
                data = "GDx%03dy%03d" % (green_blob.cx(), green_blob.cy())
                usart3.write(data)
                print(green_blob.cx(), green_blob.cy())
                #pyb.delay(10)  # 添加短暂延时
                # 在图像上绘制矩形框标记绿色色块
                img.draw_rectangle(green_blob.rect(), color=(0, 255, 0))
                # 在图像上绘制十字标记绿色色块的中心
                img.draw_cross(green_blob.cx(), green_blob.cy(), color=(0, 255, 0))

        #串口接收
                                # 检查串口接收缓冲区是否有数据，若有则读取并解码为 UTF-8 字符串
        if usart3.any():  # 检查是否有数据可读
            data = usart3.read()  # 读取所有可用数据
            text = data.decode('utf-8')
                                # 如果收到 "JX" 指令，则置位 inrectangle_send 标志，稍后会发送内框坐标
            if text == "JX":
                inrectangle_send = True
                print("Send")
            elif text == "REST":# 收到 "REST" 则回复 "SLAVER"，并打印 123（调试）
                data = "SLAVER"
                usart3.write(data)
                print(123)
            elif text == "S3":  # 收到 "S3" 就原样返回
                usart3.write(text)
            elif text == "STOP":# 收到 "STOP" 也原样返回
                usart3.write(text)

        # 绘制原始矩形
        for i in range(4):      # 原本要绘制原始矩形的四条边（绿色），但被注释掉了，所以不会显示
            p1 = stable_original_corners[i]
            p2 = stable_original_corners[(i + 1) % 4]
            #img.draw_line(p1[0], p1[1], p2[0], p2[1], color=(0, 255, 0))

        # 绘制缩小后的矩形
        for i in range(4):      # 准备绘制收缩后的内框。注意先提取了四个角点到 x1~x4（但后续绘图时仅使用了 p1,p2）
            x1 = stable_shrunk_corners[0]
            x2 = stable_shrunk_corners[1]
            x3 = stable_shrunk_corners[2]
            x4 = stable_shrunk_corners[3]
            p1 = stable_shrunk_corners[i]
            p2 = stable_shrunk_corners[(i + 1) % 4]
                                # 当收到 "JX" 指令后，inrectangle_send 为真，则构造一个字符串，包含四个内框角点坐标。注意顺序是 x4, x3, x2, x1（逆时针但起点从 x4 开始）。代码注释说“起点在左下角，按逆时针顺序排序，所以反方向打印”
            if inrectangle_send == True:   #发送一次点位
                data = "JXax%03dy%03dbx%03dy%03dcx%03dy%03ddx%03dy%03d" % (x4[0], x4[1], x3[0], x3[1], x2[0], x2[1], x1[0], x1[1])  #起点在左下角，按逆时针顺序排序，所以反方向打印
                usart3.write(data)# 发送数据并在终端打印
                print(x4[0], x4[1], x3[0], x3[1], x2[0], x2[1], x1[0], x1[1])
                                # 标志置为 False，确保只发送一次
                inrectangle_send = False
                                # 用红色线条绘制内框的四条边
            img.draw_line(p1[0], p1[1], p2[0], p2[1], color=(255, 0, 0))

    # 计算帧率
    #fps = clock.fps()
    # 在 RGB 图像上显示帧率
    #img.draw_string(10, 10, f"FPS: {fps:.2f}", color=(255, 255, 255), scale=1)
