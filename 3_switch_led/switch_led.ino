#include <SPI.h>         // needed for Arduino versions later than 0018
#include <Ethernet.h>
#include <EthernetUdp.h>         // UDP library from: bjoern@cs.stanford.edu 12/30/2008

/* PIN constants */
const byte LED = 2;
const byte SWITCH = 3;

/* Needed bytes */
const byte CMND[] = {67,77,78,68,32};
const byte DREF[] = {68,82,69,70,32};
const byte RREF[] = {82,82,69,70,32};

byte mac[] = {
  0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xED
};
IPAddress ip(192, 168, 0,20);
IPAddress remote(192,168,0,13);

unsigned int xPlanePort = 49000;      // local port to listen on

boolean prevSwitchState = HIGH;

EthernetUDP Udp;

void setup() {
  // put your setup code here, to run once:
  pinMode(LED, OUTPUT);
  pinMode(SWITCH, INPUT_PULLUP);

  Ethernet.begin(mac, ip);
  Udp.begin(xPlanePort);
  Serial.begin(9600);
}

void loop() {
  Serial.print("Prev: ");
  Serial.print(prevSwitchState);
  Serial.print(" Now: ");
  Serial.print(digitalRead(SWITCH));
  Serial.print('\n');
  
  if(prevSwitchState == LOW && digitalRead(SWITCH) == HIGH) {
    if(digitalRead(LED) == LOW) {
      digitalWrite(LED, HIGH);

      double parameters[0];
      sendMessage(CMND, parameters, "sim/transponder/transponder_alt");
    } else {
      digitalWrite(LED, LOW);

      double parameters[0];
      sendMessage(CMND, parameters, "sim/transponder/transponder_standby");
    }
  }

  prevSwitchState = digitalRead(SWITCH);
}

void sendMessage(byte* label, double* parameters, String path) {
  
  int bufferSize;
  int nbParameters;
  
  //Get label to determine buffer size
  if(label == CMND) {
    //No fixed length, so label + path size
    nbParameters = 0;
    bufferSize = 5 + path.length();
  } else if (label == DREF) {
    //Label + 4 bytes parameter + 500 bytes path = 509
    nbParameters = 1;
    bufferSize = 509;
  } else { //Just assume is RREF now
    //Label + 2 4 bytes parameters + 400 bytes path = 413
    nbParameters = 2;
    bufferSize = 413;
  }
  
  byte buffer[bufferSize];
  int cursor = 0;
  byte param[2][4]; //Max 2 parameters of 4 bytes

  /*Serial.print("Label is: ");
  for(int i = 0; i < 5; i++) {
    Serial.print(label[i]);
    Serial.print(" ");
  }
  
  Serial.println("");
  
  Serial.print("Buffer size is: ");
  Serial.println(bufferSize);

  Serial.println("Parameters: ");
  for(int i = 0; i < nbParameters; i++) {
    for(int j = 0; j < 4; j++) {
      Serial.print(param[i][j]); //Don't know whybut does not work properly without log
      Serial.print(" ");
    }
    Serial.println(); 
  }

  Serial.println(path);*/
  //Adding label to buffer
  for(int i = 0; i < 5; i ++) {
    buffer[cursor] = label[i];
    cursor++; 
  }

  //Building params matrix
  for(int i = 0; i < nbParameters; i++) {
    //Serial.println(parameters[i]);
    byte* ieee754encode = ieee754Encode(parameters[i]);

    for(int j = 0; j < 4; j++) {
      param[i][j] = ieee754encode[j];
    }
  }

  //Adding parameter to buffer
 for(int i = 0; i < nbParameters; i ++) {
    for(int j = 0; j < 4; j++) {
      Serial.println(param[i][j]); //Don't know whybut does not work properly without log
      buffer[cursor] = param[i][j];
      cursor++;
    } 
  }

   //Adding dataref path to buffer
  for(int i = 0; i < path.length(); i ++) {
    buffer[cursor] = path[i];
    cursor++; 
  }

  //Pad with nulls
  for(int i = cursor; i < bufferSize; i ++) {
    buffer[cursor] = 0;
    cursor++; 
  }

  Serial.println();
  for(int i = 0; i < bufferSize; i++){
    Serial.print((char)buffer[i]);
    //Serial.print(" ");
  }

 /*for(int i = 0; i < sizeof(param); i ++) {
    Serial.println(param[i]);
    buffer[cursor] = param[i];
    cursor++; 
 }*/

  Udp.beginPacket(remote, xPlanePort);
  Udp.write(buffer,bufferSize);
  Udp.endPacket();
}

//Since built-in pow give weird value (don't know why), we make our own
long intPow2(int x) {
  if(x == 0) {
    return 1;
  }

  if(x == 1) {
    return 2;
  }

  return 2 * intPow2(x - 1);
}

double negativePow2(int x) {
  return (double) 1/intPow2(x);
}

//Converts a byte into an arrayof bit;
boolean* bytesToBits(byte value) {
  
  Serial.print("In: ");
  Serial.println(value);
  boolean toReturn[8];
  byte total = 0;
  byte tTotal = 0; //Add to total only if not greater than value. So we have a temporary value to calculate 
  int i = 7;

  while(i >= 0) {
   tTotal = total + intPow2(i);
   
   //If not greater, then our bynary at this place is 1
   if(tTotal <= value) {
      total = tTotal;
      toReturn[7 - i] = 1;
   } else {
    toReturn[7 - i] = 0;
   }
   
   i--;
   tTotal = total;
 }

 Serial.print("Out:");
 for(int i = 0; i < 8; i++) {
  Serial.print(toReturn[i]);
 }
Serial.println("");
 return toReturn;
}

//Converts an array of bits into a byte
byte bitsArrayToByte(boolean* bitsArray) {
  byte toReturn = 0;

    for(int i = 0; i < 8; i++) {
      if(bitsArray[i] == 1) {
        toReturn += intPow2(7-i); //7 - i because for i = 0 is for 2^7, i = 1 for 2^6,... 
      }
    }

    return toReturn;
}

//Encode value to IEEE754 format to send to X-Plane
byte* ieee754Encode(double value) {
  //4 bytes is sent to X-Plane
  byte toReturn[4];

  //Will contains our bits
  boolean bitsArray[32];
  int cursor = 1; //Init to 1 because no need for fist bit right below

  //First step, bit for positive or negative
  if(value >= 0) {
    bitsArray[0] = 0; //0 if positive
  } else {
    bitsArray[0] = 1; //1 if negative
  }

  //Second step, find the exponent
  byte dExponent = 127; //Maximum exponent is 127
  while(pow(2,dExponent) > value && dExponent > -127) {
    dExponent--;
  }

  //Add 127 to exponent to allow negative exponent
  dExponent += 127;

  //Convert into bits
  boolean* bExponent = bytesToBits(dExponent);
  
  //Fill our bits array
  for(int i = 0; i < 8; i++) {
      bitsArray[cursor] = bExponent[i];
      cursor++;
  }
  
  //Third step, get the mantissa. The entire part is always 1 so we get rid of it for X-Plane
  double dMantissa = value / intPow2(dExponent - 127) - 1; //Use of own function. Don't know why but built-in return decimal

  double compare = 0;
  
  //Fill directly bitsArray
  for(int i = 0; i < 23; i++) {
    double t = negativePow2(i + 1);
    //Serial.println(compare + t, 16);
   if((compare + t) <= dMantissa) {
      bitsArray[cursor] = 1;
      compare += t; 
    } 
    else {
      bitsArray[cursor] = 0;
    }

    cursor++;
  }

for(int i = 0; i < 32; i++) {
  //Serial.print(bitsArray[i]);
}
  //Make 4 bytes
  byte rToReturn[4];
  for(int i = 0; i < 4; i++) {
    boolean b[8];
    for(int j = 0; j < 8; j++) {
       b[j] = bitsArray[(i * 8) + j];
    }
    
    rToReturn[i] = bitsArrayToByte(b);
  }

  //Reverse for X-Plane
  toReturn[0] = rToReturn[3];
  toReturn[1] = rToReturn[2];
  toReturn[2] = rToReturn[1];
  toReturn[3] = rToReturn[0];
  
  Serial.print("Bytes: ");
  for(int i = 0; i < 4; i++) {
    Serial.print(toReturn[i]);
    Serial.print(", ");
  }
  return toReturn;
}

//Converts 4 bytes X-Plane value to readable value
float ieee754(byte bytes[]) {

  //On PC, message is sent with less significant byte first. We have to reverse it
  byte rBytes[4];
  rBytes[0] = bytes[3];
  rBytes[1] = bytes[2];
  rBytes[2] = bytes[1];
  rBytes[3] = bytes[0];

  byte fullMessage[32];
  int j = 0;

  boolean* temp = bytesToBits(bytes[3]);
  for(int i = 0; i < 8; i++) {
    fullMessage[j] = temp[i];
    j++;
  }

  temp =  bytesToBits(bytes[2]);
 for(int i = 0; i < 8; i++) {
    fullMessage[j] = temp[i];
    j++;
  }

  temp =  bytesToBits(bytes[1]);
  for(int i = 0; i < 8; i++) {
    fullMessage[j] = temp[i];
    j++;
  }

  temp =  bytesToBits(bytes[0]);
  for(int i = 0; i < 8; i++) {
    fullMessage[j] = temp[i];
    j++;
  }

 for(int i = 0; i < 32; i++) {
    Serial.print(fullMessage[i]);
 }
}
