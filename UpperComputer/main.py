import socket
import numpy as np
import cv2
import asyncio
import websockets

# 根据cam单片机中烧录的代码，电脑连网"10组CAM"，电脑IPv4选择192.168.4.2，掩码0.0.0.0

# 设置UDP服务器地址和端口
UDP_IP = "0.0.0.0"  # 使用0.0.0.0来监听所有网络接口
UDP_PORT = 12345  # 与单片机的remotePort一致
MAX_PACKET_SIZE = 1024

# 创建UDP套接字
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))


def receive_image(sock1):
    # 接收开始信号
    while True:
        data, addr = sock1.recvfrom(MAX_PACKET_SIZE)
        try:
            if data.decode('utf-8') == "ok":
                break
        except UnicodeDecodeError:
            continue

    # 接收图片大小
    data, addr = sock1.recvfrom(MAX_PACKET_SIZE)
    try:
        pic_length = int(data.decode('utf-8'))
    except ValueError:
        print("Error: Invalid image size.")
        return

    # 初始化图像数据缓冲区
    image_data = b''
    while len(image_data) < pic_length:
        data, addr = sock1.recvfrom(MAX_PACKET_SIZE)
        image_data += data

    return image_data


def display_image(image_data):
    # 将图像数据转换为numpy数组并解码为图像
    image_array = np.frombuffer(image_data, dtype=np.uint8)
    image = cv2.imdecode(image_array, cv2.IMREAD_COLOR)

    if image is None:
        print("Error: Invalid image data.")
        return

    # 显示结果
    cv2.imshow('ESP32-CAM', image)
    cv2.waitKey(1)


async def send_image(image_data):
    uri = "ws://62.234.168.154/api/ws/camera/"

    async with websockets.connect(uri) as websocket:
        # 发送二进制数据到 WebSocket 服务
        await websocket.send(image_data)
        print("Image data sent successfully.")

        # 接收服务器返回的数据
        response = await websocket.recv()
        print(f"Received response from server: {response}")


def main():
    try:
        while True:
            try:
                image_data = receive_image(sock)  # 接收图像数据
                display_image(image_data)  # 显示图像
                asyncio.get_event_loop().run_until_complete(send_image(image_data))  # 发送图像数据

            except Exception as e:
                print(f"Error: {e}")
    finally:
        # 关闭窗口和连接
        cv2.destroyAllWindows()
        sock.close()


if __name__ == "__main__":
    main()
