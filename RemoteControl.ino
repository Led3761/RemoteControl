#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>
#include "icons.h"

//******************************************
//Инициализация кнопок
//******************************************
//Создаем LCD дисплей
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

enum Pages {StartPage, BoardChargePage, ConnectionPage, DrivingPage, GpsPage};

Pages currentPage = StartPage;

int currentSection = 0; //Номер текущей секции

unsigned long buttonPressedTime = 0;
unsigned long gpsConnectingAnimationTime = 0;

//******************************************
//Инициализация кнопок
//******************************************
//Входы для кнопок
const int upButton = 6;
const int downButton = 5;
const int leftButton = 4;
const int rightButton = 3;
const int centerButton = 2;

enum Button{Up, Down, Left, Right, Center, None};

//******************************************
//Инициализация RF24L01
//******************************************
bool boardConnected = false;

//******************************************
//Типы данных
//******************************************
enum DrivingMode{Extrime, Normal, Safe};

enum GpsState {GpsConnected, GpsConnecting, GpsDisconnected};

typedef struct {
  int commonCharge;
  float speed;
  GpsState gpsState;
  bool gpsTracking;
  DrivingMode drivingMode;  
}
BoardData;

typedef struct {
  int acceleration;
  int braking;
  bool gpsConnect;
  bool gpsStartTracking;
  DrivingMode drivingMode;  
}
RemoteControlData;

//******************************************
//Объявление глобальных переменных
//******************************************
BoardData boardData{100, 0.0, GpsConnected, false, Normal};
RemoteControlData remoteControlData{0, 0, false, false, Normal};

int remoteControlCharge = 100;

void setup() {
  Serial.begin(9600);

  //Инициализация дисплея
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setRotation(3);
  display.clearDisplay();  
  
  display.setTextSize(1);
  display.setTextColor(WHITE);

  lcdDrawBase();  

  //Инициализация входов для кнопок
  pinMode(upButton, INPUT);
  pinMode(downButton, INPUT);
  pinMode(leftButton, INPUT);
  pinMode(rightButton, INPUT);
  pinMode(centerButton, INPUT);

}

unsigned long changeTime = 0;

void loop() { 
  moveSelection(currentPage, readButtonState());

  display.clearDisplay();
  lcdDrawBase();
  lcdDrawPage(currentPage);
  lcdDrawSelection(currentPage, currentSection);
  display.display();

//ANIMATION START
  unsigned long dif = millis() - changeTime;
  if (dif > 1000) {
    changeTime = millis();
    remoteControlCharge = remoteControlCharge - 10;
    boardData.commonCharge = boardData.commonCharge - 10;
    boardConnected = !boardConnected;
    boardData.speed = boardData.speed + 1.25;
    remoteControlData.acceleration = remoteControlData.acceleration + 1;
    remoteControlData.braking = remoteControlData.braking +1;

    if (boardData.gpsState == GpsDisconnected) {
      boardData.gpsTracking = !boardData.gpsTracking;
    }
    
    if (boardData.gpsState == GpsDisconnected) {
      boardData.gpsState = GpsConnecting;
    }
    else if (boardData.gpsState == GpsConnecting) {
      boardData.gpsState = GpsConnected;
    }
    else if (boardData.gpsState == GpsConnected) {
      boardData.gpsState = GpsDisconnected;
    }

    if (remoteControlData.drivingMode == Normal) {
      remoteControlData.drivingMode = Extrime;
    }
    else if (remoteControlData.drivingMode == Extrime) {
      remoteControlData.drivingMode = Safe;
    }
    else if (remoteControlData.drivingMode == Safe) {
      remoteControlData.drivingMode = Normal;
    }
  }

  if (remoteControlCharge < 0) {
    remoteControlCharge = 100;
  }
  if (boardData.commonCharge < 0) {
    boardData.commonCharge = 100;
  }
  if (boardData.speed > 99.9) {
     boardData.speed = 0.0;
  }
  if (remoteControlData.acceleration > 3) {
    remoteControlData.acceleration = 0;
  }
  if (remoteControlData.braking > 2) {
    remoteControlData.braking = 0;
  }
  //ANIMATION STOP
}

void lcdDrawPage(Pages page) {
  switch(page) {
    case StartPage:
      lcdDrawStartPage();
      break;
  }  
}

void lcdDrawBase() {
  //Section 0 Common
  //********************************************************
  //BoardConnection
  //********************************************************
  if (boardConnected) {
    display.drawBitmap(0, 0, connected_16x16, 16, 16, 1);
  }
  else {
    display.drawBitmap(0, 0, disconnected_16x16, 16, 16, 1);
  }
  //********************************************************
  //RemoteCharge
  //********************************************************
  if (remoteControlCharge <= 10) {
    display.drawBitmap(16, 0, battery0_16x16, 16, 16, 1);
  }
  else if (remoteControlCharge <= 25) {
    display.drawBitmap(16, 0, battery25_16x16, 16, 16, 1);
  }
  else if (remoteControlCharge <= 50) {
    display.drawBitmap(16, 0, battery50_16x16, 16, 16, 1);
  }
  else if (remoteControlCharge <= 75) {
    display.drawBitmap(16, 0, battery75_16x16, 16, 16, 1);
  }
  else {
    display.drawBitmap(16, 0, battery100_16x16, 16, 16, 1);
  }
  //********************************************************
  //Speed
  //********************************************************
  if (boardData.speed < 10) {
    display.setCursor(5,20);
  }
  else {
    display.setCursor(2,20);
  }
  display.println(String(boardData.speed));
  display.setCursor(5,30);
  display.println("km/h");
  //********************************************************
  //Direction
  //********************************************************
  if (remoteControlData.acceleration > 0) {
    display.drawBitmap(0, 37, moveForward_16x16, 16, 16, 1);
  }
  if (remoteControlData.braking > 0) {
    display.drawBitmap(16, 37, moveBackward_16x16, 16, 16, 1);
  }
}

void lcdDrawStartPage() {
  //********************************************************
  //Section 1 GPS
  //********************************************************
  display.drawLine(2,53,29,53,1);
  
  display.setCursor(8, 57);
  display.println("GPS");
  //********************************************************
  //Route state
  //********************************************************
  int gpsOffset = 8;
  if (boardData.gpsTracking) {
    gpsOffset = 0;
    display.drawBitmap(16, 67, route_16x16, 16, 16, 1);
  }

  //********************************************************
  //GPS state
  //********************************************************
  switch(boardData.gpsState) {
    case GpsConnected:
      display.drawBitmap(gpsOffset, 67, gpsConnected_16x16, 16, 16, 1);
      break;
    case GpsDisconnected:
      display.drawBitmap(gpsOffset, 67, gpsDisconnected_16x16, 16, 16, 1);
      break;
    case GpsConnecting:
      unsigned long diff = millis() - gpsConnectingAnimationTime;
      if (diff < 250) {
        display.drawBitmap(gpsOffset, 67, gpsConnecting1_16x16, 16, 16, 1);
      }
      else if (diff < 500) {
        display.drawBitmap(gpsOffset, 67, gpsConnecting2_16x16, 16, 16, 1);
      }
      else if (diff < 750) {
        display.drawBitmap(gpsOffset, 67, gpsConnecting3_16x16, 16, 16, 1);
      }
      else if (diff < 1000) {
        display.drawBitmap(gpsOffset, 67, gpsConnecting4_16x16, 16, 16, 1);        
      }
      else {
        gpsConnectingAnimationTime = millis();
      }
      break;
  }
  //********************************************************
  //Section 2 MODE
  //********************************************************
  display.drawLine(2,83,29,83,1);

  display.setCursor(5, 92);
  switch(remoteControlData.drivingMode) {
    case Normal:
      display.println("Norm");
      break;
    case Extrime:
      display.println("Ext.");
      break;
    case Safe:
      display.println("Safe");
      break;
  }

  display.drawLine(2,107,29,107,1);
  //********************************************************
  //Section 3 Board Charge
  //********************************************************
  if (boardData.commonCharge <= 10) {
    display.drawBitmap(0, 110, battery0_32x16, 32, 16, 1);
  }
  else if (boardData.commonCharge <= 25) {
    display.drawBitmap(0, 110, battery25_32x16, 32, 16, 1);
  }
  else if (boardData.commonCharge <= 50) {
    display.drawBitmap(0, 110, battery50_32x16, 32, 16, 1);
  }
  else if (boardData.commonCharge <= 75) {
    display.drawBitmap(0, 110, battery75_32x16, 32, 16, 1);
  }
  else {
    display.drawBitmap(0, 110, battery100_32x16, 32, 16, 1);
  }
}

void moveSelection(Pages page, Button but) {
  switch(but) {
    case Up:
      currentSection++;
      break;
    case Down:
      currentSection--;
      break;
  }

  switch (page) {
    case StartPage:
    if (currentSection > 2) {
      currentSection = 0;
    }
    else if (currentSection < 0) {
      currentSection = 2;
    }
    break;
  }
}

void lcdDrawSelection(Pages page, int sectionNumber) {

  switch (page) {
    case StartPage: 
      switch(sectionNumber) {
      case 0:
        display.drawRect(0, 53, 32, 31, 1);
        break;
      case 1:
        display.drawRect(0, 83, 32 , 25, 1);
        break;
      case 2:
        display.drawRect(0, 107, 32 , 20, 1);
        break;
      }
    break;
  }
}

Button readButtonState() {
  unsigned long dif = millis() - buttonPressedTime;
  if (dif < 100) {
    return None;
  }
  if (digitalRead(upButton)) {
    buttonPressedTime = millis();
    return Up;
  }
//  else if (digitalRead(downButton)) {
//    buttonPressedTime = millis();
//    return Down;
//  }
//  else if (digitalRead(leftButton)) {
//    buttonPressedTime = millis();
//    return Left;
//  }
//  else if (digitalRead(rightButton)) {
//    buttonPressedTime = millis();
//    return Right;
//  }
//  else if (digitalRead(centerButton)) {
//    buttonPressedTime = millis();
//    return Center;
//  }
  else {
    return None;
  }
}
