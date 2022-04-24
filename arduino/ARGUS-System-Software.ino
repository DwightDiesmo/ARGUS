#include "camera.h"
#include <PDM.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_ADXL345_U.h>

#include <WiFi.h>
#include <BSP.h>
#include <SDMMCBlockDevice.h>
#include "FATFileSystem.h"

#define ARGUS_SYS_NUM "/argus0" //argus0 root folder name
#define ARGUS_AUDIO_FILE "/audio" //audio folder name
#define ARGUS_GPS_FILE "gps" //gps folder name
#define ARGUS_IMAGE_FILE "image" //image folder name
#define ARGUS_SEISMIC_FILE "seismic" //seismic folder name
#define FTP_PORT 21 //FTP port number, usually 21 or 22 default

#define FTP_USERNAME "portentah7" //username to access FTP server
#define FTP_PASSWORD "argus2" //password to access FTP server

#define FATFileSystem_NAME "fs" //fs (filesystem) while using files

#define PASVPORT 50001

#define WIFI_IP1 192 //WiFi IP address
#define WIFI_IP2 168 //WiFi IP address
#define WIFI_IP3 43 //WiFi IP address
#define WIFI_IP4 132 //WiFi IP address
                                    //Riley mobile hotspot IP: 192, 168, 43, 132
                                    //Home IP: 192, 168, 0, 185
                                    //TEST DEFINE REMOVE
                                    //rileyDesktop
//////////////////////////
#define ARGUS_PART_NUM "fs/0"

#define ARGUS_APPEND ".txt"

//////////////////////////////////////////////PIR
#define pirLeft A1 //A1
#define pirCenter A2 //A2
#define pirRight A3 //A3
#define testLED 0

#define mic A4 /////////////////////NEW MIC //A4

int pirThreshold = 5;
const int pirQueueSize = 10;

bool pirStatusLeft = false;
int pirQueueLeft[pirQueueSize] = {0};
bool pirStatusCenter = false;
int pirQueueCenter[pirQueueSize] = {0};
bool pirStatusRight = false;
int pirQueueRight[pirQueueSize] = {0};
//////////////////////////////////////////////PIR

int dirIndex = 0;
int dirIndexDef = 1;

char *fileNameArray[50];

char fileName[20];
char fileNameTemp[20] = "00000000";
int iFile = 0;

char outBuf[128]; //buffer of 128 char for FTP server to recieve
char outCount; //buffer counter

SDMMCBlockDevice block_device;
mbed::FATFileSystem fs(FATFileSystem_NAME);
FILE *myFile;

IPAddress server(WIFI_IP1, WIFI_IP2, WIFI_IP3, WIFI_IP4);

WiFiClient client;
WiFiClient dclient;

/* Camera Global */
CameraClass cam;
uint8_t fb[320*320]; //320*320
void takePicture();

/* Mic Global */
static const char channels = 1;
static const int frequency = 64000;
short sampleBuffer[512];
volatile int samplesRead;

/* Seismic Global */
Adafruit_ADXL345_Unified seismic = Adafruit_ADXL345_Unified(12345);
void recordSeismic();

/* GPS Global */
#define GPS Serial1
void recordGPS();

void onPDMdata();

void mountSD();
void setupWiFi();
byte receiveBytes();
void fileRead();
int FTPConnect();
void nameFile();

//PIR
bool Motion_Detected (int queue[], int queue_length, int new_data, int threshold);
void Print_Motion_Queue(int motion_queue[]);

//counter to transmit
int transmitCounter = 1;

void setup() {
  Serial.begin(921600);
  pinMode(pirLeft, INPUT);
  pinMode(pirCenter, INPUT);
  pinMode(pirRight, INPUT);
  pinMode(LED_BUILTIN, OUTPUT);
  
  mountSD();
  setupWiFi();  //uncomment to connect to ftp server

  /* Camera Setup */
  cam.begin(CAMERA_R320x320, 30); //320x320
  /* Mic Setup */
  /*PDM.onReceive(onPDMdata);
  if (!PDM.begin(channels, frequency)) {
    Serial.println("Failed to start PDM!");
    while (1);
  }*/
  pinMode(mic, INPUT);
  /* Seismic Setup */
  if(!seismic.begin()){
    Serial.println("There was a problem detecting the ADXL345 ... check your connections");
    while(1);
  }
  seismic.setRange(ADXL345_RANGE_16_G); 
  /* GPS Setup */
  GPS.begin(9600);
}

void loop() {
  if(transmitCounter % 6 == 0) { //10 //or 3
    fileRead(); //uncomment for ftp transmission
    FTPConnect(); //uncomment for ftp transmission
  } else {
    pirStatusLeft = Motion_Detected (pirQueueLeft, pirQueueSize, analogRead(pirLeft), pirThreshold);
    pirStatusCenter = Motion_Detected (pirQueueCenter, pirQueueSize, analogRead(pirCenter), pirThreshold);
    pirStatusRight = Motion_Detected (pirQueueRight, pirQueueSize, analogRead(pirRight), pirThreshold);

    Serial.print("Left: ");
    Print_Motion_Queue(pirQueueLeft);
    Serial.println();
    Serial.print("Center: ");
    Print_Motion_Queue(pirQueueCenter);
    Serial.println();
    Serial.print("Right: ");
    Print_Motion_Queue(pirQueueRight);
    Serial.println();
  
    if(pirStatusLeft || pirStatusCenter || pirStatusRight) {
      Serial.println("True High");
      digitalWrite(LED_BUILTIN, HIGH);
    
      nameFile();
      strcat(fileName, ARGUS_APPEND);
      Serial.print("FULL FILE NAME: "); Serial.println(fileName);
      myFile = fopen(fileName, "wb+");
      //////////////////////////////////////////
      Serial.println("Audio Data");
      fprintf(myFile, "&");
      for(int i1 = 0; i1 < 10; i1++) {
        recordAudio();
      }
      
      Serial.println("Seismic Data");
      fprintf(myFile, "&");
      recordSeismic();
      
      Serial.println("Image Data");
      fprintf(myFile, "&");
      takePicture();

      Serial.println("GPS Data");
      fprintf(myFile, "&");
      recordGPS();

      
      //////////////////////////////////////////
      Serial.println("FINISHED GAUNTLET");
      fclose(myFile); 
      Serial.println("FILE CLOSED");
      transmitCounter++;
    } else {
      digitalWrite(LED_BUILTIN, LOW);
    }
    delay(1000);
  }
}
void takePicture() {
  cam.grab(fb);
  int i = 0;
  for(int rows = 0; rows < (320 * 320); rows++) { //320 * 320 //320 * 240
    //Serial.print(fb[i]); Serial.print("_"); /////////////////////////////////////////////////////SERIAL PRINT
    fprintf(myFile, "%d_", fb[i]);
//    for(int cols = 0; cols < 320; cols++) {
//      Serial.print(fb[i]);
//      fprintf(myFile, "%f")
//      Serial.print(" ");
//      i++;
//    }
    i++;
  }
}

/*void recordAudio() {
  if (samplesRead) {
    for (int i = 0; i < samplesRead; i++) {
      Serial.println(sampleBuffer[i]);
      fprintf(myFile, "%d_", sampleBuffer[i]);
    }
    samplesRead = 0;
  }
}*/
void recordAudio() {
  for(int i = 0; i < 8000; i++) {
   int d = analogRead(mic);
   //Serial.print(d); Serial.print(" "); ///////////////////////////////////////////////////SERIAL PRINT
   fprintf(myFile, "%d ", d); 
  }
}

void onPDMdata() {
  int bytesAvailable = PDM.available();
  PDM.read(sampleBuffer, bytesAvailable);
  samplesRead = bytesAvailable / 2;
}

void recordSeismic() {
  for(int i = 0; i < (355 * 10); i++) { //60
    sensors_event_t seismicEvent; 
    seismic.getEvent(&seismicEvent);
    //Serial.print(seismicEvent.acceleration.x); //////////////////////////////////////////////////SERIAL PRINT
    fprintf(myFile, "%f", seismicEvent.acceleration.x);
    //Serial.print(" "); //////////////////////////////////////////////////SERIAL PRINT
    fprintf(myFile, " ");
    //Serial.print(seismicEvent.acceleration.y); //////////////////////////////////////////////////SERIAL PRINT
    fprintf(myFile, "%f", seismicEvent.acceleration.y);
    //Serial.print(" "); //////////////////////////////////////////////////SERIAL PRINT
    fprintf(myFile, " ");
    //Serial.println(seismicEvent.acceleration.z); //////////////////////////////////////////////////SERIAL PRINT
    fprintf(myFile, "%f_", seismicEvent.acceleration.z);
  }
}

void recordGPS() {
//  while (GPS.available() > 0)
//  Serial.write(GPS.read());
//  fprintf(myFile, "%s", GPS.read());
  for(int i = 0; i < 500; i++){
     if (GPS.available()) {
        char c = GPS.read();
        if (c == '\n') {
          c='_';
        }
        //Serial.print(c); ////////////////////////////////////////////////////////////////////////////SERIAL PRINT
        fprintf(myFile, "%c", c);
      }
  }
}

void fileRead() {
  DIR *d; //dir
  struct dirent *dir; //ent
  d = opendir("/fs");
  if(d) {
    while((dir = readdir(d)) != NULL) {
      fileNameArray[dirIndex] = (char *)malloc(strlen(dir->d_name)+1);
      strcpy(fileNameArray[dirIndex], dir->d_name);
      Serial.println(fileNameArray[dirIndex]);
      dirIndex++;
    }
    closedir(d);
    Serial.print("dirIndex: "); Serial.println(dirIndex);
  }
  else {
    // Could not open directory
    Serial.println("Error opening SDCARD\n");
    while(1);
  }
  if(dirIndex == 0) {
    Serial.println("Empty SDCARD");
  }
}

int FTPConnect() {
  //open and prep certain file to send (function)
  while((dirIndexDef <= dirIndex-1) && (fileNameArray[dirIndexDef]) != "System Volume Information") {
    if(client.connect(server, FTP_PORT)) {
      Serial.println("Connected"); //TEST LINE REMOVE
    }
    else {
      Serial.println("Failed"); //TEST LINE REMOVE
    }
    Serial.print("FILE: "); Serial.println(fileNameArray[dirIndexDef]);
    char fs1[50] = "fs/";
    char curr[50];
    strcpy(curr, fileNameArray[dirIndexDef]);
    Serial.print("curr FILE: "); Serial.println(curr);
    strcat(fs1, curr);
    Serial.print("Concat String: "); Serial.println(fs1);
    //myFile = fopen("fs/0_000_a.txt", "rb");
    myFile = fopen(fs1, "rb");
    if(myFile == NULL) {
      Serial.println("SD OPEN FAIL");
      return 0;
    }
  
    if(!receiveBytes()) return 0; //constantly try to get recieveBytes() to pass and return 1
    client.print(F("USER ")); client.println(F(FTP_USERNAME)); //login username for FTP server
    Serial.print(F("USER ")); Serial.println(F(FTP_USERNAME));
    if(!receiveBytes()) return 0;
    client.print(F("PASS ")); client.println(F(FTP_PASSWORD)); //login password for FTP server
    Serial.print(F("PASS ")); Serial.println(F(FTP_PASSWORD));
    if(!receiveBytes()) return 0;
    client.println(F("SYST"));
    Serial.println(F("SYST")); 
    if(!receiveBytes()) return 0;
    client.println(F("PASV"));
    Serial.println(F("PASV")); 
    if(!receiveBytes()) return 0;
    Serial.print(F("Data port: "));
    Serial.println(PASVPORT);

    if (dclient.connect(server,PASVPORT)) {
      Serial.println(F("Data connected"));
    } 
    else {
      Serial.println(F("Data connection failed"));
      client.stop();
      fclose(myFile);
      return 0;
    }
    //cwd
    client.println(F("CWD /argus0"));
    
    /////////////////////////////////////////////////////
    Serial.println(fileNameArray[dirIndexDef]); //print current name of function
    client.print(F("STOR ")); Serial.print("STOR ");
    client.println(fileNameArray[dirIndexDef]); Serial.println(fileNameArray[dirIndexDef]);
    if(!receiveBytes()) return 0; Serial.println("Reading and Writing");
    int sz = 0;
    char buffer1[128]; //64
    //explicit_bzero(buffer1, 128); //64
    bzero(buffer1, 128); ////////////////////////////////////////////////////////////
    fseek(myFile, 0, SEEK_END);
    sz = ftell(myFile);
    rewind(myFile);
    Serial.println(sz);
    while(1)
    {
      if(feof(myFile))
      {
        break;
      }
      if(sz > 127) //63
      {
        fgets(buffer1, 128, myFile); //64
        dclient.write(buffer1, 127); //63
        sz = sz - 127; //63
        //Serial.println("sz > 63");
        delay(1);
      }
      if(sz > 0 && sz < 128) //64
      {
        fgets(buffer1, 128, myFile); //64
        dclient.write(buffer1, sz);
        //Serial.println("sz > 0 && sz < 64");
        delay(1);
      }   
    }
    dclient.stop();
    fclose(myFile);
    remove(fs1); /////////////////////////REMOVE LINE///////////////////////////////////
    Serial.println(F("Data disconnected"));
    client.println("QUIT");
    if(!receiveBytes()) return 0;
    client.stop();
    Serial.println(F("Command disconnected"));
    dirIndexDef++;
  }
  dirIndex = 0;
  dirIndexDef = 1;
  transmitCounter = 1; ///////////////////
  return 1;
}

byte receiveBytes() {
  byte response; //to alert if there is an emergency
  byte readByte; //read bytes 128 at a time
  while(!client.available()) delay(1); //constantly try to see if client is available
  response = client.peek(); //read byte from file and NOT advance to next
  outCount = 0; //set initial counter to zero
  while(client.available()) {
    readByte = client.read(); //read byte from server
    Serial.write(readByte); 
    if(outCount < 127) {
      outBuf[outCount] = readByte; //load the read byte into buffer that will store 128 bytes at a time
      outCount++;
      outBuf[outCount] = 0; //then put next count with a temporary zero in the buffer
    }
  }
  if(response >= '4') {//trigger emergency and QUIT connection
    readByte = 0; //initially set to zero of read byte
    client.println(F("QUIT")); //QUIT statement for FTP
    while(!client.available()) delay(1); //constantly try to see if client is available
    while(client.available()) {
      readByte = client.read(); //load latest byte from server to client
      Serial.write(readByte);
    }
    client.stop(); //stop client connection to server
    Serial.println("Disconnected"); //TEST LINE REMOVE
    Serial.println("SD closed"); //TEST LINE REMOVE
    return 0; //this fails and will then request to recall recieveBytes()
  }
  return 1; //this passes and allows for client.print() in FTP server, want to keep state bitstream
}

void mountSD() {
  int err = fs.mount(&block_device); // return one if 1 if could not mount SD
    if (err) {
    // Reformat if we can't mount the filesystem
    // this should only happen on the first boot
    Serial.println("No filesystem found, please check on computer and manually format");
    err = fs.reformat(&block_device);  // seriously don't want to format good data
  }
  if (err) {
     Serial.println("Error formatting SDCARD ");
     while(1);
  }
}


void setupWiFi() {
  char ssid[] = "OnePlus 5T"; //network SSID (name) //80FB54 //rileyDesktop
  char pass[] = "riley123"; //network password //W2T76B2B01189 //teamargus2
  int status = WL_IDLE_STATUS; //WiFi radio status;

  //attempt to connect to WiFi network:
  while(status != WL_CONNECTED) {
    Serial.print("Attempting to connect to SSID: "); //TEST LINE REMOVE
    Serial.println(ssid); //TEST LINE REMOVE
    status = WiFi.begin(ssid, pass); //Connect to exclusively WPA/WPA2 network
    //delay(3 * 1000); //wait for WIFI_DELAY_SECONDS for connection indefinitely:
    //motionStart = NULL; //disable motion sensor
  }
  Serial.println("Connected to wifi"); //TEST LINE REMOVE
  //printWifiStatus(); //TEST LINE REMOVE
  //motionStart = HIGH; //enable motion sensor; 
}

void nameFile() {
  fileNameTemp[0] = iFile/10000000 % 10 + '0';
  fileNameTemp[1] = iFile/1000000 % 10 + '0';
  fileNameTemp[2] = iFile/100000 % 10 + '0';
  fileNameTemp[3] = iFile/10000 % 10 + '0';
  fileNameTemp[4] = iFile/1000 % 10 + '0';
  fileNameTemp[5] = iFile/100 % 10 + '0';
  fileNameTemp[6] = iFile/10 % 10 + '0';
  fileNameTemp[7] = iFile % 10 + '0';
  iFile++;
  Serial.print("I: "); Serial.println(iFile);
  Serial.print("FILE: "); Serial.println(fileNameTemp);
  explicit_bzero(fileName, 20);
  strcpy(fileName, ARGUS_PART_NUM);
  Serial.print("curr FILE: "); Serial.println(fileName);
  strcat(fileName, fileNameTemp);
  Serial.print("Concat String: "); Serial.println(fileName);
}

bool Motion_Detected (int queue[], int queue_length, int new_data, int threshold) {
  int scaledData = 0;
  if(new_data > 500) {
    scaledData = 1;
  } else {
    scaledData = 0;
  }
  for(int i = 0; i < queue_length-1; i++) {
    queue[i] = queue[i+1];
  }
  queue[queue_length-1] = scaledData;
  int sum = 0;
  for (int i = (queue_length-threshold-1); i<queue_length; i++) {
    sum += queue[i];
  }
  if(sum >= threshold) {
    return true;
  } else {
    return false;
  }
}

void Print_Motion_Queue(int motion_queue[]) {
  Serial.print(motion_queue[0]);
  Serial.print(" ");
  Serial.print(motion_queue[1]);
  Serial.print(" ");
  Serial.print(motion_queue[2]);
  Serial.print(" ");
  Serial.print(motion_queue[3]);
  Serial.print(" ");
  Serial.print(motion_queue[4]);
  Serial.print(" ");
  Serial.print(motion_queue[5]);
  Serial.print(" ");
  Serial.print(motion_queue[6]);
  Serial.print(" ");
  Serial.print(motion_queue[7]);
  Serial.print(" ");
  Serial.print(motion_queue[8]);
  Serial.print(" ");
  Serial.print(motion_queue[9]);
  Serial.print(" ");
}
