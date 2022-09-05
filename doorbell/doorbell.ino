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
// Byte 0: the MSB of number of bytes in command
// Byte 1: the LSB of number of bytes in command
// Byte 2: is command number
//
// Command 1: Print bytes [3:] to serial monitor
//
// Command 2: Ring
//    Byte 3: file id to play
//
// Command 3: Create file
//    Byte 3: file id
//    Bytes [4:]: file contents
//
// Command 4: Append to file
//    Byte 3: file id
//    Bytes [4:]: file contents
//
// Command 5: Delete file
//    Byte 3: file id
//
// Command 6: Format filesystem
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

  static const int BUFF_SIZE = 4095;
  static char command_buff[BUFF_SIZE];

  WiFiClient remoteClient = server.available();
  command_buff[0] = 0;  // Reset null terminator
  
  if (remoteClient) {
    Serial.println("Got client");

    int buff_ptr = 0;
    memset(command_buff, 0, BUFF_SIZE);

    char str[40];
    while (remoteClient.available() && buff_ptr < (BUFF_SIZE - 1)) {    
      char read_byte = remoteClient.read();      
      command_buff[buff_ptr] = read_byte;
      buff_ptr++;
    }

    // Verify all bytes were received
    int num_expected = (command_buff[0] << 8) + command_buff[1];
    if(buff_ptr != num_expected) {
      Serial.print("Error: buffer length incorrect. Expected ");
      Serial.print(num_expected);
      Serial.print(" but length was ");
      Serial.println(buff_ptr);
      return;
    }
    
    Serial.printf("Command: [%d, %d, %d]: %d bytes\n", command_buff[0], command_buff[1], command_buff[2], buff_ptr);

    // Delegate command handling
    int cmd = command_buff[2];
    switch(cmd) {
      case 1: {
        Serial.print("Print command: ");
        Serial.println((command_buff + 3));
        break;
      }
        
      case 2: {
        Serial.println("Ring command...");
        char filename[FILENAME_SIZE];
        sprintf(filename, "/%d.wav", command_buff[3]);
        play_audio(filename);
        break;
      }
        
      case 3: {
        char filename[FILENAME_SIZE];
        sprintf(filename, "/%d.wav", command_buff[3]);
        writeFile(SPIFFS, filename, &command_buff[4]); 
        break;
      }

      case 4: {
        char filename[FILENAME_SIZE];
        sprintf(filename, "/%d.wav", command_buff[3]);
        appendFile(SPIFFS, filename, &command_buff[4]);
        break;
      }

      case 5: {
        char filename[FILENAME_SIZE];
        sprintf(filename, "/%d.wav", command_buff[3]);
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

    remoteClient.print("ack");
    remoteClient.stop();
    Serial.println("Disconnected from client");
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
