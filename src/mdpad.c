#include <stdio.h>
#include <x68k/iocs.h>

#include "mdpad.h"

const char* PORT_STRING[] = { "#1", "#2"};
const char* PAD_STRING[]  = { "  2B", "MD3B", "MD6B" };

void mdpad_bits_to_binary(uint16_t bits, char out[13]) {
  static const uint16_t order[12] = {
    MDPAD_MODE, MDPAD_Z, MDPAD_Y, MDPAD_X, MDPAD_START, MDPAD_C, MDPAD_B, MDPAD_A, MDPAD_RIGHT, MDPAD_LEFT, MDPAD_DOWN, MDPAD_UP
  };
  for (int i = 0; i < 12; ++i){
    out[i] = (bits & order[i]) ? '1' : '0';
  }
  out[12] = '\0';
}
int main(int argc, char* argv[]) {
  int usp = _iocs_b_super(0);
  printf("           MZYXSCBARLDU\n");
  printf("------------------------\n");
  while(1){
    mdpad_port_t  port  = MDPAD_PORT_A;
    mdpad_type_t  type  = mdpad_detect(port);
    mdpad_state_t state = (type == MDPAD_MD6) ? mdpad_read6(port)
                                              : mdpad_read3(port);
    char bin[13];
    mdpad_bits_to_binary(state.bits, bin);
    printf("\r[%s][%s][%s]        ", PORT_STRING[port], PAD_STRING[(int)type], bin);
  }
  _iocs_b_super(usp);
  return 0;
}
