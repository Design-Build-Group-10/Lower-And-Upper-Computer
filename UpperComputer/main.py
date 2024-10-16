import socket
import asyncio
import websockets

# 根据cam单片机中烧录的代码，电脑连网"10组CAM"，电脑IPv4选择192.168.4.2，掩码0.0.0.0

# 设置UDP服务器地址和端口
UDP_IP = "0.0.0.0"  # 使用0.0.0.0来监听所有网络接口
UDP_PORT = 10000
MAX_PACKET_SIZE = 1024

# 创建UDP套接字
sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind((UDP_IP, UDP_PORT))

a = 0


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


# def display_image(image_data):
#     # 将图像数据转换为numpy数组并解码为图像
#     image_array = np.frombuffer(image_data, dtype=np.uint8)
#     image = cv2.imdecode(image_array, cv2.IMREAD_COLOR)
#
#     if image is None:
#         print("Error: Invalid image data.")
#         return
#
#     # 显示结果
#     cv2.imshow('ESP32-CAM', image)
#     cv2.waitKey(1)


async def send_images(websocket):
    while True:
        try:
            # 接收图像数据
            image_data = receive_image(sock)

            if image_data:
                # 通过 WebSocket 发送图像数据
                await websocket.send(image_data)
                global a
                print("Image data sent successfully.", a)
                a += 1

        except Exception as e:
            print(f"Error: {e}")
            break


async def main():
    uri = "wss://service.design-build.site/api/ws/camera/SR-2024X-7B4D-QP98"

    async with websockets.connect(uri) as websocket:
        print("WebSocket connection established.")
        await send_images(websocket)  # 开始接收和发送图像


if __name__ == "__main__":
    asyncio.get_event_loop().run_until_complete(main())