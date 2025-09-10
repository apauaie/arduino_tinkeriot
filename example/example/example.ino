#include <ESP32Servo.h>
#include <MakerKit.h>

#define TINKERCODE_AUTH_TOKEN "f1e5f43529184bd9a2ccececdf802a0c"


#define TINKERIOT_PRINT Serial


#include <WiFi.h>
#include <TinkerIoT.h>
char ssid[] = "Bare with me";
char pass[] = "20051130";
#include <Adafruit_NeoPixel.h>
 Adafruit_NeoPixel pixels = Adafruit_NeoPixel(5, 4, NEO_GRB + NEO_KHZ800);

TINKERIOT_WRITE(C0)
{
int pinValue = param.asInt();
  if (pinValue == 1) {
    pixels.setPixelColor(0, 0xffffff);
    pixels.setPixelColor(1, 0xffffff);
    pixels.setPixelColor(2, 0xffffff);
    pixels.setPixelColor(3, 0xffffff);
    pixels.setPixelColor(4, 0xffffff);
    pixels.show();
    pixels.show();

  } else {
    pixels.setPixelColor(0, 0x000000);
    pixels.setPixelColor(1, 0x000000);
    pixels.setPixelColor(2, 0x000000);
    pixels.setPixelColor(3, 0x000000);
    pixels.setPixelColor(4, 0x000000);
    pixels.show();
    pixels.show();

  }
}

int speedee;
 ESP32PWM pwm1;
ESP32PWM pwm2;

TINKERIOT_WRITE(C1)
{
int pinValue = param.asInt();
  if (pinValue == 1) {
    digitalWrite(13,HIGH);
    digitalWrite(14,LOW);
    speedee=round((100 / 100.0) * 255);
    pwm1.write(speedee);

  } else {
    digitalWrite(13,HIGH);
    digitalWrite(14,LOW);
    speedee=round((0 / 100.0) * 255);
    pwm1.write(speedee);

  }
}

Servo servo_5;
TINKERIOT_WRITE(C2)
{
int pinValue = param.asInt();
  servo_5.write(pinValue);
}

dht11 DHT11;
TinkerIoTTimer Timer1;

void Timer1_TimerEvent()
{
  TinkerIoT.cloudWrite(C3, readDHT(26,1));
  delay(100);
}


int readDHT(int Pin, int Stat)
{
  DHT11.read(Pin);
  if (Stat == 1) {return DHT11.humidity;}
  else {return DHT11.temperature;}
}



void setup() {
  Serial.begin(9600);
  TinkerIoT.begin(TINKERCODE_AUTH_TOKEN, ssid, pass,"10.132.223.100",8008);
  pixels.begin();

  ESP32PWM::allocateTimer(0);
 ESP32PWM::allocateTimer(1);
 ESP32PWM::allocateTimer(2);
 ESP32PWM::allocateTimer(3);
 pinMode(13,OUTPUT);
 pinMode(14,OUTPUT);
 pinMode(25,OUTPUT);
 pinMode(18,OUTPUT);
 pinMode(19,OUTPUT);
 pinMode(15,OUTPUT);
 pwm1.attachPin(25, 1000, 8);
 pwm2.attachPin(15, 1000, 8);

  servo_5.attach(5);
  Timer1.setInterval(1000, Timer1_TimerEvent);

}

void loop() {
  TinkerIoT.run();
  if (TinkerIoT.connected())
   {Timer1.run();
  }

}