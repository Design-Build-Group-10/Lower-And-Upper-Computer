#include "esp_camera.h"
#include <WiFi.h>
#include <AsyncUDP.h>


AsyncUDP udp;

const char *ssid = "10组CAM";
const char *password = "87654321";


//.......................................................//

#define max_packet_byte 1024 //定义发送图片的包分割大小,每个图像包最大1024字节



//安信可板卡型号选择
#define PWDN_GPIO_NUM 32
#define RESET_GPIO_NUM -1
#define XCLK_GPIO_NUM 0
#define SIOD_GPIO_NUM 26
#define SIOC_GPIO_NUM 27
#define Y9_GPIO_NUM 35
#define Y8_GPIO_NUM 34
#define Y7_GPIO_NUM 39
#define Y6_GPIO_NUM 36
#define Y5_GPIO_NUM 21
#define Y4_GPIO_NUM 19
#define Y3_GPIO_NUM 18
#define Y2_GPIO_NUM 5
#define VSYNC_GPIO_NUM 25
#define HREF_GPIO_NUM 23
#define PCLK_GPIO_NUM 22



int initCamera()
{
  //配置摄像头
  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000; //摄像头的工作时钟，在允许范围内频率越高帧率就越高

  //----------可用的图像格式----------
  //  PIXFORMAT_RGB565,    // 2BPP/RGB565
  //  PIXFORMAT_YUV422,    // 2BPP/YUV422
  //  PIXFORMAT_GRAYSCALE, // 1BPP/GRAYSCALE
  //  PIXFORMAT_JPEG,      // JPEG/COMPRESSED
  //  PIXFORMAT_RGB888,    // 3BPP/RGB888
  //  PIXFORMAT_RAW,       // RAW
  //  PIXFORMAT_RGB444,    // 3BP2P/RGB444
  //  PIXFORMAT_RGB555,    // 3BP2P/RGB555
  config.pixel_format = PIXFORMAT_JPEG; //输出JPEG图像

  //----------可用的图像分辨率----------
  //  FRAMESIZE_96x96,    // 96x96
  //  FRAMESIZE_QQVGA,    // 160x120
  //  FRAMESIZE_QQVGA2,   // 128x160
  //  FRAMESIZE_QCIF,     // 176x144
  //  FRAMESIZE_HQVGA,    // 240x176
  //  FRAMESIZE_240x240,  // 240x240
  //  FRAMESIZE_QVGA,     // 320x240
  //  FRAMESIZE_CIF,      // 400x296
  //  FRAMESIZE_VGA,      // 640x480
  //  FRAMESIZE_SVGA,     // 800x600
  //  FRAMESIZE_XGA,      // 1024x768
  //  FRAMESIZE_SXGA,     // 1280x1024
  //  FRAMESIZE_UXGA,     // 1600x1200
  //  FRAMESIZE_QXGA,     // 2048*1536

  if (psramFound())
  {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }
  else
  {
    //实际走这里
    config.frame_size = FRAMESIZE_QVGA; //分辨率 ,不宜调大，否则会出现闪屏
    config.jpeg_quality = 14;           //（10-63）压缩率，越小照片质量越好
    config.fb_count = 1;
  }

  // camera init
  esp_err_t err = esp_camera_init(&config);
  if (err != ESP_OK)
  {
    Serial.printf("Camera init failed with error 0x%x", err);
    return 1;
  }

  sensor_t *s = esp_camera_sensor_get();
  s->set_vflip(s, 1); //no flip it back

  Serial.println("Camera Init OK!!!");
  return 0;
}

void setup() //程序加电后初始化
{
//  Serial.begin(115200);
  Serial1.begin(115200, SERIAL_8N1, 3, 1); //设置串口1,用于向机器人主体芯片发送指令 ，3为rx,1为tx

  Serial.setDebugOutput(true);
 


  WiFi.mode(WIFI_AP);                  //wifi初始化
  while (!WiFi.softAP(ssid, password)) //启动AP
  {
  }
  Serial.println("AP启动成功");
  while (!udp.listen(10001)) //等待本机udp监听端口设置成功
  {
  }

  udp.onPacket([](AsyncUDPPacket packet) { //注册一个数据包接收事件，可异步接收数据
    String curCommand = String((const char*)packet.data()).substring(0, packet.length());

    Serial.println("收到命令:" + curCommand);

    if (curCommand.equals("ledon")) {
      digitalWrite(4, HIGH);
    } else if (curCommand.equals("ledoff")) {
      digitalWrite(4, LOW);
    } else  {
      //Serial1.println(curCommand); //向串口1发送其他操作命令
      //byte sendcmd = curCommand.charAt(0);
      //Serial.write(&sendcmd,sizeof(sendcmd));
      Serial.println(curCommand);
      //Serial.println("12345");
      delay(20);
    }

    const char*p = curCommand.c_str();
    packet.printf(p, strlen(p));  //向上位机回传收到的命令，表示已收到
  });

  initCamera(); //相机初始化



  pinMode(4, OUTPUT);    //LED灯，GPIO4
  digitalWrite(4, HIGH); // turn the LED on by making the voltage HIGH
  delay(1000);
  digitalWrite(4, LOW); // turn the LED off by making the voltage LOW

  //用于视频传输
  xTaskCreate(
    taskOne,
    "TaskOne",
    10000,
    NULL,
    1,
    NULL
  );

  //用于串口通信
  xTaskCreate(
    taskTwo,
    "TaskTwo",
    10000,
    NULL,
    1,
    NULL
  );
}

void loop() {

}



void taskOne(void *parameter) {

  while (1) {

    if (udp.connect(IPAddress(192, 168, 4, 2), 10000)) //检查网络连接是否存在,这取决于上位机是否连接,这里条件是如果有连接则处理,否则进入下一次loop.
    {
      camera_fb_t *fb = NULL;
      fb = esp_camera_fb_get(); //拍照
      if (!fb)                  //拍照不成功时重新拍照
      {
        Serial.println("Camera capture failed");
        return;
      }
      uint8_t *P_temp = fb->buf;                            //暂存指针初始位置
      int pic_length = fb->len;                             //获取图片字节数量
      int pic_pack_quantity = pic_length / max_packet_byte; //将图片分包时可以分几个整包
      int remine_byte = pic_length % max_packet_byte;       //余值,即最后一个包的大小

      udp.print("ok");                            //向上位机发送一个字符串"ok"表示开始发送信息,这也是第一个包,包的内容是字符串,即只有2个字节.虽然是二进制发送,但对方可以方便地转为ASCII字符
      udp.print(pic_length);                      //向上位机发送这个图片的大小,这个包也是用字符串发送,即每个字符占一个字节
      for (int j = 0; j < pic_pack_quantity; j++) //发送图片信息,这是按分包循环发送,每一次循环发送一个包,包的大小就是上面指定的1024个字节.
      {
        udp.write(fb->buf, max_packet_byte); //将图片分包发送
        for (int i = 0; i < max_packet_byte; i++)
        {
          fb->buf++; //图片内存指针移动到相应位置
        }
      }
      udp.write(fb->buf, remine_byte); //发送最后一个包，剩余的数据

      fb->buf = P_temp;         //将当时保存的指针重新返还最初位置
      esp_camera_fb_return(fb); //清理像机

    }
    delay(100);
  }

}


void taskTwo(void *parameter) {
  while (1) {
    // 发送信息到 Serial1
    String command = "HELLO";
    Serial1.println(command); // 发送命令到机器人主体芯片
    Serial.println("Sent command: " + command); // 在调试串口上打印发送的命令

    // 检查是否有来自 Serial1 的数据
    // if (Serial1.available()) {
    //   String response = Serial1.readStringUntil('\n'); // 读取直到换行符
    //   Serial.println("Received response: " + response); // 在调试串口上打印收到的响应
    // }

    delay(100);
  }
}