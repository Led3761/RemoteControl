#include <SPI.h>
#include <Wire.h>
#include <Adafruit_GFX.h>
#include <Adafruit_SSD1306.h>

//Создаем LCD Дисплей
#define OLED_RESET -1
Adafruit_SSD1306 display(OLED_RESET);

#if (SSD1306_LCDHEIGHT != 32)
#error("Height incorrect, please fix Adafruit_SSD1306.h!");
#endif

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

unsigned long buttonPressedTime = 0;

int currentSection = 0;

void setup() {
  Serial.begin(9600);

  //Инициализация дисплея
  display.begin(SSD1306_SWITCHCAPVCC, 0x3C);
  display.setRotation(3);
  display.clearDisplay();  

  lcdDrawBase();  

  //Инициализация входов для кнопок
  pinMode(upButton, INPUT);
  pinMode(downButton, INPUT);
  pinMode(leftButton, INPUT);
  pinMode(rightButton, INPUT);
  pinMode(centerButton, INPUT);

}

void loop() {
  // put your main code here, to run repeatedly:
  
  moveSelection(readButtonState());
}

void lcdDrawBase() {
  display.clearDisplay();
  display.drawRect(0, 0, 32 , 128, 1);
  display.display();

}

void moveSelection(Button but) {
  switch(but) {
    case Up:
      currentSection++;
      break;
    case Down:
      currentSection--;
      break;
  }

  if (currentSection > 4) {
    currentSection = 0;
  }
  else if (currentSection < 0) {
    currentSection = 4;
  }

  lcdDrawSelection(currentSection);
}

void lcdDrawSelection(int sectionNumber) {
  display.clearDisplay();
  switch(sectionNumber) {
    case 0:
      display.drawRect(0, 0, 32 , 28, 1);
      break;
    case 1:
      display.drawRect(0, 28, 32 , 25, 1);
      break;
    case 2:
      display.drawRect(0, 53, 32 , 25, 1);
      break;
    case 3:
      display.drawRect(0, 78, 32 , 25, 1);
      break;
    case 4:
      display.drawRect(0, 103, 32 , 25, 1);
      break;
  }
  display.display();
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
  else if (digitalRead(downButton)) {
    buttonPressedTime = millis();
    return Down;
  }
  else if (digitalRead(leftButton)) {
    buttonPressedTime = millis();
    return Left;
  }
  else if (digitalRead(rightButton)) {
    buttonPressedTime = millis();
    return Right;
  }
  else if (digitalRead(centerButton)) {
    buttonPressedTime = millis();
    return Center;
  }
  else {
    return None;
  }
}
