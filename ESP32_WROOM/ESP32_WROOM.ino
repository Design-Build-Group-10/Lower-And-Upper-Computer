#include <soc/soc.h>
#include <soc/rtc_cntl_reg.h>
#include <soc/rtc_wdt.h> //设置看门狗用
#include <Wire.h>
#include <WiFi.h>
#include <WiFiClient.h>
#include <WebServer.h>
#include <ESPmDNS.h>
#include <SPI.h>
#include <SD.h>
#include <WiFiAP.h>
#include <ESP32Servo.h>
#include <BluetoothSerial.h>
//#include<map>//map函数头文件
#include "EEPROM.h"

#define Addr 0x1C
#define SDA 18
#define SCL 19

#define EEPROM_SIZE 64
Servo servo1[12];
const int servo1_pin[12] = {18,  5, 19, 2,  4, 15, 33, 25, 32, 14, 27, 13};

/* Servos --------------------------------------------------------------------*/
//define 12 servos for 4 legs
Servo servo[4][3];
//define servos' ports
const int servo_pin[4][3] = {{18,  5,  19}, {  2,  4, 15}, {33, 25,  32}, {14,  27, 13}};
int8_t offset[4][3] =    {{0, 0, 0}, {0, 0, 0}, { 0, 0, 0}, {0, 0, 0}};
int mapM[16] = {1,0,2,5,3,4,0,0,0,0,7,6,8,11,9,10};
//map<int, int> mapM = {{19:8},{11:6},{14:7},{15:10},{12:9},{13:11},{0:2},{1:0},{4:1},{5:4},{2:3},{3:5}};
/* Size of the robot ---------------------------------------------------------*/
const float length_a = 55;
const float length_b = 77.5;
const float length_c = 27.5;
const float length_side = 71;
const float z_absolute = -28;
/* Constants for movement ----------------------------------------------------*/
const float z_default = -50, z_up = -30, z_boot = z_absolute;
const float x_default = 62, x_offset = 0;
const float y_start = 0, y_step = 40;
const float y_default = x_default;
/* variables for movement ----------------------------------------------------*/
volatile float site_now[4][3];    //real-time coordinates of the end of each leg
volatile float site_expect[4][3]; //expected coordinates of the end of each leg
float temp_speed[4][3];   //each axis' speed, needs to be recalculated before each movement
float move_speed = 1.4;   //movement speed
float speed_multiple = 1; //movement speed multiple
const float spot_turn_speed = 4;
const float leg_move_speed = 8;
const float body_move_speed = 3;
const float stand_seat_speed = 1;
volatile int rest_counter;      //+1/0.02s, for automatic rest
//functions' parameter
const float KEEP = 255;
//define PI for calculation
const float pi = 3.1415926;
/* Constants for turn --------------------------------------------------------*/
//temp length
const float temp_a = sqrt(pow(2 * x_default + length_side, 2) + pow(y_step, 2));
const float temp_b = 2 * (y_start + y_step) + length_side;
const float temp_c = sqrt(pow(2 * x_default + length_side, 2) + pow(2 * y_start + y_step + length_side, 2));
const float temp_alpha = acos((pow(temp_a, 2) + pow(temp_b, 2) - pow(temp_c, 2)) / 2 / temp_a / temp_b);
//site for turn
const float turn_x1 = (temp_a - length_side) / 2;
const float turn_y1 = y_start + y_step / 2;
const float turn_x0 = turn_x1 - temp_b * cos(temp_alpha);
const float turn_y0 = temp_b * sin(temp_alpha) - turn_y1 - length_side;

int FRFoot = 0;
int FRElbow = 0;
int FRShdr = 0;
int FLFoot = 0;
int FLElbow = 0;
int FLShdr = 0;
int RRFoot = 0;
int RRElbow = 0;
int RRShdr = 0;
int RLFoot = 0;
int RLElbow = 0;
int RLShdr = 0;

int abc = 0;

static uint32_t currentTime = 0;
static uint16_t previousTime = 0;
uint16_t cycleTime = 0;
//超声波避障变量定义
float       distance;
const int   trig_echo = 23;

int act = 1;
char val;

String header;
const char *ssid = "10组小蜘蛛"; //热点名称，自定义
const char *password = "87654321";//连接密码，自定义 
void Task1code( void *pvParameters );
void Task2code( void *pvParameters );
void Task3code( void *pvParameters );
//wifi在core0，其他在core1；1为大核
WiFiServer server(80);

void setup()
{
  //ESP32看门狗设置 需要先引入 #include "soc/rtc_wdt.h" //设置看门狗用
  rtc_wdt_protect_off(); //看门狗写保护关闭
  rtc_wdt_enable(); //启用看门狗
  rtc_wdt_feed(); //喂狗
  rtc_wdt_set_time(RTC_WDT_STAGE0, 7000); // 设置看门狗超时 7000ms.

  Serial.begin(115200);
  Serial.println("Robot starts initialization");

  // 串口设置
  Serial1.begin(115200, SERIAL_8N1, 16, 17); 
  Serial.println("Setup complete, waiting for data...");


  //initialize default parameter
  set_site(0, x_default - x_offset, y_start + y_step, z_boot);
  set_site(1, x_default - x_offset, y_start + y_step, z_boot);
  set_site(2, x_default + x_offset, y_start, z_boot);
  set_site(3, x_default + x_offset, y_start, z_boot);
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      site_now[i][j] = site_expect[i][j];
    }
  } 
  EEPROM.begin(64);
  //start servo service
  Serial.println("Servo service started");
  //initialize servos
  servo_celebration();
  servo_attach();
  Serial.println("Servos initialized");
  Serial.println("Robot initialization Complete");

  WiFi.softAP(ssid, password);//不写password，即热点开放不加密
  IPAddress myIP = WiFi.softAPIP();//此为默认IP地址192.168.4.1，也可在括号中自行填入自定义IP地址
  Serial.print("AP IP address:");
  Serial.println(myIP);
  server.begin();
  Serial.println("Server started");
 
  for (int i = 0; i < 12; i++)
  {
    int8_t val = EEPROM.read(i);
    Serial.println(val); Serial.print(" ");
  }
 // servo_celebration();
  //  打印测试offset数组是否正确
  int8_t left1 = EEPROM.read(7);
  int8_t left2 = EEPROM.read(6);
  int8_t left3 = EEPROM.read(8);
  int8_t left4 = EEPROM.read(11);
  int8_t left5 = EEPROM.read(9);
  int8_t left6 = EEPROM.read(10);
  int8_t right1 = EEPROM.read(1);
  int8_t right2 = EEPROM.read(0);
  int8_t right3 = EEPROM.read(2);
  int8_t right4 = EEPROM.read(5);
  int8_t right5 = EEPROM.read(3);
  int8_t right6 = EEPROM.read(4);
  
  String legValStr = String(left1) + " " + String(left2) + " " + String(left3) + " " + String(left4) + " " + String(left5) + " " + String(left6) + " " + String(right1) + " " + String(right2) + " " + String(right3) + " " + String(right4) + " " + String(right5) + " " + String(right6);
  Serial.println(legValStr);
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      int8_t asc = offset[i][j];
      Serial.print(" :");
      Serial.println(asc);
    }
  }


  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);//关闭低电压检测,避免无限重启
  xTaskCreatePinnedToCore(Task1, "Task1", 10000, NULL, 1, NULL,  0);  //最后一个参数至关重要，决定这个任务创建在哪个核上.PRO_CPU 为 0, APP_CPU 为 1,或者 tskNO_AFFINITY 允许任务在两者上运行.
  xTaskCreatePinnedToCore(Task2, "Task2", 10000, NULL, 1, NULL,  1);//
  //实现任务的函数名称（task1）；任务的任何名称（“ task1”等）；分配给任务的堆栈大小，以字为单位；任务输入参数（可以为NULL）；任务的优先级（0是最低优先级）；任务句柄（可以为NULL）；任务将运行的内核ID（0或1）
  xTaskCreatePinnedToCore(Task3, "Task3", 10000, NULL, 1, NULL,  1);
}



void servo_celebration(void)
{
  int z = 0;
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      int8_t val = EEPROM.read(z);
      offset[i][j] = val;
      z++;
      delay(100);
    }
  }
}

void servo_attach(void)
{
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      servo[i][j].attach(servo_pin[i][j], 500, 2500);
      delay(100);
      servo[i][j].write(90+offset[i][j]);
      Serial.print("offset:");
      Serial.println(offset[i][j]);
      delay(20);
    }
  }

  
}

void servo_detach(void)
{
  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      servo[i][j].detach();
      delay(100);
    }
  }
}

void writeKeyValue(int8_t key, int8_t value)
{
  EEPROM.write(key, value);
  EEPROM.commit();
}

int8_t readKeyValue(int8_t key)
{
  Serial.println("read");
  Serial.println(key);

  int8_t value = EEPROM.read(key);
}


//从web接收设置的校准值
void handleSave()
{

  String key = header.substring(14, 16);
  String value = header.substring(23, 26);

  int8_t keyInt = key.toInt();
  int8_t valueInt = 0;
  for (int i = value.length() - 1; i >= 0; i--) {
    if (value.charAt(i) >=48 && value.charAt(i) <= 57) {
      value = value.substring(0, i + 1);
      break;
    }
  }
  keyInt = mapM[keyInt];
  valueInt = value.toInt();
  
  Serial.print("key:");
  Serial.println(keyInt);
  Serial.print("value:");
  Serial.println(value);
  Serial.print(header);
//  int z = 0;
//  for (int i = 0; i < 4; i++)
//  {
//    for (int j = 0; j < 3; j++)
//    {
//      if (z = keyInt) {
//        //servo[i][j].attach(servo_pin[i][j], 500, 2500);
//        servo[i][j].write(90 + valueInt);
//        vTaskDelay(20);
//      }
//      z++;
//      if (z == 6) z = 10;
//
//    }
//  }
//
  int row = keyInt / 3;
  int col = keyInt % 3;
  servo[row][col].write(90 + valueInt);
//  servo[row][col].attach(servo_pin[row][col], 500, 2500);
  offset[row][col] = valueInt;
  if (valueInt >= -99 && valueInt <= 124) // 确认数据介于 -124 ~ 124
  {
    writeKeyValue(keyInt, valueInt); // 存储校正值
  }
  //servo_attach();
  //servo_service();
  servo_celebration();
  vTaskDelay(10);
}
//
////APP点击验证
//void handleController()
//{
//  String pm = server.arg("pm");
//  String servo = server.arg("servo");
//
//  if (pm != "")
//  {
//    Servo_PROGRAM = pm.toInt();
//  }
//
//  if (servo != "")
//  {
//    GPIO_ID = servo.toInt();
//    ival = server.arg("value");
//  }
//
//  server.send(200, "text/html", "(pm)=(" + pm + ")");
//}



void loop()
{
  // put your main code here, to run repeatedly:
}

void Task1(void *pvParameters)
{
  //在这里可以添加一些代码，这样的话这个任务执行时会先执行一次这里的内容（当然后面进入while循环之后不会再执行这部分了）
  while (1)
  {
    vTaskDelay(20);
    WiFiClient client = server.available();  //监听连入设备
    if (client)
    {
      String currentLine = "";
      while (client.connected())
      {
        if (client.available())
        {
          char c = client.read();
          header += c;
          if (c == '\n')
          {
            if (currentLine.length() == 0)
            {
              client.println("HTTP/1.1 200 OK");
              client.println("Content-type:text/html");
              client.println();
              client.println("<!DOCTYPE html><html>");
              client.println("<head><meta name=\"viewport\" content=\"width=device-width, initial-scale=1\">");
              client.println("<meta charset=\"UTF-8\">");
              client.println("<link rel=\"icon\" href=\"data:,\">");
              client.println("<style>html { font-family: Helvetica; display: inline-block; margin: 0px auto; text-align: center;}");
              client.println(".button { background-color: #096625; border: none; color: white; padding: 20px 25px; ");
              client.println("text-decoration: none; font-size: 18px; margin: 2px; cursor: pointer;}");
              client.println(".button2 {background-color: #555555; border: none; color: white; padding: 16px 20px;text-decoration: none; font-size: 16px; margin: 1px; cursor: pointer;}</style></head>");
              client.println("<body><h1>四足机器人控制</h1>");
              client.println("<p><a href=\"/20/on\"><button class=\"button\">前进</button></a></p>");
              client.println("<p><a href=\"/21/on\"><button class=\"button\">左转</button></a><a href=\"/22/on\"><button class=\"button\">停止</button></a><a href=\"/23/on\"><button class=\"button\">右转</button></a></p>");
              client.println("<p><a href=\"/24/on\"><button class=\"button\">后退</button></a></p>");
              client.println("<p><a href=\"/25/on\"><button class=\"button2\">挥手</button></a><a href=\"/26/on\"><button class=\"button2\">握手</button></a><a href=\"/27/on\"><button class=\"button2\">跳舞</button></a><a  href=\"/28/on\"><button class=\"button2\">坐下</button></a></p>");
              client.println("</body></html>");
              client.println();
              if (header.indexOf("controller?pm") >= 0) //完成校准，进入准备做动作的状态
              {
                //abc = 2;
                int8_t left1 = EEPROM.read(7);
                int8_t left2 = EEPROM.read(6);
                int8_t left3 = EEPROM.read(8);
                int8_t left4 = EEPROM.read(11);
                int8_t left5 = EEPROM.read(9);
                int8_t left6 = EEPROM.read(10);
                int8_t right1 = EEPROM.read(1);
                int8_t right2 = EEPROM.read(0);
                int8_t right3 = EEPROM.read(2);
                int8_t right4 = EEPROM.read(5);
                int8_t right5 = EEPROM.read(3);
                int8_t right6 = EEPROM.read(4);
                
                String legValStr ="LegVal:" + String(left1) + " " + String(left2) + " " + String(left3) + " " + String(left4) + " " + String(left5) + " " + String(left6) + " " + String(right1) + " " + String(right2) + " " + String(right3) + " " + String(right4) + " " + String(right5) + " " + String(right6);
                client.println(legValStr);
                vTaskDelay(20);
                
              }
              if (header.indexOf("save?key") >= 0) //校准函数
              {
              
                abc = 1;
                vTaskDelay(20);
//                server.send(200,"text/html",initialVal);  
              }
              if (header.indexOf("GET /20/on") >= 0) //前进
              {
                abc = 3;
                Serial.println("前进");
                step_forward(1);
              }
              if (header.indexOf("GET /21/on") >= 0) //后退
              {
                abc = 3;
                Serial.println("后退");
                step_back(1);
              }
              if (header.indexOf("GET /22/on") >= 0) //左转
              {
                abc = 3;
                Serial.println("22");
                turn_left(1);
              }
              if (header.indexOf("GET /23/on") >= 0) //右转
              {
                abc = 3;
                turn_right(1);
              }
              if (header.indexOf("GET /24/on") >= 0) //停止
              {
                abc = 3;
                val = 'b';
                Serial.println(val);
              }
              if (header.indexOf("GET /25/on") >= 0) //握手
              {
                abc = 3;
                hand_shake(1);
              }
              if (header.indexOf("GET /26/on") >= 0) //抬头
              {
                abc = 3;
                FLElbow -= 4; FRElbow -= 4;
                RLElbow += 4; RRElbow += 4;
                FLFoot -= 4; FRFoot -= 4;
                RLFoot += 4; RRFoot += 4;
                stand();
              }
              if (header.indexOf("GET /27/on") >= 0) //招手
              {
                abc = 3;
                hand_wave(1);
              }
              if (header.indexOf("GET /28/on") >= 0) //左扭
              {
                abc = 3;
                FLShdr += 4; FRShdr -= 4;
                RLShdr -= 4; RRShdr += 4;
                stand();
              }
              if (header.indexOf("GET /29/on") >= 0) //低头
              {
                abc = 3;
                FLElbow += 4; FRElbow += 4;
                RLElbow -= 4; RRElbow -= 4;
                FLFoot += 4; FRFoot += 4;
                RLFoot -= 4; RRFoot -= 4;
                stand();
              }
              if (header.indexOf("GET /30/on") >= 0) //右扭
              {
                abc = 3;
                FLShdr -= 4; FRShdr += 4;
                RLShdr += 4; RRShdr -= 4;
                stand();
              }
              if (header.indexOf("GET /31/on") >= 0) //左倾
              {
                abc = 3;
                if (!is_stand()) stand();
                FLElbow += 4; FRElbow -= 4;
                RLElbow += 4; RRElbow -= 4;
                FLFoot -= 4; FRFoot += 4;
                RLFoot -= 4; RRFoot += 4;
                stand();
              }
              if (header.indexOf("GET /32/on") >= 0) //抬升
              {
                abc = 3;
                FLElbow -= 4; FRElbow -= 4; //--
                RLElbow -= 4; RRElbow -= 4; //--
                FLFoot -= 4; FRFoot -= 4; //++
                RLFoot -= 4; RRFoot -= 4; //++
                stand();
              }
              if (header.indexOf("GET /33/on") >= 0) //右倾
              {
                abc = 3;
                if (!is_stand()) stand();
                FLElbow -= 4; FRElbow += 4;
                RLElbow -= 4; RRElbow += 4;
                FLFoot += 4; FRFoot -= 4;
                RLFoot += 4; RRFoot -= 4;
                stand();
              }
              if (header.indexOf("GET /34/on") >= 0) //服务
              {
                abc = 3;
                sit();
                b_init();
                FLElbow = 0; FRElbow = 0;
                RLElbow = 0; RRElbow = 0;
                FLFoot = 0; FRFoot = 0;
                RLFoot = 0; RRFoot = 0;
                FLShdr = 0; FRShdr = 0;
                RLShdr = 0; RRShdr = 0;
                stand();
              }
              if (header.indexOf("GET /35/on") >= 0) //下蹲
              {
                abc = 3;
                FLElbow += 4; FRElbow += 4;
                RLElbow += 4; RRElbow += 4;
                FLFoot += 4; FRFoot += 4;
                RLFoot += 4; RRFoot += 4;
                stand();
              }
              if (header.indexOf("GET /36/on") >= 0) //复位
              {
                abc = 3;
                FLElbow = 0; FRElbow = 0;
                RLElbow = 0; RRElbow = 0;
                FLFoot = 0; FRFoot = 0;
                RLFoot = 0; RRFoot = 0;
                FLShdr = 0; FRShdr = 0;
                RLShdr = 0; RRShdr = 0;
                stand();
              }
              if (header.indexOf("GET /37/on") >= 0) //坐下
              {
                abc = 3;
                sit();
              }
              if (header.indexOf("GET /38/on") >= 0) //跳舞
              {
                abc = 3;
                body_dance(10);
              }
              if (header.indexOf("GET /39/on") >= 0) //开始避障
              {
                abc = 3;
                Avoid();//
              }
              if (header.indexOf("GET /40/on") >= 0) //结束避障
              {
                abc = 3;
                sit();
                b_init();
                FLElbow = 0; FRElbow = 0;
                RLElbow = 0; RRElbow = 0;
                FLFoot = 0; FRFoot = 0;
                RLFoot = 0; RRFoot = 0;
                FLShdr = 0; FRShdr = 0;
                RLShdr = 0; RRShdr = 0;
                stand(); //
              }
              break;
            }
            else
            {
              currentLine = ""; //如果收到是换行，则清空字符串变量
            }
          }
          else if (c != '\r')
          {
            currentLine += c;
          }
        }
        vTaskDelay(1);
      }
      header = "";
      client.stop(); //断开连接
    }
  }
}

bool Avoid(void)
{
  while (1)
  {
    vTaskDelay(10);
    WiFiClient client = server.available();  //监听连入设备
    pinMode(trig_echo, OUTPUT);                      //设置Trig_RX_SCL_I/O为输出

    digitalWrite(trig_echo, HIGH);
    delayMicroseconds(10);
    digitalWrite(trig_echo, LOW);                    //Trig_RX_SCL_I/O脚输出10US高电平脉冲触发信号

    pinMode(trig_echo, INPUT);                       //设置Trig_RX_SCL_I/O为输入，接收模块反馈的距离信号

    distance  = pulseIn(trig_echo, HIGH);            //计数接收到的高电平时间
    //Serial.print("原始值: ");
    Serial.print(distance);
    distance  = distance * 340 / 2 / 10000;          //计算距离 1：声速：340M/S  2：实际距离为1/2声速距离 3：计数时钟为1US//温补公式：c=(331.45+0.61t/℃)m•s-1 (其中331.45是在0度）
    Serial.print("距离: ");
    Serial.print(distance);
    Serial.println("CM");                            //串口输出距离信号
    pinMode(trig_echo, OUTPUT);                      //设置Trig_RX_SCL_I/O为输出，准备下次测量
    // Serial.println("");                              //换行

    if (client)
    {
      String currentLine = "";
      while (client.connected())
      {
        if (client.available())
        {
          char c = client.read();
          header += c;
          if (c == '\n')
          {
            if (currentLine.length() == 0)
            {
              if (header.indexOf("GET /40/on") >= 0) //结束避障
              {
                return true;
              }
              else
              {
                return false;
              }
            }
          }
        }
      }
    }
    delay(30);                                       //单次测离完成后加30mS的延时再进行下次测量。防止近距离测量时，测量到上次余波，导致测量不准确。

    if (distance <= 30)
    {
      stand();
      turn_left(5);
    }
    else
    {
      step_forward(5);
    }

  }
}



void Task2(void *pvParameters)
{
  while (1)
  {
    vTaskDelay(15);
    if (abc == 1)
    {
      handleSave();
      abc = 0;
    }

//    if (abc == 2)
//    {
//      set_zero();
//      
//      abc = 0;
//    }
    
    else if (abc == 2)
    {
      servo_service();
//      Serial.print("abc:");
//      Serial.print(abc);
     // Serial.println();
      abc = 3;
    }else if (abc == 3) {
      servo_service();
    }

  }
}

//
//
//void set_zero()
//{
//
//  for (int i = 0; i < 4; i++)
//  {
//    for (int j = 0; j < 3; j++)
//    {
//      //servo_attach;
//      //servo_detach;
//      servo[i][j].write(90);
//      vTaskDelay(20);
//      String key = header.substring(14, 16);
//      String value = header.substring(23, 26);
//
//      int8_t keyInt = key.toInt();
//      int8_t valueInt = value.toInt();
//      Serial.print("key:");
//      Serial.println(keyInt);
//      Serial.print("value:");
//      Serial.println(value);
//      //delay(100);
//    }
//  }
//
//}

// 打印串口消息
void Task3(void * parameter) {
  while (true) {
    if (Serial1.available()) {
      while (Serial1.available()) {
        char c = Serial1.read();
        Serial.write(c);  // 将接收到的数据转发给PC
      }
    }

    delay(3000);
  }
}


/*
  - is_stand
   ---------------------------------------------------------------------------*/
bool is_stand(void)
{
  if (site_now[0][2] == z_default)
    return true;
  else
    return false;
}

/*
  - sit
  - blocking function
   ---------------------------------------------------------------------------*/
void sit(void)
{
  move_speed = stand_seat_speed;
  for (int leg = 0; leg < 4; leg++)
  {
    set_site(leg, KEEP, KEEP, z_boot);
  }
  wait_all_reach();
}

/*
  - stand
  - blocking function
   ---------------------------------------------------------------------------*/
void stand(void)
{
  move_speed = stand_seat_speed;
  for (int leg = 0; leg < 4; leg++)
  {
    set_site(leg, KEEP, KEEP, z_default);
  }
  wait_all_reach();
}


/*
  - Body init
  - blocking function
   ---------------------------------------------------------------------------*/
void b_init(void)
{
  //stand();
  set_site(0, x_default, y_default, z_default);
  set_site(1, x_default, y_default, z_default);
  set_site(2, x_default, y_default, z_default);
  set_site(3, x_default, y_default, z_default);
  wait_all_reach();
}


/*
  - spot turn to left
  - blocking function
  - parameter step steps wanted to turn
   ---------------------------------------------------------------------------*/
void turn_left(unsigned int step)
{
  move_speed = spot_turn_speed;
  while (step-- > 0)
  {
    if (site_now[3][1] == y_start)
    {
      //leg 3&1 move
      set_site(3, x_default + x_offset, y_start, z_up);
      wait_all_reach();

      set_site(0, turn_x1 - x_offset, turn_y1, z_default);
      set_site(1, turn_x0 - x_offset, turn_y0, z_default);
      set_site(2, turn_x1 + x_offset, turn_y1, z_default);
      set_site(3, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();

      set_site(3, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();

      set_site(0, turn_x1 + x_offset, turn_y1, z_default);
      set_site(1, turn_x0 + x_offset, turn_y0, z_default);
      set_site(2, turn_x1 - x_offset, turn_y1, z_default);
      set_site(3, turn_x0 - x_offset, turn_y0, z_default);
      wait_all_reach();

      set_site(1, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();

      set_site(0, x_default + x_offset, y_start, z_default);
      set_site(1, x_default + x_offset, y_start, z_up);
      set_site(2, x_default - x_offset, y_start + y_step, z_default);
      set_site(3, x_default - x_offset, y_start + y_step, z_default);
      wait_all_reach();

      set_site(1, x_default + x_offset, y_start, z_default);
      wait_all_reach();
    }
    else
    {
      //leg 0&2 move
      set_site(0, x_default + x_offset, y_start, z_up);
      wait_all_reach();

      set_site(0, turn_x0 + x_offset, turn_y0, z_up);
      set_site(1, turn_x1 + x_offset, turn_y1, z_default);
      set_site(2, turn_x0 - x_offset, turn_y0, z_default);
      set_site(3, turn_x1 - x_offset, turn_y1, z_default);
      wait_all_reach();

      set_site(0, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();

      set_site(0, turn_x0 - x_offset, turn_y0, z_default);
      set_site(1, turn_x1 - x_offset, turn_y1, z_default);
      set_site(2, turn_x0 + x_offset, turn_y0, z_default);
      set_site(3, turn_x1 + x_offset, turn_y1, z_default);
      wait_all_reach();

      set_site(2, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();

      set_site(0, x_default - x_offset, y_start + y_step, z_default);
      set_site(1, x_default - x_offset, y_start + y_step, z_default);
      set_site(2, x_default + x_offset, y_start, z_up);
      set_site(3, x_default + x_offset, y_start, z_default);
      wait_all_reach();

      set_site(2, x_default + x_offset, y_start, z_default);
      wait_all_reach();
    }
  }
}

/*
  - spot turn to right
  - blocking function
  - parameter step steps wanted to turn
   ---------------------------------------------------------------------------*/
void turn_right(unsigned int step)
{
  move_speed = spot_turn_speed;
  while (step-- > 0)
  {
    if (site_now[2][1] == y_start)
    {
      //leg 2&0 move
      set_site(2, x_default + x_offset, y_start, z_up);
      wait_all_reach();

      set_site(0, turn_x0 - x_offset, turn_y0, z_default);
      set_site(1, turn_x1 - x_offset, turn_y1, z_default);
      set_site(2, turn_x0 + x_offset, turn_y0, z_up);
      set_site(3, turn_x1 + x_offset, turn_y1, z_default);
      wait_all_reach();

      set_site(2, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();

      set_site(0, turn_x0 + x_offset, turn_y0, z_default);
      set_site(1, turn_x1 + x_offset, turn_y1, z_default);
      set_site(2, turn_x0 - x_offset, turn_y0, z_default);
      set_site(3, turn_x1 - x_offset, turn_y1, z_default);
      wait_all_reach();

      set_site(0, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();

      set_site(0, x_default + x_offset, y_start, z_up);
      set_site(1, x_default + x_offset, y_start, z_default);
      set_site(2, x_default - x_offset, y_start + y_step, z_default);
      set_site(3, x_default - x_offset, y_start + y_step, z_default);
      wait_all_reach();

      set_site(0, x_default + x_offset, y_start, z_default);
      wait_all_reach();
    }
    else
    {
      //leg 1&3 move
      set_site(1, x_default + x_offset, y_start, z_up);
      wait_all_reach();

      set_site(0, turn_x1 + x_offset, turn_y1, z_default);
      set_site(1, turn_x0 + x_offset, turn_y0, z_up);
      set_site(2, turn_x1 - x_offset, turn_y1, z_default);
      set_site(3, turn_x0 - x_offset, turn_y0, z_default);
      wait_all_reach();

      set_site(1, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();

      set_site(0, turn_x1 - x_offset, turn_y1, z_default);
      set_site(1, turn_x0 - x_offset, turn_y0, z_default);
      set_site(2, turn_x1 + x_offset, turn_y1, z_default);
      set_site(3, turn_x0 + x_offset, turn_y0, z_default);
      wait_all_reach();

      set_site(3, turn_x0 + x_offset, turn_y0, z_up);
      wait_all_reach();

      set_site(0, x_default - x_offset, y_start + y_step, z_default);
      set_site(1, x_default - x_offset, y_start + y_step, z_default);
      set_site(2, x_default + x_offset, y_start, z_default);
      set_site(3, x_default + x_offset, y_start, z_up);
      wait_all_reach();

      set_site(3, x_default + x_offset, y_start, z_default);
      wait_all_reach();
    }
  }
}

/*
  - go forward
  - blocking function
  - parameter step steps wanted to go
   ---------------------------------------------------------------------------*/
void step_forward(unsigned int step)
{
  move_speed = leg_move_speed;
  while (step-- > 0)
  {
    Serial.println(step);
    if (site_now[2][1] == y_start)
    {
      //leg 2&1 move
      set_site(2, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      set_site(2, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      set_site(2, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();

      move_speed = body_move_speed;

      set_site(0, x_default + x_offset, y_start, z_default);
      set_site(1, x_default + x_offset, y_start + 2 * y_step, z_default);
      set_site(2, x_default - x_offset, y_start + y_step, z_default);
      set_site(3, x_default - x_offset, y_start + y_step, z_default);
      wait_all_reach();

      move_speed = leg_move_speed;

      set_site(1, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      set_site(1, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      set_site(1, x_default + x_offset, y_start, z_default);
      wait_all_reach();
    }
    else
    {
      //leg 0&3 move
      set_site(0, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      set_site(0, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      set_site(0, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();

      move_speed = body_move_speed;

      set_site(0, x_default - x_offset, y_start + y_step, z_default);
      set_site(1, x_default - x_offset, y_start + y_step, z_default);
      set_site(2, x_default + x_offset, y_start, z_default);
      set_site(3, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();

      move_speed = leg_move_speed;

      set_site(3, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      set_site(3, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      set_site(3, x_default + x_offset, y_start, z_default);
      wait_all_reach();
    }
  }
}

/*
  - go back
  - blocking function
  - parameter step steps wanted to go
   ---------------------------------------------------------------------------*/
void step_back(unsigned int step)
{
  move_speed = leg_move_speed;
  while (step-- > 0)
  {
    if (site_now[3][1] == y_start)
    {
      //leg 3&0 move
      set_site(3, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      set_site(3, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      set_site(3, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();

      move_speed = body_move_speed;

      set_site(0, x_default + x_offset, y_start + 2 * y_step, z_default);
      set_site(1, x_default + x_offset, y_start, z_default);
      set_site(2, x_default - x_offset, y_start + y_step, z_default);
      set_site(3, x_default - x_offset, y_start + y_step, z_default);
      wait_all_reach();

      move_speed = leg_move_speed;

      set_site(0, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      set_site(0, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      set_site(0, x_default + x_offset, y_start, z_default);
      wait_all_reach();
    }
    else
    {
      //leg 1&2 move
      set_site(1, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      set_site(1, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      set_site(1, x_default + x_offset, y_start + 2 * y_step, z_default);
      wait_all_reach();

      move_speed = body_move_speed;

      set_site(0, x_default - x_offset, y_start + y_step, z_default);
      set_site(1, x_default - x_offset, y_start + y_step, z_default);
      set_site(2, x_default + x_offset, y_start + 2 * y_step, z_default);
      set_site(3, x_default + x_offset, y_start, z_default);
      wait_all_reach();

      move_speed = leg_move_speed;

      set_site(2, x_default + x_offset, y_start + 2 * y_step, z_up);
      wait_all_reach();
      set_site(2, x_default + x_offset, y_start, z_up);
      wait_all_reach();
      set_site(2, x_default + x_offset, y_start, z_default);
      wait_all_reach();
    }
  }
}

// add by RegisHsu

void body_left(int i)
{
  set_site(0, site_now[0][0] + i, KEEP, KEEP);
  set_site(1, site_now[1][0] + i, KEEP, KEEP);
  set_site(2, site_now[2][0] - i, KEEP, KEEP);
  set_site(3, site_now[3][0] - i, KEEP, KEEP);
  wait_all_reach();
}

void body_right(int i)
{
  set_site(0, site_now[0][0] - i, KEEP, KEEP);
  set_site(1, site_now[1][0] - i, KEEP, KEEP);
  set_site(2, site_now[2][0] + i, KEEP, KEEP);
  set_site(3, site_now[3][0] + i, KEEP, KEEP);
  wait_all_reach();
}

void hand_wave(int i)
{
  float x_tmp;
  float y_tmp;
  float z_tmp;
  move_speed = 1;
  if (site_now[3][1] == y_start)
  {
    body_right(15);
    x_tmp = site_now[2][0];
    y_tmp = site_now[2][1];
    z_tmp = site_now[2][2];
    move_speed = body_move_speed;
    for (int j = 0; j < i; j++)
    {
      set_site(2, turn_x1, turn_y1, 50);
      wait_all_reach();
      set_site(2, turn_x0, turn_y0, 50);
      wait_all_reach();
    }
    set_site(2, x_tmp, y_tmp, z_tmp);
    wait_all_reach();
    move_speed = 1;
    body_left(15);
  }
  else
  {
    body_left(15);
    x_tmp = site_now[0][0];
    y_tmp = site_now[0][1];
    z_tmp = site_now[0][2];
    move_speed = body_move_speed;
    for (int j = 0; j < i; j++)
    {
      set_site(0, turn_x1, turn_y1, 50);
      wait_all_reach();
      set_site(0, turn_x0, turn_y0, 50);
      wait_all_reach();
    }
    set_site(0, x_tmp, y_tmp, z_tmp);
    wait_all_reach();
    move_speed = 1;
    body_right(15);
  }
}

void hand_shake(int i)
{
  float x_tmp;
  float y_tmp;
  float z_tmp;
  move_speed = 1;
  if (site_now[3][1] == y_start)
  {
    body_right(15);
    x_tmp = site_now[2][0];
    y_tmp = site_now[2][1];
    z_tmp = site_now[2][2];
    move_speed = body_move_speed;
    for (int j = 0; j < i; j++)
    {
      set_site(2, x_default - 30, y_start + 2 * y_step, 55);
      wait_all_reach();
      set_site(2, x_default - 30, y_start + 2 * y_step, 10);
      wait_all_reach();
    }
    set_site(2, x_tmp, y_tmp, z_tmp);
    wait_all_reach();
    move_speed = 1;
    body_left(15);
  }
  else
  {
    body_left(15);
    x_tmp = site_now[0][0];
    y_tmp = site_now[0][1];
    z_tmp = site_now[0][2];
    move_speed = body_move_speed;
    for (int j = 0; j < i; j++)
    {
      set_site(0, x_default - 30, y_start + 2 * y_step, 55);
      wait_all_reach();
      set_site(0, x_default - 30, y_start + 2 * y_step, 10);
      wait_all_reach();
    }
    set_site(0, x_tmp, y_tmp, z_tmp);
    wait_all_reach();
    move_speed = 1;
    body_right(15);
  }
}

void head_up(int i)
{
  set_site(0, KEEP, KEEP, site_now[0][2] - i);
  set_site(1, KEEP, KEEP, site_now[1][2] + i);
  set_site(2, KEEP, KEEP, site_now[2][2] - i);
  set_site(3, KEEP, KEEP, site_now[3][2] + i);
  wait_all_reach();
}

void head_down(int i)
{
  set_site(0, KEEP, KEEP, site_now[0][2] + i);
  set_site(1, KEEP, KEEP, site_now[1][2] - i);
  set_site(2, KEEP, KEEP, site_now[2][2] + i);
  set_site(3, KEEP, KEEP, site_now[3][2] - i);
  wait_all_reach();
}

void body_dance(int i)
{
  float x_tmp;
  float y_tmp;
  float z_tmp;
  float body_dance_speed = 2;
  sit();
  move_speed = 1;
  set_site(0, x_default, y_default, KEEP);
  set_site(1, x_default, y_default, KEEP);
  set_site(2, x_default, y_default, KEEP);
  set_site(3, x_default, y_default, KEEP);
  wait_all_reach();
  //stand();
  set_site(0, x_default, y_default, z_default - 20);
  set_site(1, x_default, y_default, z_default - 20);
  set_site(2, x_default, y_default, z_default - 20);
  set_site(3, x_default, y_default, z_default - 20);
  wait_all_reach();
  move_speed = body_dance_speed;
  head_up(30);
  for (int j = 0; j < i; j++)
  {
    if (j > i / 4)
      move_speed = body_dance_speed * 2;
    if (j > i / 2)
      move_speed = body_dance_speed * 3;
    set_site(0, KEEP, y_default - 20, KEEP);
    set_site(1, KEEP, y_default + 20, KEEP);
    set_site(2, KEEP, y_default - 20, KEEP);
    set_site(3, KEEP, y_default + 20, KEEP);
    wait_all_reach();
    set_site(0, KEEP, y_default + 20, KEEP);
    set_site(1, KEEP, y_default - 20, KEEP);
    set_site(2, KEEP, y_default + 20, KEEP);
    set_site(3, KEEP, y_default - 20, KEEP);
    wait_all_reach();
  }
  move_speed = body_dance_speed;
  head_down(30);
}


/*
  - microservos service /timer interrupt function/50Hz
  - when set site expected,this function move the end point to it in a straight line
  - temp_speed[4][3] should be set before set expect site,it make sure the end point
   move in a straight line,and decide move speed.
   ---------------------------------------------------------------------------*/
void servo_service(void)
{
  //Serial.print("abc:");
  //Serial.print(abc);
  static float alpha, beta, gamma;

  for (int i = 0; i < 4; i++)
  {
    for (int j = 0; j < 3; j++)
    {
      if (abs(site_now[i][j] - site_expect[i][j]) >= abs(temp_speed[i][j]))
        site_now[i][j] += temp_speed[i][j];
      else
        site_now[i][j] = site_expect[i][j];
    }

    cartesian_to_polar(alpha, beta, gamma, site_now[i][0], site_now[i][1], site_now[i][2]);
    polar_to_servo(i, alpha, beta, gamma);
  }

  rest_counter++;
  vTaskDelay(10);
}

/*
  - set one of end points' expect site
  - this founction will set temp_speed[4][3] at same time
  - non - blocking function
   ---------------------------------------------------------------------------*/
void set_site(int leg, float x, float y, float z)
{
  float length_x = 0, length_y = 0, length_z = 0;

  if (x != KEEP)
    length_x = x - site_now[leg][0];
  if (y != KEEP)
    length_y = y - site_now[leg][1];
  if (z != KEEP)
    length_z = z - site_now[leg][2];

  float length = sqrt(pow(length_x, 2) + pow(length_y, 2) + pow(length_z, 2));

  temp_speed[leg][0] = length_x / length * move_speed * speed_multiple;
  temp_speed[leg][1] = length_y / length * move_speed * speed_multiple;
  temp_speed[leg][2] = length_z / length * move_speed * speed_multiple;

  if (x != KEEP)
    site_expect[leg][0] = x;
  if (y != KEEP)
    site_expect[leg][1] = y;
  if (z != KEEP)
    site_expect[leg][2] = z;
}

/*
  - wait one of end points move to expect site
  - blocking function
   ---------------------------------------------------------------------------*/
void wait_reach(int leg)
{
  while (1)
  {
    if (site_now[leg][0] == site_expect[leg][0])
      if (site_now[leg][1] == site_expect[leg][1])
        if (site_now[leg][2] == site_expect[leg][2])
          break;

    vTaskDelay(1);
  }
}

/*
  - wait all of end points move to expect site
  - blocking function
   ---------------------------------------------------------------------------*/
void wait_all_reach(void)
{
  for (int i = 0; i < 4; i++)
    wait_reach(i);
}

/*
  - trans site from cartesian to polar
  - mathematical model 2/2
   ---------------------------------------------------------------------------*/
void cartesian_to_polar(volatile float &alpha, volatile float &beta, volatile float &gamma, volatile float x, volatile float y, volatile float z)
{
  //calculate w-z degree
  float v, w;
  w = (x >= 0 ? 1 : -1) * (sqrt(pow(x, 2) + pow(y, 2)));
  v = w - length_c;
  alpha = atan2(z, v) + acos((pow(length_a, 2) - pow(length_b, 2) + pow(v, 2) + pow(z, 2)) / 2 / length_a / sqrt(pow(v, 2) + pow(z, 2)));
  beta = acos((pow(length_a, 2) + pow(length_b, 2) - pow(v, 2) - pow(z, 2)) / 2 / length_a / length_b);
  //calculate x-y-z degree
  gamma = (w >= 0) ? atan2(y, x) : atan2(-y, -x);
  //trans degree pi->180
  alpha = alpha / pi * 180;
  beta = beta / pi * 180;
  gamma = gamma / pi * 180;
}

///*
//  - trans site from polar to microservos
//  - mathematical model map to fact
//  - the errors saved in eeprom will be add
//   ---------------------------------------------------------------------------*/
//void polar_to_servo(int leg, float alpha, float beta, float gamma)
//{
//	if (leg == 0)
//  {
//    alpha = 90 - alpha;
//    beta = beta;
//    gamma += 90;
//  }
//  else if (leg == 1)
//  {
//    alpha += 90;
//    beta = 180 - beta;
//    gamma = 90 - gamma;
//  }
//  else if (leg == 2)
//  {
//    alpha += 90;
//    beta = 180 - beta;
//    gamma = 90 - gamma;
//  }
//  else if (leg == 3)
//  {
//    alpha = 90 - alpha;
//    beta = beta;
//    gamma += 90;
//  }
//
//	servo[leg][0].write(alpha+offset[leg][0]);
//	servo[leg][1].write(beta+offset[leg][1]);
//	servo[leg][2].write(gamma+offset[leg][2]);
//}

/*
  - trans site from polar to microservos
  - mathematical model map to fact
  - the errors saved in eeprom will be add
   ---------------------------------------------------------------------------*/
void polar_to_servo(int leg, float alpha, float beta, float gamma)
{
    if (leg == 0) //Front Right
    {
        alpha = 90 - alpha - FRElbow; //elbow (- is up)
        beta = beta - FRFoot; //foot (- is up)
        gamma = 90 - gamma - FRShdr;    // shoulder (- is left)
    }
    else if (leg == 1) //Rear Right
    {
        alpha += 90 + RRElbow; //elbow (+ is up)
        beta = 180 - beta + RRFoot; //foot (+ is up)
        gamma = 90 + gamma + RRShdr; // shoulder (+ is left)
    }
    else if (leg == 2) //Front Left
    {
        alpha += 90 + FLElbow; //elbow (+ is up)
        beta = 180 - beta + FLFoot; //foot (+ is up)
        gamma = 90 + gamma + FLShdr;// shoulder (+ is left)
    }
    else if (leg == 3) // Rear Left
    {
        alpha = 90 - alpha - RLElbow; //elbow (- is up)
        beta = beta - RLFoot; //foot; (- is up)
        gamma = 90 - gamma - RLShdr;// shoulder (- is left)
    }
    //  int AL = ((850/180)*alpha);if (AL > 580) AL=580;if (AL < 125) AL=125;pwm.setPWM(servo_pin[leg][0],0,AL);
    //  int BE = ((850/180)*beta);if (BE > 580) BE=580;if (BE < 125) BE=125;pwm.setPWM(servo_pin[leg][1],0,BE);
    //  int GA = ((580/180)*gamma);if (GA > 580) GA=580;if (GA < 125) GA=125;pwm.setPWM(servo_pin[leg][2],0,GA);

    servo[leg][0].write(alpha+offset[leg][0]);
    servo[leg][1].write(beta+offset[leg][1]);
    servo[leg][2].write(gamma+offset[leg][2]);
}
