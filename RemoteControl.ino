#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

#include "nRF24L01.h"
#include "RF24.h"

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

const int analogZeroMark = 558;
const int analogDeadZone = 3;

//******************************************
//Инициализация RF24L01
//******************************************
RF24 radio(9,10);  // make sure this corresponds to the pins you are using
const uint64_t pipes[2] = {0xF0F0F0F0E1LL, 0xF0F0F0F0D2LL };
unsigned long replyStartedWaitingAt = 0;

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
//Контроль заряда батареи пульта
//******************************************
enum BatteryState{Charging, Charged, InUse};

typedef struct {
  BatteryState state;
  int charge; 
}
RemoteControlBatteryData;

const int batteryStdBy = 7;
const int batteryCharg = 8;

//******************************************
//Объявление глобальных переменных
//******************************************
BoardData boardData{100, 0.0, GpsDisconnected, false, Normal};
RemoteControlData remoteControlData{0, 0, false, false, Normal};
RemoteControlBatteryData remoteControlBatteryData{InUse, 0};

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
  digitalWrite(upButton, HIGH);
  pinMode(downButton, INPUT);
  digitalWrite(downButton, HIGH);
  pinMode(leftButton, INPUT);
  digitalWrite(leftButton, HIGH);
  pinMode(rightButton, INPUT);
  digitalWrite(rightButton, HIGH);
  pinMode(centerButton, INPUT);
  digitalWrite(centerButton, HIGH);

  //Инициализация входов для состояния зарядки батареи
  pinMode(batteryStdBy, INPUT);
  digitalWrite(batteryStdBy, HIGH);
  pinMode(batteryCharg, INPUT);
  digitalWrite(batteryCharg, HIGH);
	
	//Инициализация радио модуля
	radio.begin();
  radio.openWritingPipe(pipes[0]);
  radio.openReadingPipe(1,pipes[1]);


}

unsigned long changeTime = 0;

void loop() { 
  processRemoteBatteryState();
  processAnalogInput();

	radioTransmittReceive();
	
  moveSelection(readButtonState());

  display.clearDisplay();
  lcdDrawBase();
  lcdDrawPage(currentPage);
  lcdDrawSelection(currentPage, currentSection);
  display.display();  
}

void processRemoteBatteryState() {
  int remoteCharge = analogRead(A1);
  float remoteChargeVoltage = (float)remoteCharge * 5. / 1024.;
  remoteControlBatteryData.charge = (remoteChargeVoltage - 3.0)/1.2*100;

  if (!digitalRead(batteryCharg)) {
    remoteControlBatteryData.state = Charged;
  }
  else if (!digitalRead(batteryStdBy)) {
    remoteControlBatteryData.state = Charging;
  }
  else {
    remoteControlBatteryData.state = InUse;
  }
}

void processAnalogInput() {
  int analogIn = analogRead(A0);
  int analogSpeed = analogIn - analogZeroMark;

  if (analogSpeed > 0) {
    if (analogSpeed < analogDeadZone) {
      analogSpeed = 0;
    }
    remoteControlData.acceleration = int((float(analogSpeed)/float(1024-analogZeroMark - analogDeadZone))*100);
    remoteControlData.braking = 0;
  }
  else {
    if (abs(analogSpeed) < analogDeadZone) {
      analogSpeed = 0;
    }
    remoteControlData.acceleration = 0;
    remoteControlData.braking = int(float(abs(analogSpeed))/float(analogZeroMark - analogDeadZone)*100);
  }
}

void radioTransmittReceive() {
	radio.stopListening();
  bool ok = radio.write( &remoteControlData, 
												 sizeof(remoteControlData) );
  radio.startListening();
 
  if (radio.available()) {
    radio.read( &boardData, sizeof(boardData) );
    replyStartedWaitingAt = millis();
    boardConnected = true;
  }
  else if (millis() - replyStartedWaitingAt > 250) {
    Serial.println("Failed, response timed out.");
    replyStartedWaitingAt = millis();
    boardConnected = false;
  }

//  Serial.print("Acceleration: ");
//  Serial.println(remoteControlData.acceleration);
//  Serial.print("Braking: ");
//  Serial.println(remoteControlData.braking);
//  Serial.print("Gps Connect: ");
//  Serial.println(remoteControlData.gpsConnect);
//  Serial.print("Gps Start tracking: ");
//  Serial.println(remoteControlData.gpsStartTracking);
//  Serial.print("Driving Mode: ");
//  Serial.println(remoteControlData.drivingMode);
}

Button readButtonState() {
  unsigned long dif = millis() - buttonPressedTime;
  if (dif < 200) {
    return None;
  }
  if (!digitalRead(upButton)) {
    buttonPressedTime = millis();
    return Up;
  }
  else if (!digitalRead(downButton)) {
    buttonPressedTime = millis();
    return Down;
  }
  else if (!digitalRead(leftButton)) {
    buttonPressedTime = millis();
    return Left;
  }
  else if (!digitalRead(rightButton)) {
    buttonPressedTime = millis();
    return Right;
  }
  else if (!digitalRead(centerButton)) {
    buttonPressedTime = millis();
    return Center;
  }
  else {
    return None;
  }
}

void moveSelection(Button but) {
  switch(but) {
    case Up:
      currentSection--;
      break;
    case Down:
			currentSection++;
		break;
		case Left:
			if (currentPage != StartPage) {
				currentPage = StartPage;
				currentSection = 0;
			}
		break;
		case Center:
			switch(currentPage) {
        case StartPage:
          switch(currentSection) {
          case 0:
            currentPage = GpsPage;
          break;
          case 1:
            currentPage = DrivingPage;
          break;
          case 2:
            currentPage = BoardChargePage;
          break;
          }
          currentSection = 0;
        break;
				case GpsPage:
					if (currentSection == 0) {
						remoteControlData.gpsConnect = !remoteControlData.gpsConnect;
					}
					else if (currentSection == 1) {
						remoteControlData.gpsStartTracking = !remoteControlData.gpsStartTracking;
					}
				break;
				case DrivingPage:
					if (currentSection == 0) {
						remoteControlData.drivingMode = Normal;
					}
					else if (currentSection == 1) {
						remoteControlData.drivingMode = Extrime;
					}
					else if (currentSection == 2) {
						remoteControlData.drivingMode = Safe;
					}
				break;
			}
		break;
  }

  switch (currentPage) {
    case StartPage:
			if (currentSection > 2) {
				currentSection = 0;
			}
			else if (currentSection < 0) {
				currentSection = 2;
			}
			break;
		case GpsPage:
			if (currentSection > 1) {
				currentSection = 0;
			}
			else if (currentSection < 0) {
				currentSection = 1;
			}
			break;
		case DrivingPage:
			if (currentSection > 2) {
				currentSection = 0;
			}
			else if (currentSection < 0) {
				currentSection = 2;
			}
			break;
		case BoardChargePage:
			currentSection = 0;
			break;
  }
}

void lcdDrawPage(Pages page) {
  switch(page) {
    case StartPage:
      lcdDrawStartPage();
		break;
		case GpsPage:
      lcdDrawGpsPage();
		break;
		case DrivingPage:
      lcdDrawDrivingPage();
		break;
		case BoardChargePage:
      lcdDrawBatteryPage();
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
		//For other page selection draw in the page function
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
  if (remoteControlBatteryData.state == Charging) {
    display.drawBitmap(16, 0, batteryCharge_16x16, 16, 16, 1);
  }
  else if (remoteControlBatteryData.state == Charged) {
    display.drawBitmap(16, 0, batteryFullCharge_16x16, 16, 16, 1);
  }
  else {
    if (remoteControlBatteryData.charge <= 10) {
      display.drawBitmap(16, 0, battery0_16x16, 16, 16, 1);
    }
    else if (remoteControlBatteryData.charge <= 25) {
      display.drawBitmap(16, 0, battery25_16x16, 16, 16, 1);
    }
    else if (remoteControlBatteryData.charge <= 50) {
      display.drawBitmap(16, 0, battery50_16x16, 16, 16, 1);
    }
    else if (remoteControlBatteryData.charge <= 75) {
      display.drawBitmap(16, 0, battery75_16x16, 16, 16, 1);
    }
    else {
      display.drawBitmap(16, 0, battery100_16x16, 16, 16, 1);
    }
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
        display.drawBitmap(gpsOffset, 67, gpsConnecting1_16x16, 16, 16, 1);
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

void lcdDrawGpsPage() {
	//********************************************************
  //Section 0 GPS
  //********************************************************
  display.drawLine(2,53,29,53,1);
  
  display.setCursor(8, 57);
  display.println("GPS");
	//********************************************************
  //GPS state
  //********************************************************
  switch(boardData.gpsState) {
    case GpsConnected:
      display.drawBitmap(8, 67, gpsConnected_16x16, 16, 16, 1);
      break;
    case GpsDisconnected:
      display.drawBitmap(8, 67, gpsDisconnected_16x16, 16, 16, 1);
      break;
    case GpsConnecting:
      unsigned long diff = millis() - gpsConnectingAnimationTime;
      if (diff < 250) {
        display.drawBitmap(8, 67, gpsConnecting1_16x16, 16, 16, 1);
      }
      else if (diff < 500) {
        display.drawBitmap(8, 67, gpsConnecting2_16x16, 16, 16, 1);
      }
      else if (diff < 750) {
        display.drawBitmap(8, 67, gpsConnecting3_16x16, 16, 16, 1);
      }
      else if (diff < 1000) {
        display.drawBitmap(8, 67, gpsConnecting4_16x16, 16, 16, 1);        
      }
      else {
        gpsConnectingAnimationTime = millis();
        display.drawBitmap(8, 67, gpsConnecting1_16x16, 16, 16, 1);
      }
      break;
  }
	//********************************************************
  //GPS connect
  //********************************************************
	display.setCursor(5,86);
	
	if (currentSection == 0) {
		display.fillRect(3,84,27,11,1);
		display.setTextColor(0);
		if (remoteControlData.gpsConnect) {			
			display.println("dis.");
		}
		else {
			display.println("con.");
		}
		display.setTextColor(1);
	}
	else {
		if (remoteControlData.gpsConnect) {			
			display.println("dis.");
		}
		else {
			display.println("con.");
		}
	}
	//********************************************************
  //Section 1 Track
  //********************************************************
	display.drawLine(2,98,29,98,1);
	if (boardData.gpsTracking) {			
			display.drawBitmap(8, 99, route_16x16, 16, 16, 1);
		}
	else {
		display.drawBitmap(8, 99, noRoute_16x16, 16, 16, 1);
	}
	display.setCursor(5,118);
	if (currentSection == 1) {
		display.fillRect(3,116,27,11,1);
		display.setTextColor(0);
		if (remoteControlData.gpsStartTracking) {			
			display.println("stop");
		}
		else {
			display.println("strt");
		}
		display.setTextColor(1);
	}
	else {
		if (remoteControlData.gpsStartTracking) {			
			display.println("stop");
		}
		else {
			display.println("strt");
		}
		display.setTextColor(1);
	}
}

void lcdDrawDrivingPage() {
	//********************************************************
  //Section 0 Mode
  //********************************************************
  display.drawLine(2,53,29,53,1);
  
  display.setCursor(5, 57);
  display.println("MODE");
	
	display.setCursor(5, 71);
	switch (remoteControlData.drivingMode) {
		case Normal:
			display.println("norm");
		break;
		case Extrime:
			display.println("ext.");
		break;
		case Safe:
			display.println("safe");
		break;
	}
	
	//********************************************************
  //Section Switcher
  //********************************************************
	if (currentSection == 0) {		
		display.fillRect(3,86,27,11,1);
		display.setTextColor(0);		
		display.setCursor(5,88);
		display.println("norm");
		display.setTextColor(1);
		display.setCursor(5,103);
		display.println("ext.");
		display.setCursor(5,118);
		display.println("safe");
	}
	else if (currentSection == 1) {		
		display.fillRect(3,101,27,11,1);			
		display.setCursor(5,88);
		display.println("norm");		
		display.setCursor(5,103);
		display.setTextColor(0);
		display.println("ext.");
		display.setTextColor(1);
		display.setCursor(5,118);
		display.println("safe");
	}
	else {		
		display.fillRect(3,116,27,11,1);			
		display.setCursor(5,88);
		display.println("norm");		
		display.setCursor(5,103);		
		display.println("ext.");		
		display.setCursor(5,118);
		display.setTextColor(0);
		display.println("safe");
		display.setTextColor(1);
	}
}

void lcdDrawBatteryPage() {
	
}
