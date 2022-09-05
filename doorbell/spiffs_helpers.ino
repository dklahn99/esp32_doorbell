#include <FS.h>
#include <SPIFFS.h>

/* You only need to format SPIFFS the first time you run a
   test or else use the SPIFFS plugin to create a partition
   https://github.com/me-no-dev/arduino-esp32fs-plugin */

int count_files(fs::FS &fs, const char* dir_name) {
  File root = fs.open(dir_name);
  if(!root) {
    Serial.println("ERROR counting files: root not valid");
    return -1;
  }
  if(!root.isDirectory()){
    Serial.println("ERROR counting files: root not a directory");
    return -1;
  }

  File file = root.openNextFile();
  int count = 0;
  for(; file; count++) {
    file = root.openNextFile();
  }

  file.close();
  root.close();
  return count;
}

void get_rand_file(fs::FS &fs, char* file_name, int len) {
  File root = fs.open("/");
  if(!root) {
    Serial.println("ERROR counting files: root not valid");
    return;
  }
  if(!root.isDirectory()){
    Serial.println("ERROR counting files: root not a directory");
    return;
  }

  int chosen = random(count_files(fs, "/"));
  File file = root.openNextFile();
  for(int i = 0; i != chosen && file; i++) {
    file = root.openNextFile();
  }

  sprintf(file_name, "/%s", file.name());

  file.close();
  root.close();
}

void listDir(fs::FS &fs, const char * dirname, uint8_t levels){
    Serial.printf("Listing directory: %s\r\n", dirname);

    File root = fs.open(dirname);
    if(!root){
        Serial.println("- failed to open directory");
        return;
    }
    if(!root.isDirectory()){
        Serial.println(" - not a directory");
        return;
    }

    File file = root.openNextFile();
    while(file){
        if(file.isDirectory()){
            Serial.print("  DIR : ");
            Serial.println(file.name());
            if(levels){
                listDir(fs, file.path(), levels -1);
            }
        } else {
            Serial.print("  FILE: ");
            Serial.print(file.name());
            Serial.print("\tSIZE: ");
            Serial.println(file.size());
        }
        file = root.openNextFile();
    }
    root.close();
}

void readFile(fs::FS &fs, const char * path, int max_to_read){
    Serial.printf("Reading file: %s\r\n", path);

    File file = fs.open(path);
    if(!file || file.isDirectory()){
        Serial.println("- failed to open file for reading");
        return;
    }

    Serial.println("- read from file:");
    int num_read = 0;
    while(file.available() && num_read <= max_to_read){
        Serial.print((int) file.read());
        Serial.print(", ");
        num_read++;
    }
    file.close();
}

void writeFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Writing file: %s\r\n", path);

    File file = fs.open(path, FILE_WRITE);
    if(!file){
        Serial.println("- failed to open file for writing");
        return;
    }
    if(file.print(message)){
        Serial.println("- file written");
    } else {
        Serial.println("- write failed");
    }
    file.close();
}

void appendFile(fs::FS &fs, const char * path, const char * message){
    Serial.printf("Appending to file: %s\r\n", path);

    File file = fs.open(path, FILE_APPEND);
    if(!file){
        Serial.println("- failed to open file for appending");
        return;
    }
    if(file.print(message)){
        Serial.println("- message appended");
    } else {
        Serial.println("- append failed");
    }
    file.close();
}

void renameFile(fs::FS &fs, const char * path1, const char * path2){
    Serial.printf("Renaming file %s to %s\r\n", path1, path2);
    if (fs.rename(path1, path2)) {
        Serial.println("- file renamed");
    } else {
        Serial.println("- rename failed");
    }
}

void deleteFile(fs::FS &fs, const char * path){
    Serial.printf("Deleting file: %s\r\n", path);
    if(fs.remove(path)){
        Serial.println("- file deleted");
    } else {
        Serial.println("- delete failed");
    }
}

void formatFS(fs::FS &fs) {
  Serial.printf("Formatting file system...\r\n");
  bool formatted = SPIFFS.format();
  if(formatted){
      Serial.println("\n\nSuccess formatting");
  }else{
      Serial.println("\n\nError formatting");
  }
}
