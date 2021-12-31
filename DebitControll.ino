#include <EEPROM.h>
#include "ST7032_asukiaaa.h"
#include "UBX_GPS.h"

ST7032_asukiaaa lcd;

int pinS1 = 5;
int pinS2 = 6;
int pinKey = 7;

int oldPos  = 0;
int pos  = 0;
int subpos = 0;

unsigned long lon;

unsigned int consumption = 0;
//float preasure = 0;
float speedms = 0; //m/s
float spent = 0.0;
float spentOld[2] = {0.0,0.0};
int spentIndex = 0;
float lpersec = 0;

float coef = 27; //27
float _coef = 0.037; //0.037
float width = 18;


int periods = 0;
void PinFalling() {
  spent += _coef;
}

void setup() {
  
  lcd.begin(16, 2); // LCD columns and rows.
  lcd.setContrast(40);
  lcd.print("Loading ...");

  Serial.begin(38400);
  
  attachInterrupt(digitalPinToInterrupt(2), PinFalling, FALLING); //CHANGE

  pinMode(4, OUTPUT);

  if(EEPROM.length()<=0) {
    int address = 0;
    EEPROM.put(address, coef);
    address += sizeof(coef);
    EEPROM.put(address, width);
  }
  else {
    int address = 0;
    EEPROM.get(address, coef);
    address += sizeof(coef);
    EEPROM.get(address, width);
  }
  
  _coef = 1/coef;
  initScreen();
}

unsigned long refreshedTime;

boolean menu = false;
int submenu = 0;
const int menusNum = 4;
String menus[menusNum] = {"Width:","Coef:","Reset Spent: Yes","Exit:        Yes"};
boolean inSubmenu = false;

void initScreen() {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print(consumption);
    lcd.setCursor(5, 0);
    lcd.print("LHa");
    lcd.setCursor(9, 0);
    lcd.print(lpersec*60.0,1);
    lcd.setCursor(15, 0);
    lcd.print("m");

    lcd.setCursor(0, 1);
    lcd.print(speedms*3.6,1); //
    lcd.setCursor(5, 1);
    lcd.print("KmH");
    lcd.setCursor(9, 1);
    lcd.print((int)spent);
    lcd.setCursor(15, 1);
    lcd.print("L");
}

void refresh() {

  spentIndex ++;
  if(spentIndex > 1) spentIndex = 0;

  lpersec = (spent - spentOld[spentIndex]);
  spentOld[spentIndex] = spent;
  consumption = 10000.0 * lpersec / (width * speedms); // L/s / (m*m/s)  -> L/m2
  if(consumption > 9999) consumption = 9999;
  if(consumption < 0) consumption = 0; 
  
  if(!menu) {
    lcd.setCursor(0, 0);
    lcd.print("     ");
    lcd.setCursor(0, 0);
    lcd.print(consumption);

    lcd.setCursor(9, 0);
    lcd.print("     ");
    lcd.setCursor(9, 0);
    lcd.print(lpersec*60.0,1);

    lcd.setCursor(0, 1);
    lcd.print("     ");
    lcd.setCursor(0, 1);
    lcd.print(speedms*3.6,1);

    lcd.setCursor(9, 1);
    lcd.print("     ");
    lcd.setCursor(9, 1);
    lcd.print((int)spent);
    
  }
}
void longClicked() {
   spent = 0.0;
   spentOld[0] = 0.0;
   spentOld[1] = 0.0;
}
void clicked() {
  if(menu) {
    inSubmenu = !inSubmenu;
    if(submenu==menusNum-1) {
      menu = false;
      inSubmenu = false;
    }
    if(submenu==2) {
      spent = 0.0;
      spentOld[0] = 0.0;
      spentOld[1] = 0.0;
      menu = false;
      inSubmenu = false;
    }
  }
  else {
    menu=true;
    inSubmenu = false;
  }

  /*VISUALIZATION*/
  if(menu) {
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Settings");
    lcd.setCursor(0, 1);
    lcd.print(menus[submenu]);
    
    if(submenu==0) {
      lcd.setCursor(10, 1);
      lcd.print(width);
    }
    if(submenu==1) {
      lcd.setCursor(10, 1);
      lcd.print(coef);
    }
    
    if(inSubmenu) {
      lcd.setCursor(9, 1);
      lcd.blink();
    }
    else lcd.noBlink();
  }
  else { //Back to main
    int address = 0;
    EEPROM.put(address, coef);
    address += sizeof(coef);
    EEPROM.put(address, width);
    _coef = 1/coef;
    initScreen();
  }
}
void rotated() {
  if(menu) {
    if(pos > oldPos) {
      if(inSubmenu) {
        if(submenu==0) width+=0.1;
        if(submenu==1) coef+=0.1;
      }
      else submenu ++;
    }
    else {
      if(inSubmenu) {
        if(submenu==0) width-=0.1;
        if(submenu==1) coef-=0.1;
      }
      else submenu --;
    }

    if(submenu < 0) submenu = menusNum-1;
    if(submenu > menusNum-1) submenu = 0;

    /*VISUALIZATION*/
    lcd.setCursor(0, 1);
    lcd.print("                ");
    lcd.setCursor(0, 1);
    lcd.print(menus[submenu]);
    if(submenu==0) {
      lcd.setCursor(10, 1);
      lcd.print(width);
    }
    if(submenu==1) {
      lcd.setCursor(10, 1);
      lcd.print(coef);
    }
    if(inSubmenu) {
      lcd.setCursor(9, 1);
      lcd.blink();
    }
    else lcd.noBlink(); 
  }
} 

boolean p1 = false; 
boolean p2 = false;
boolean key = false;
//boolean isClicked = false;
//boolean isLongClicked = false;
unsigned long clickedTime = 0;
unsigned long rotatedTime = 0;
unsigned long pingTime = 0;
boolean ping = false;
boolean ps1, ps2, k;
unsigned long mils = 0;
void loop() {
  
  ps1 = digitalRead(pinS1);
  ps2 = digitalRead(pinS2);
  k = digitalRead(pinKey);
  
  if(p1 != ps1) {
    p1 = ps1;
    if(!p2 && !p1) subpos ++;
  }
  if(p2!=ps2) {
    p2 = ps2;
    if(!p2 && !p1) subpos --;
  }

 
  
  if(k!=key) {
    if(!k && key) {
      if(millis() - clickedTime > 300) {
        clickedTime = millis();
        clicked();
      }
    }
    key = k;
  }
  

    /*if(!k) { //pressed
      if(millis() - clickedTime > 3000) {
        if(!isLongClicked) longClicked();
        isLongClicked = true;
      }
      else if(millis() - clickedTime > 50) {
        if(!isClicked) clicked();
        isClicked = true;
      }
    }
    //key = k;
    else { //released
      clickedTime = millis();
      isLongClicked = false;
      isClicked = false;
    }*/
  
  if(menu && !inSubmenu) {
    if (subpos != oldPos && millis() - rotatedTime >100) {
      rotatedTime = millis();
      if(subpos > oldPos) pos++;
      else pos --;
      rotated();
      oldPos = subpos;
    }
  }
  else {
    if (subpos != oldPos && millis() - rotatedTime > 10) {
      rotatedTime = millis();
      if(subpos > oldPos) pos++;
      else pos --;
      rotated();
      oldPos = subpos;
    }
  }
  
  mils = millis();
  if(mils - refreshedTime >= 500) {
    refresh();
    refreshedTime = mils;
  }

  if(millis() - pingTime >= 50) {
    ping = !ping;
    digitalWrite(4,ping);
    pingTime = millis();
  }

  int msgType = processGPS();
  if ( msgType == MT_NAV_VELNED ) {
    //lcd.setCursor(10, 1);
    //lcd.print(ubxMessage.navVelned.iTOW);
    speedms = ubxMessage.navVelned.gSpeed / 100.0;
  } 
}
