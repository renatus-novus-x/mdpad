#include <stdio.h>
#include <x68k/iocs.h>

#include "mdpad.h"

int main(void) {
  int usp = _iocs_b_super(0);
  mdpad_type_t t = mdpad_detect(MDPAD_PORT_A);
  mdpad_state_t st = (t == MDPAD_MD6) ? mdpad_read6(MDPAD_PORT_A)
                                      : mdpad_read3(MDPAD_PORT_A);
  printf("type=%d bits=0x%04X\n", (int)t, st.bits);
  _iocs_b_super(usp);
  return 0;
}
