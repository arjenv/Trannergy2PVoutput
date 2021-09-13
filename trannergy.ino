/*********************************************************
 * 
 * Arjen Vellekoop
 * September 2021
 * Trannergy Solar logger to PVoutput.org
 * 
 *
 * 
 *  Respons format Trannergy or Omnik
 *  
 *  Seems to be ready for 3 solar strings
 *  
 *  
 *  BYTE                                              nr of bytes
 *   0:   ? (start message?)                          3
 *   4:   Twice SerialNumber of the datalogger        8
 *  12:   ?
 *  15:   SerialNumber Inverter                       16
 *  31:   temperature * 10                            2
 *  33:   Vpv1 * 10                                   2
 *  35:   Vpv2 * 10                                   2
 *  37:   ? Vpv3 *10                                  2
 *  39:   Probably Ipv1                               2
 *  41:   Probably Ipv2                               2
 *  43:   Probably Ipv3                               2
 *  45:   Iac1 * 10                                   2
 *  47:   Probably Iac2 * 10                          2
 *  49:   Probably Iac3 * 10                          2
 *  51:   Vac1 * 10                                   2
 *  53:   Probably Vac2 * 10                          2
 *  55:   Probably Vac3 * 10                          2
 *  57:   Fac * 100 (gridfrequency)                   2
 *  59:   Pac1                                        2
 *  61:   Probably Pac2                               2
 *  63:   Probably Pac3                               2
 *  65:   ??                                          4
 *  69:   Daily yield kWh * 100                       2
 *  71:   ??                                          2
 *  73:   Total yield in kWh * 10 since reset         2
 *  75:   Might be 2 extra bytes for the total yield  2
 *  77:   On-time                                     2
 *  79 -96 ???
 *  97:   Might be checksum                           1
 *  98:   ? Seems to be always the same (Omnik)
 * 103:   2x serial Number datalogger (Omnik)         8
 * 111:   "DATA SEND IS OK" (Omnik)         
 * 126:   ?? End message?                             2
 *
 ***********************************************************/
 
 
 
 
 /****LIBRARIES**************************************************/
#include "secrets.h"
#include <OneWire.h> 
#include <DallasTemperature.h>
#include <ESP8266WiFi.h>
#include <time.h>

/****TEMPEARTURE INIT********************************************/
// Temperature init. DS18B20 at pin 14
#define ONE_WIRE_BUS 14 
OneWire oneWire(ONE_WIRE_BUS); 
DallasTemperature sensors(&oneWire);

/**GLOBAL VARIABLES************************************************/
byte magicmessage[] = {0x68, 0x02, 0x40, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x16};
long serial_number = TrannergySN;
long SN;
unsigned char  InverterID[20];
unsigned char  server_reply[256];
int i, response_length;

float Trannergy_temperature;
float PVVoltageDC;
float IVCurrentDC;
float PVVoltageAC;
float IVCurrentAC;
float frequency;
float PVPower;
float PowerToday;
float TotalPower;
float TotalHours;

int timezone = 1 * 3600;
int dst = 1 * 3600;

char current_date[10], current_time[10];
unsigned long lasttime;

/*
String weekday;
char daysOfTheWeek[7][10] = {"Sunday", "Monday", "Tuesday", "Wednesday", "Thursday", "Friday", "Saturday"};
char Thismonth[12][10] = {"January", "February", "March", "April", "May", "June", "July", "August", "September", "October", "November", "December"};
*/

/******SETUP******************/
void setup(void) 
{ 
  int checksum=0;
  Serial.begin(115200);            // start serial port 
  sensors.begin(); // Start temp sensor
  sensors.setResolution(9);
  WiFi.mode(WIFI_STA);

  //connect to your local wi-fi network
  WiFi.begin(SECRET_SSID, SECRET_PASS);

  //check wi-fi is connected to wi-fi network
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print(".");
  }
  
  Serial.println();
  Serial.print("WiFi connected..! ");
  Serial.print(SECRET_SSID);
  Serial.print(" ");
  Serial.println(WiFi.localIP());

  // generate the magic message
  // first 4 bytes are fixed x68 x02 x40 x30
  // next 8 bytes are the reversed serial number twice(hex)
  // next 2 bytes are fixed x01 x00
  // next byte is a checksum (2x each binary number from the serial number + 115)
  // last byte is fixed x16
  
  for (i=0; i<4; i++) {
    magicmessage[4+i] = ((serial_number>>(8*i))&0xff);
    magicmessage[8+i] = ((serial_number>>(8*i))&0xff);
    checksum += magicmessage[4+i];
  }
  magicmessage[14] = ((checksum*2 + 115)&0xff);

  if (DEBUG) {
    for (i=0;i<16;i++) {
      Serial.print(magicmessage[i],HEX);
      Serial.print(" ");
    }
    Serial.println();
  }

  /*********** TIMEZONE ********************/
  configTime(timezone, dst, "pool.ntp.org","time.nist.gov");
  setenv("TZ", "CET-1CEST-2,M3.5.0/02:00:00,M10.5.0/03:00:00", 1);
  tzset();
  Serial.println("\nWaiting for NTP...");

  time_t now = time(nullptr);
  while(now < 1631544104){  // sept 2021
     delay(1000);            // wait a second
     now = time(nullptr);
     Serial.print(now); Serial.print(" ");
  }
  Serial.println();
  
} 

/************** LOOP *******************/

void loop(void) {
  unsigned errflag;
  unsigned long timeout;
  
  sensors.requestTemperatures(); // Send the command to get temperature readings 
  Serial.print("Temperature is: "); 
  Serial.println(sensors.getTempCByIndex(0));

  time_t now = time(nullptr);
  
  struct tm* p_tm = localtime(&now);

  if (DEBUG) {
    Serial.print("year: ");Serial.println(p_tm->tm_year);
    Serial.print("Month: ");Serial.println(p_tm->tm_mon);
    Serial.print("day: ");Serial.println(p_tm->tm_mday);
    Serial.print("hour: ");Serial.println(p_tm->tm_hour);
    Serial.print("minutes: ");Serial.println(p_tm->tm_min);
    Serial.print("secs: ");Serial.println(p_tm->tm_sec);
  }

  // some date/time formatting we'll need for PVOUTPUT
  strftime(current_date, sizeof current_date, "%Y%m%d", p_tm); 
  Serial.print(current_date);
  Serial.print(" ");
  strftime(current_time, sizeof current_time, "%H:%M", p_tm);
  Serial.println(current_time);

  
  WiFiClient client;
  if (client.connect(TrannergyURL, TrannergyPort)) {
    Serial.println("Connected to Inverter!");
    delay(100);
    client.write((const uint8_t*)magicmessage, (uint8_t) 16);
    Serial.println();

    errflag = 0;
    timeout = millis();
    while (!client.available() && !errflag) {
      if (millis() - timeout > 5000) {
        Serial.println(">>> Client Timeout !");
        client.stop();
        errflag = 1;
      }
    }
    if (!errflag) {
      // Read all the lines of the reply from server and print them to Serial
      response_length = 0;
      while (client.available() && (response_length<255)) {
        server_reply[response_length] = client.read();
        if (DEBUG) {
          Serial.printf("%i\t", response_length);
          Serial.print(server_reply[response_length],HEX);
          if ((server_reply[response_length] > '/') && (server_reply[response_length] < '{')) {
            Serial.printf("\t%c", server_reply[response_length]);
          }
          Serial.println();
        }
        response_length++;
        if (response_length>255){
          Serial.print("response longer than expected!");
          errflag = 2;
        }
        delay(10); // seems necessary for all the bytes to arrive
      }
      Serial.printf("Total bytes received: %i\n\n", response_length);
      client.stop();

      if (!errflag) {   // Extract the values from the reponse (if any)
        SN=0;
        Serial.print("Serial Number: ");
        for (i=0; i<4; i++) {
          Serial.printf("%x", (unsigned char) server_reply[7-i]);
          SN += (long) (unsigned char) server_reply[7-i] * pow(256, (3-i));
        }
        Serial.printf("\t\t%ld\n", SN);
        strncpy((char *) InverterID, (const char *) &server_reply[15], 16);
        InverterID[16] = 0;
        Serial.printf("ID Inverter: \t\t\t%s\n", InverterID);

        Trannergy_temperature = ctonr(&server_reply[31], 2, 10);
        PVVoltageDC =           ctonr(&server_reply[33], 2, 10);
        IVCurrentDC =           ctonr(&server_reply[39], 2, 10);
        PVVoltageAC =           ctonr(&server_reply[51], 2, 10);
        IVCurrentAC =           ctonr(&server_reply[45], 2, 10);
        PVPower =               ctonr(&server_reply[59], 2, 1);
        frequency =             ctonr(&server_reply[57], 2, 100);
        PowerToday =      1000* ctonr(&server_reply[69], 2, 100);
        TotalPower =            ctonr(&server_reply[71], 4, 10);
        TotalHours =            ctonr(&server_reply[75], 4, 1);

        Serial.printf("Temperature:\t\t\t%.1f\n", Trannergy_temperature);
        Serial.printf("PV1 Voltage (DC):\t\t%.1f\n", PVVoltageDC);
        Serial.printf("IV1 Voltage (DC):\t\t%.1f\n", IVCurrentDC);
        Serial.printf("PV1 Voltage (AC):\t\t%.1f\n", PVVoltageAC);
        Serial.printf("IV1 Voltage (AC):\t\t%.1f\n", IVCurrentAC);
        Serial.printf("PV1 Power:\t\t\t%.0f\n", PVPower);
        Serial.printf("Frequency (AC):\t\t\t%.2f\n", frequency);
        Serial.printf("Total Power today (Wh):\t\t%.0f\n", PowerToday);
        Serial.printf("Total Power since reset (kWh):\t%.1f\n", TotalPower);
        Serial.printf("Total Hours since reset:\t%.0f\n\n", TotalHours);
      }
    }
  }

  Serial.print("Last Upload since (secs) : ");
  Serial.println(now-lasttime);
  
  if (((now-lasttime) > 300) && (!errflag)) {    // every 5 minutes upload data
    lasttime = now;
    
    // the curse for PVOutput:
    char pvstring[200];
    sprintf(pvstring, "GET %s?d=%s&t=%s&v1=%.0f&v2=%.0f&v5=%.1f&v6=%.1f&key=%s&sid=%d HTTP/1.1\r\nHost: %s \r\nConnection: close\r\n\r\n",
                PVoutputURL,
                current_date,
                current_time,
                PowerToday,
                PVPower,
                Trannergy_temperature,
                PVVoltageDC,
                PVoutputapi, 
                SystemID,
                HostURL);

    if (DEBUG) {
      Serial.println(pvstring);
    }
    Serial.println("Trying to connect to PVOutput..");

    WiFiClient client;
    const int httpPort = 80;
    if (!client.connect(HostURL, httpPort)) {
      Serial.println("connection failed");
    }
    else {
      Serial.println("PVOutput Connected!");
      // This will send the request to the server
      client.print(pvstring);
      errflag = 0;
      timeout = millis();
      while (!client.available() && !errflag) {
        if (millis() - timeout > 5000) {
          Serial.println(">>> Client Timeout !");
          client.stop();
          errflag = 1;
        }
      }
      if (!errflag) {
        // Read all the lines of the reply from server and print them to Serial
        while(client.available()) {
          String line = client.readStringUntil('\n');
          Serial.println(line);
        }
        Serial.println();
        Serial.println("Disconnect PVOutput....");
        Serial.println();
        client.stop();
      }
      else {
      // Parser error, print error
        Serial.println(errflag);
      }
    }
  }
  delay(59000); //59 sec
}
        

/**************** FUNCTIONS ******************/

float ctonr(unsigned char * src, int nrofbytes, int div) {
  int i, flag=0;
  float sum=0;

//  sanity check
  if (nrofbytes<=0 || nrofbytes>4) 
    return -1;

  for (i=nrofbytes; i>0; i--) {
    sum += (float) (src[i-1] * pow(256, nrofbytes-i));
    if (src[i-1] == 0xff)
      flag++;
  }
  if (flag == nrofbytes) // all oxff
    sum = 0;
  sum /= (float) div;
  return sum;
}
