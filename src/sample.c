#include <stdio.h>
#include <x68k/iocs.h>

#include "mdpad.h"

const char* PAD_TYPE[] = { "ATARI", "MD3B", "MD6B" };

int main(void) {
  int usp = _iocs_b_super(0);
  while(1){
    mdpad_type_t t = mdpad_detect(MDPAD_PORT_A);
    mdpad_state_t st = (t == MDPAD_MD6) ? mdpad_read6(MDPAD_PORT_A)
                                        : mdpad_read3(MDPAD_PORT_A);
    printf("\r[%s] bits=0x%04X        ", PAD_TYPE[(int)t], st.bits);
  }
  _iocs_b_super(usp);
  return 0;
}
