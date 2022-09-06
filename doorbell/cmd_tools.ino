#include "auth_tokens.h"
#include "types.h"

char calc_checksum(const char* packet, int len) {
  char sum = 0;

  for(int i = 0; i < len; i++) {
    sum += packet[i];
  }
  
  return sum;
}


bool is_valid_auth(const char* auth_token) {

  int num_valid_tokens = sizeof(VALID_AUTH_TOKENS) / sizeof(VALID_AUTH_TOKENS[0]);

  for(int i = 0; i < num_valid_tokens; i++) {
    if(!memcmp(auth_token, VALID_AUTH_TOKENS[i], AUTH_TOKEN_SIZE)) return true;
  }

  return false;
}


int validate_packet(const char* packet, int len) {
  /*
   * Returns 0 if the packet is valid
   */

  // Check length is correct
  int expected_len = (packet[1] << 8) + packet[2];
  if(len != expected_len) {
    Serial.printf("Packet was %d bytes but expected %d\n", len, expected_len);
    return 1;
  }

  // Verify checksum
  char checksum = calc_checksum(packet, len);
  if(checksum) {
    Serial.printf("Invalid checksum %d\n", checksum);
    return 2;
  }

  // Verify authentication token
  if(!is_valid_auth(&packet[3])) {  // Authentication token starts at byte 3
    Serial.printf("Invalid authentication token\n");
    return 3;
  }

  return 0;
}
