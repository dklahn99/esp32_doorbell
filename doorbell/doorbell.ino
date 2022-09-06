#include <WiFi.h>
#include <driver/dac.h>
#include <FS.h>
#include <SPIFFS.h>

#include "types.h"

//
// ---------- COMMAND PROTOCOL ----------
//
// Each audio file stored in SPIFFS filesystem with "<integer>.wav" filename
// Audio files should be unsigned 8-bit PCM .wav files with sample rate = 32000 samples/s
//
// Command protocol
// Byte 0: 1-byte message checksum
// Bytes [1:3]: the number of bytes in the packet, including the checksum. Byte 4 is the MSB
// Bytes [3:11]: 8-byte authentication token. Byte 10 is the MSB
// Byte 11: is the command number
//
// Command 1: Print bytes [12:] to serial monitor
//
// Command 2: Ring
//    Byte 12: file id to play
//
// Command 3: Create file
//    Byte 12: file id
//    Bytes [13:]: file contents
//
// Command 4: Append to file
//    Byte 12: file id
//    Bytes [13:]: file contents
//
// Command 5: Delete file
//    Byte 12: file id. If the file id is 0xFF, delete all files
//
// Command 6: Format filesystem
//
// Command 7: Add authentication token  // TODO
// 
// Command 8: Remove authentication token // TODO
//
//
//
// CHECKSUM:
//    The checksum is a 8-bit sum complement checksum of the
//    entire packet including metadata
//    (https://en.wikipedia.org/wiki/Checksum#Sum_complement)
//


// Audio Declarations
const dac_channel_t DAC_CHANNEL = DAC_CHANNEL_2;

const int FILENAME_SIZE = 32;
const int AUDIO_SAMPLE_RATE = 32000;  // Samples/s
const int AUDIO_SAMPLE_PERIOD = (1000000 / AUDIO_SAMPLE_RATE) - 1;  // microseconds/sample


// WiFi Connection Declarations
char network[] = "Router I Hardly Know Her";
char password[] = "geolemurs";
IPAddress IP(192, 168, 4, 69);
IPAddress GATEWAY(192, 168, 4, 1);
IPAddress SUBNET(255, 255, 252, 0);

WiFiServer server(80);


void setup() {
	Serial.begin(115200);

	// Setup pins
	dac_output_enable(DAC_CHANNEL);
	analogReadResolution(12);
	adcAttachPin(VIN_PIN);
  pinMode(AUDIO_ENABLE_PIN, OUTPUT);
  digitalWrite(AUDIO_ENABLE_PIN, LOW);

	// Setup Filesystem
	if(!SPIFFS.begin(true)){
		Serial.println("SPIFFS Mount Failed");
		return;
	}

	listDir(SPIFFS, "/", 0);

	//
	// Connect to WIFI
	//
	WiFi.begin(network, password);
	WiFi.config(IP, GATEWAY, SUBNET);

	uint8_t count = 0; //count used for Wifi check times
	Serial.print("Attempting to connect to ");
	Serial.println(network);
	while (WiFi.status() != WL_CONNECTED && count < 6) { //can change this to more attempts
		delay(500);
		Serial.print(".");
		count++;
	}
	delay(1000);  //acceptable since it is in the setup function.
	if (WiFi.isConnected()) { //if we connected then print our IP, Mac, and SSID we're on
		Serial.println("CONNECTED!");
		Serial.printf("%d:%d:%d:%d (%s) (%s)\n", WiFi.localIP()[3], WiFi.localIP()[2],
					  WiFi.localIP()[1], WiFi.localIP()[0],
					  WiFi.macAddress().c_str() , WiFi.SSID().c_str());
		delay(500);
	} else { //if we failed to connect just Try again.
		Serial.println("Failed to Connect :/  Going to restart");
		Serial.println(WiFi.status());
		ESP.restart(); // restart the ESP (proper way)
	}

	IPAddress myAddress = WiFi.localIP();
	Serial.print("IP: ");
	Serial.println(myAddress);

	server.begin();

}

//int max_val = 0;
void loop() {

  check_for_commands();
	Action action = update_state_machine();

	switch(action) {
		case ACTION_RING:
			Serial.println("Got ACTION_RING");
      char filename[FILENAME_SIZE];
      get_rand_file(SPIFFS, filename, FILENAME_SIZE);
      play_audio(filename);
			break;
	}
 
}


void check_for_commands() {

  static const int BUFF_SIZE = 4096;
  static char packet[BUFF_SIZE];

  static const int RESP_SIZE = 1024;
  static char response[RESP_SIZE];

  WiFiClient remoteClient = server.available();
  
  if (remoteClient) {
    Serial.println("Got client");

    int read_len = 0;
    memset(packet, 0, BUFF_SIZE);

    while (remoteClient.available() && read_len < (BUFF_SIZE - 1)) {    
      char read_byte = remoteClient.read();      
      packet[read_len] = read_byte;
      read_len++;
    }
    
    memset(response, 0, RESP_SIZE);

    // Response based on packet success
    switch(validate_packet(packet, read_len)) {
      case 0:
        execute_command(packet, read_len);
        sprintf(response, "ACK\r\n");
        break;  // Packet is valid

      case 1:
        sprintf(response, "Packet length did not match expected");
        break;

      case 2:
        sprintf(response, "Invalid checksum");
        break;

      case 3:
        sprintf(response, "Invalid authentication token");
        break;
    }

    remoteClient.print(response);
    remoteClient.stop();
    Serial.println("Disconnected from client");
  }  
}


void execute_command(char* packet, int len) {

  Serial.printf("Executing command: [%d, %d]: %d bytes\n", packet[11], packet[12], len);

  packet[len] = 0;  // Append the null terminator for commands that expect the packet to end in a string. Kinda hacky but whatever

  // Delegate command handling
  int cmd = packet[11];
  switch(cmd) {
    case 1: {
      Serial.print("Print command: ");
      Serial.println((packet + 12));
      break;
    }
      
    case 2: {
      char filename[FILENAME_SIZE];
      sprintf(filename, "/%d.wav", packet[12]);
      play_audio(filename);
      break;
    }
      
    case 3: {
      char filename[FILENAME_SIZE];
      sprintf(filename, "/%d.wav", packet[12]);
      writeFile(SPIFFS, filename, &packet[13]); 
      break;
    }

    case 4: {
      char filename[FILENAME_SIZE];
      sprintf(filename, "/%d.wav", packet[12]);
      appendFile(SPIFFS, filename, &packet[13]);
      break;
    }

    case 5: {
      // TODO: 0xFF deletes all files
      char filename[FILENAME_SIZE];
      sprintf(filename, "/%d.wav", packet[12]);
      deleteFile(SPIFFS, filename);
      break;
    }

    case 6: {
      formatFS(SPIFFS);
      break;
    }
      
    default:
      Serial.printf("Unrecognized command: %d", cmd);
      break;
  }
}


void play_audio(const char* filename) {
  
  Serial.printf("Play audio filename: %s\r\n", filename);

  File file = SPIFFS.open(filename);
  Serial.printf("Addr of file: %d\n\r", &file);

  if(!file) {
    Serial.println("Error opening file :(");
    return;
  }
  
  int BUFF_SIZE = 520;
  int NUM_TO_READ = 512;
  uint8_t audio_buff[BUFF_SIZE];
  int num_read = file.read(audio_buff, NUM_TO_READ);

  digitalWrite(AUDIO_ENABLE_PIN, HIGH);
  int audio_start_time = micros();
  int audio_end_time;
  while(true) {

    int sample_start_time = micros();
    for(int i = 0 ; i < num_read; i++) {
      uint8_t sample = audio_buff[i];
      dac_output_voltage(DAC_CHANNEL, sample);

      while(micros() - sample_start_time < AUDIO_SAMPLE_PERIOD);
      sample_start_time = micros();
    }

    if(file.available()) {
      num_read = file.read(audio_buff, NUM_TO_READ);
    } else {
      audio_end_time = micros();
      dac_output_voltage(DAC_CHANNEL, 0);

      Serial.printf("Done playing %d bytes. Took %d us, target = %d\n", file.size(), audio_end_time - audio_start_time, AUDIO_SAMPLE_PERIOD * file.size());
      file.close();
      digitalWrite(AUDIO_ENABLE_PIN, LOW);
      return;
    }
  }
}
