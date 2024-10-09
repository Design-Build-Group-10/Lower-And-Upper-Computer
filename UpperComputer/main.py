import socket
import numpy as np
import cv2
import time
import os

# 根据cam单片机中烧录的代码，电脑连网"10组CAM"，电脑IPv4选择192.168.4.2，掩码0.0.0.0

# 设置UDP服务器地址和端口
UDP_IP = "0.0.0.0"  # 使用0.0.0.0来监听所有网络接口
UDP_PORT = 10000
MAX_PACKET_SIZE = 1024

# 创建UDP套接字
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

def receive_image(sock):
    # 接收开始信号
    while True:
        data, addr = sock.recvfrom(MAX_PACKET_SIZE)
        try:
            if data.decode('utf-8') == "ok":
                break
        except UnicodeDecodeError:
            continue

    # 接收图片大小
    data, addr = sock.recvfrom(MAX_PACKET_SIZE)
    pic_length = int(data.decode('utf-8'))

    # 初始化图像数据缓冲区
    image_data = b''
    while len(image_data) < pic_length:
        data, addr = sock.recvfrom(MAX_PACKET_SIZE)
        image_data += data

    return image_data

def main():
    try:
        while True:
            try:
                # 接收图像数据
                image_data = receive_image(sock)

                # 将图像数据转换为numpy数组并解码为图像
                image_array = np.frombuffer(image_data, dtype=np.uint8)
                image = cv2.imdecode(image_array, cv2.IMREAD_COLOR)

                # 显示结果
                cv2.imshow('ESP32-CAM', image)

                if cv2.waitKey(1) & 0xFF == ord('q'):
                    break
            except Exception as e:
                print(f"Error: {e}")
    finally:
        # 关闭窗口和连接
        cv2.destroyAllWindows()
        sock.close()

if __name__ == "__main__":
    main()