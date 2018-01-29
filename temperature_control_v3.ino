#include <SoftwareSerial.h>

#define SMS_RESPONSE_ON_SUCCESS "Turned ON."
#define SMS_RESPONSE_ON_FAIL "Failed to turn ON."
#define SMS_RESPONSE_OFF_SUCCESS "Turned OFF."
#define SMS_RESPONSE_OFF_FAIL "Failed to turn OFF."
#define SMS_RESPONSE_ADD_SUCCESS "Added."
#define SMS_RESPONSE_ADD_FAIL "Failed to add."
#define SMS_RESPONSE_DELETE_SUCCESS "Deleted."
#define SMS_RESPONSE_DELETE_FAIL "Failed to delete."
#define SMS_RESPONSE_STATUS_ON "Status: ON"
#define SMS_RESPONSE_STATUS_OFF "Status: OFF"
#define SMS_RESPONSE_STATUS_UNKNOWN "Status: UNKNOWN"

SoftwareSerial serialSIM800(8,7);
char buffer[200];
volatile bool status = HIGH;
volatile bool rewritestatus = false;
unsigned long buttonTimer;

void clearSerialBuffer() {
  while(serialSIM800.available()) {
    serialSIM800.read();
  }
}

void clearBuffer(char* buffer, int length) {
  for (int i=0;i<length;i++) {
    buffer[i] = '\0';
  }
}

int readLine(int timeout) {
  unsigned long timerStart;
  timerStart = millis();
  int i = 0;
  clearBuffer(buffer, 200);
  while(true) {
    if (i == 200) {return 0;}
    if(serialSIM800.available()) {
      buffer[i] = serialSIM800.read();
      if (i > 0 && buffer[i-1] == '\r' && buffer[i] == '\n') {
        Serial.print(F("                       READ: "));
        Serial.print(buffer);
        delay(25);
        return 1;
      }
      i++;
    }
    if(millis() - timerStart > 1000 * timeout) {
      return -1;
    }
  }
  return 0;
}

void writeCommand(char* command) {
  serialSIM800.write(command);
  serialSIM800.write('\n');
}

int readCommand(char* command) {
  int length = strlen(command)-1;
  if (length < 1) {return 0;}
  readLine(10);
  if (strncmp(buffer, command, length) == 0) {return 1;}
  return 0;
}

int readContact(int index, char* number, char* name) {
  clearSerialBuffer();
  char command[12];
  clearBuffer(number, 40);
  clearBuffer(name, 30);
  clearBuffer(command, 12);
  snprintf(command, sizeof(command), "AT+CPBR=%d", index);
  writeCommand(command);
  if (readCommand(command) == 1 && readCommand("+CPBR: ") == 1) {
    int x = 0;
    int y = 0;
    while (x<200) {
      if (buffer[x] == '"' && y == 0) {
        x++;
        while(buffer[x] != '"') {
          number[y] = buffer[x];
          x++;
          y++;
        }
        x++;
      }
      else if (buffer[x] == '"') {
        x++;
        y = 0;
        while(buffer[x] != '"') {
          name[y] = buffer[x];
          x++;
          y++;
        }
        break;
      }
      else {
        x++;
      }
    }
    readLine(10);
    if (readCommand("OK") == 1) {
      return 1;
    }
  }
  return 0;
}

int saveContact(char* number, char* name, int index = 0) {
  clearSerialBuffer();  
  if (strlen(number) > 40 || strlen(name) > 30) {return 0;}
  char command[90];
  clearBuffer(command, 90);
  if (index == 0) {
    snprintf(command, sizeof(command), "AT+CPBW=,\"%s\",,\"%s\"", number, name);
  }
  else {
    snprintf(command, sizeof(command), "AT+CPBW=%d,\"%s\",,\"%s\"", index, number, name);
  }
  writeCommand(command);
  if (readCommand(command) == 1 && readCommand("OK") == 1) {return 1;}
  return 0;
}

int deleteContacts(char* name) {
  if (strlen(name) > 30) {return 0;}
  unsigned long timerStart = millis();
  char command[41];
  int index[50];
  int i = 0;
  for (int x = 0; x < 50; x++) {
    index[x] = 0;
  }
  clearBuffer(command, 41);
  snprintf(command, sizeof(command), "AT+CPBF=\"%s\"", name);
  writeCommand(command);
  if (readCommand(command) != 1) {return 0;}
  while(true) {
    if(serialSIM800.available()) {
      if (readCommand("+CPBF: ") == 1) {
        if (buffer[8] == ',') {
          index[i] = buffer[7]-'0';
          i++;
        }
        else if (buffer[9] == ',') {
          index[i] = (buffer[7]-'0')*10+buffer[8]-'0';
          i++;
        }
        else {
          return 0;
        }
      }
      else if (strncmp(buffer, "OK", 2) == 0) {
        clearBuffer(command, 41);
        for (int x = 0; x < i; x++) {
          snprintf(command, sizeof(command), "AT+CPBW=%d", index[x]);
          writeCommand(command);
          if (readCommand(command) != 1 || readCommand("OK") != 1) {
            return 0;
          }
        }
        return 1;
      }
    }
    if(millis() - timerStart > 1000 * 10) {
      return -1;
    }
  }
}

int readSMS(int index, char* header, char* message) {
  if (index>50) {return 0;}
  clearSerialBuffer();
  char command[11];
  clearBuffer(command,11);
  clearBuffer(header, 200);
  clearBuffer(message, 200);
  snprintf(command, sizeof(command), "AT+CMGR=%d",index);
  writeCommand(command);
  if (readCommand(command) == 1 && readCommand("+CMGR: ") == 1) {
    for (int i = 0; i < 200; i++) {
      if(i > 0 && buffer[i-1] == '\r' && buffer[i] == '\n') {break;}
      header[i] = buffer[i];
    }
    readLine(10);
    for (int i = 0; i < 200; i++) {
      if(i > 0 && buffer[i-1] == '\r' && buffer[i] == '\n') {break;}
      message[i] = buffer[i];
    }
    while (readLine(10) == 1) {
      if (strncmp(buffer, "OK", 2) == 0) {
        return 1;
      }
    }
  }
  return 0;
}

int sendSMS(char* number, char* message) {
  clearSerialBuffer();
  unsigned long timerStart;
  timerStart = millis();
  char c;
  serialSIM800.write("AT+CMGS=\"");
  serialSIM800.write(number);
  serialSIM800.write("\"\n");
  while (true) {
    if (serialSIM800.available()) {
      c = serialSIM800.read();
      if (c == '>') {
        serialSIM800.write(message);
        serialSIM800.write((char)26);
        while (true) {
          if(readCommand("+CMGS: ") == 1) {
            return 1;
          }
          if(millis() - timerStart > 1000 * 10) {
            return -1;
          }
        }
      }
    }
    if(millis() - timerStart > 1000 * 10) {
      return -1;
    }
  }
  return 0;
}

int deleteSMS(int index) { //if index is 0, delete all
  if (index > 50 || index < 0) {return 0;}
  unsigned long timerStart;
  timerStart = millis();
  if (index == 0) {
    while(true) {
      writeCommand("AT+CMGDA=\"DEL ALL\"");
      if (readCommand("AT+CMGDA=\"DEL ALL\"") == 1 && readCommand("OK") == 1) {return 1;}
      if(millis() - timerStart > 1000 * 10) {
        return -1;
      }
    }
  }
  else {
    char command[11];
    clearBuffer(command, 11);
    snprintf(command, sizeof(command), "AT+CMGD=%d", index);
    while(true) {
      writeCommand(command);
      if (readCommand(command) == 1 && readCommand("OK") == 1) {return 1;}
      if(millis() - timerStart > 1000 * 10) {
        return -1;
      }
    }
  }
  return 0;
}

int adminCheck(char* header) {
    int x = 0;
    for (int i = 0; i < 5; i++) {
      while (true) {
        if (x == 195) {return 0;}
        if (header[x] == '"') {
          x++;
          break;
        }
        x++;
      }
    }
    if (strncmp(header+x, "ADMIN", 5) == 0) {return 1;}
    return 0;
}

int messageHandler(char* header, char* message) {
  int x = 0;
  while(true) {
    if (message[x] == '\0' || x == 200) {break;}
    message[x] = toupper(message[x]);
    x++;
  }
  char number[41];
  clearBuffer(number,41);
  x = 0;
  for (int i = 0; i < 3; i++) {
    while (true) {
      if (x == 160) {return 0;}
      if (header[x] == '"') {
        x++;
        break;
      }
      x++;
    }
  }
  int i = 0;
  while(header[x] != '"') {
    number[i] = header[x];
    x++;
    i++;
  }
  if (strncmp(message, "ON", 2) == 0) {
    if (saveContact("1", "STATUS", 1) == 1) {
      status = HIGH;
      digitalWrite(A0, status);
      sendSMS(number, SMS_RESPONSE_ON_SUCCESS);
      return 1;
    }
    else {
      sendSMS(number, SMS_RESPONSE_ON_FAIL);
      return 0;
    }
  }
  else if (strncmp(message,"OFF", 3) == 0) {
    if (saveContact("0", "STATUS", 1) == 1) {
      status = LOW;
      digitalWrite(A0, status);
      sendSMS(number, SMS_RESPONSE_OFF_SUCCESS);
      return 1;
    }
    else {
      sendSMS(number, SMS_RESPONSE_OFF_FAIL);
      return 0;
    }
  }
  else if (strncmp(message, "ADD", 3) == 0) {
    char name[31];
    clearBuffer(name,31);
    char number2[41];
    clearBuffer(number2,41);
    x = 0;
    i = 0;
    while (true) {
      if (x == 200) {return 0;}
      if (message[x] == ' ') {
        x++;
        break;
      }
      x++;
    }
    while(true) {
      if (i == 40 || x == 200) {return 0;}
      if (message[x] == ' ') {
        x++;
        break;
      }
      number2[i] = message[x];
      i++;
      x++;
    }
    for (int y = 0; y < 5; y++) {
      name[y] = "ADMIN"[y];
    }
    i = 5;
    while(true) {
      if (i == 30 || x == 200) {return 0;}
      if (message[x] == '\r') {
        x++;
        break;
      }
      name[i] = message[x];
      i++;
      x++;
    }
    if (saveContact(number2, name) == 1) {
      sendSMS(number, SMS_RESPONSE_ADD_SUCCESS);
      return 1;
    }
    else {
      sendSMS(number, SMS_RESPONSE_ADD_FAIL);
      return 0;
    }
  }
  else if (strncmp(message, "DELETE", 6) == 0) {
    char name[30];
    clearBuffer(name,30);
    x = 0;
    i = 0;
    while (true) {
      if (x == 200) {return 0;}
      if (message[x] == ' ') {
        x++;
        break;
      }
      x++;
    }
    while(true) {
      if (i == 30 || x == 200) {return 0;}
      if (message[x] == '\r') {
        x++;
        break;
      }
      name[i] = message[x];
      i++;
      x++;
    }
    if (strncmp(name, "ADMIN", 5) != 0) {
      if (deleteContacts(name) == 1) {
      sendSMS(number, SMS_RESPONSE_DELETE_SUCCESS);
      return 1;
      }
      else {
      sendSMS(number, SMS_RESPONSE_DELETE_FAIL);
      return 0;
      }
    }
  }
  else if (strncmp(message, "STATUS", 6) == 0) {
    if (status == HIGH) {
      sendSMS(number, SMS_RESPONSE_STATUS_ON);
      return 1;
    }
    else if (status == LOW) {
      sendSMS(number, SMS_RESPONSE_STATUS_OFF);
      return 1;
    }
    else {
      sendSMS(number, SMS_RESPONSE_STATUS_UNKNOWN);
      return 0;
    }
  }
  return 0;
}

void buttonInterrupt() {
  if (millis() - buttonTimer > 1000 * 3) {
    buttonTimer = millis();
    if (status == HIGH) {
      status = LOW;
    }
    else if (status == LOW) {
      status = HIGH;
    }
    digitalWrite(A0, status);
    rewritestatus = true;
  }
}

void setup() {
	Serial.begin(9600);
	while(!Serial);
	serialSIM800.begin(9600);
	while(!serialSIM800);
	Serial.println(F("Serial OK."));
	while(true) {
		writeCommand("AT");
    if (readCommand("AT") == 1 && readCommand("OK") == 1) {
    }
    else {
      delay(3000);
      clearSerialBuffer();
      continue;
    }
    writeCommand("AT+CFUN=1");
    if (readCommand("AT+CFUN=1") == 1 && readCommand("OK") == 1) {
    }
    else {
      delay(3000);
      clearSerialBuffer();
      continue;
    }
    writeCommand("AT+CSCS=\"GSM\"");
    if (readCommand("AT+CSCS=\"GSM\"") == 1 && readCommand("OK") == 1) {
    }
    else {
      delay(3000);
      clearSerialBuffer();
      continue;
    }
    writeCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"");
    if (readCommand("AT+CPMS=\"SM\",\"SM\",\"SM\"") == 1 && readCommand("+CPMS: ") == 1 && readLine(10) && readCommand("OK")) {
    }
    else {
      delay(3000);
      clearSerialBuffer();
      continue;
    }
    writeCommand("AT+GSMBUSY=1");
    if (readCommand("AT+GSMBUSY=1") == 1 && readCommand("OK") == 1) {
    }
    else {
      delay(30000);
      clearSerialBuffer();
      continue;
    }
    writeCommand("AT+CMGF=1");
    if (readCommand("AT+CMGF=1") == 1 && readCommand("OK") == 1) {
      break;
    }
    else {
      delay(3000);
      clearSerialBuffer();
      continue;
    }
		}
 Serial.println(F("SMS init OK."));
 buttonTimer = millis();
 pinMode(A0, OUTPUT);
 pinMode(2, INPUT);
 attachInterrupt(digitalPinToInterrupt(2), buttonInterrupt, RISING);
 char name[30];
 char number[40];
 readContact(1, number, name);
 if (number[0] == '1') {
  status = HIGH;
 }
 else if (number[0] == '0') {
  status = LOW;
 }
 else {
  status = LOW;
  saveContact("0", "STATUS", 1);
 }
 Serial.println(F("Init OK."));
}

void loop() {
  if (readCommand("+CMTI: ") == 1) {
    int i = 0;
    int messageid = 0;
    while(i < 200 && buffer[i] != ',') {
      i++;
    }
    if (buffer[i+2] == '\r') {
        messageid = buffer[i+1] - '0';
    }
    else if (buffer[i+3] == '\r') {
        messageid = (buffer[i+1] - '0')*10 + buffer[i+2] - '0';
    }
    char header[200];
    char message[200];
    if (messageid > 0) {
      readSMS(messageid, header, message);
      deleteSMS(0);
      if(adminCheck(header) == 1) {
        messageHandler(header, message);
      }
    }
  }
  digitalWrite(A0, status);
  if (rewritestatus == true) {
    if (status == HIGH) {
      saveContact("1", "STATUS", 1);
    }
    else {
      saveContact("0", "STATUS", 1);
    }
    rewritestatus = false;
  }
  
  /*
  while(true) {
        if(serialSIM800.available()){
            Serial.write(serialSIM800.read());
        }
        if(Serial.available()){     
            serialSIM800.write(Serial.read()); 
        }
  }
  */
}
