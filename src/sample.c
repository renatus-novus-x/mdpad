#include <stdio.h>
#include <x68k/iocs.h>

#include "mdpad.h"

const char* PORT_STRING[] = { "JOY STICK 1", "JOY STICK 2"};
const char* PAD_STRING[] = { "ATARI", "MD3B", "MD6B" };

int main(int argc, char* argv[]) {
  int usp = _iocs_b_super(0);
  while(1){
    mdpad_type_t t = mdpad_detect(MDPAD_PORT_A);
    mdpad_state_t st = (t == MDPAD_MD6) ? mdpad_read6(MDPAD_PORT_A)
                                        : mdpad_read3(MDPAD_PORT_A);
    printf("\r[%s][%s] bits=0x%04X        ", PORT_STRING[MDPAD_PORT_A], PAD_STRING[(int)t], st.bits);
  }
  _iocs_b_super(usp);
  return 0;
}
