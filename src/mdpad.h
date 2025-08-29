/* mdpad.h  X68000 MegaDrive pad helper (header-only)
 * Toolchain: elf2x68k
 * Notes:
 *  - Supervisor mode required to touch 0xE9A00x.
 */

#ifndef MDPAD_H_
#define MDPAD_H_

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* I/O addresses (X68000) */
#define MDPAD_IO_PA   0x00E9A001  /* Port A data */
#define MDPAD_IO_PB   0x00E9A003  /* Port B data */
#define MDPAD_IO_PC   0x00E9A007  /* Port C control (8255 BSR) */

/* BSR codes for TH line (PC4/PC5) */
#define MDPAD_BSR_A_TH_LOW   0x08  /* PC4 reset  -> TH=0 */
#define MDPAD_BSR_A_TH_HIGH  0x09  /* PC4 set    -> TH=1 */
#define MDPAD_BSR_B_TH_LOW   0x0A  /* PC5 reset  -> TH=0 */
#define MDPAD_BSR_B_TH_HIGH  0x0B  /* PC5 set    -> TH=1 */

typedef enum { MDPAD_PORT_A = 0, MDPAD_PORT_B = 1 } mdpad_port_t;
typedef enum { MDPAD_ATARI = 0, MDPAD_MD3 = 1, MDPAD_MD6 = 2 } mdpad_type_t;

#define MDPAD_STABLE_READS      (3)
#define MDPAD_STABLE_MAX_ITERS (64)

static inline uint8_t __mdpad_read_stable(volatile uint8_t* rd) {
  uint8_t prev = *rd, cur;
  int stable = 0, tries = 0;
  do {
    cur = *rd;
    stable = (cur == prev) ? (stable + 1) : 0;
    prev = cur;
  } while (stable < MDPAD_STABLE_READS && ++tries < MDPAD_STABLE_MAX_ITERS);
  return cur; /* raw (active-low) */
}

static inline void __mdpad_barrier(void){ __asm__ volatile("" ::: "memory"); }

/* Normalized bit masks (pressed = 1) */
enum {
  MDPAD_UP    = 1u << 0,
  MDPAD_DOWN  = 1u << 1,
  MDPAD_LEFT  = 1u << 2,
  MDPAD_RIGHT = 1u << 3,
  MDPAD_A     = 1u << 4,
  MDPAD_B     = 1u << 5,
  MDPAD_C     = 1u << 6,
  MDPAD_START = 1u << 7,
  MDPAD_X     = 1u << 8,  /* extended (6-button) */
  MDPAD_Y     = 1u << 9,
  MDPAD_Z     = 1u << 10,
  MDPAD_MODE  = 1u << 11
};

typedef struct {
  uint16_t bits;   /* OR of MDPAD_* */
} mdpad_state_t;

/* Raw samples used for 6-button reads (TH=0/1 repeated) */
typedef struct {
  /* Sequence:
   * raw[0]: TH=0, raw[1]: TH=1,
   * raw[2]: TH=0, raw[3]: TH=1,
   * raw[4]: TH=0, raw[5]: TH=1
   * (each still ACTIVE-LOW at the bus; we return them already inverted)
   */
  uint8_t raw[6];
} mdpad_raw6_t;

/* ---- Helpers: choose address & BSR pair by port ---- */
static inline volatile uint8_t* __mdpad_data_ptr(mdpad_port_t p) {
  return (volatile uint8_t*)(p == MDPAD_PORT_A ? MDPAD_IO_PA : MDPAD_IO_PB);
}
static inline uint8_t __mdpad_bsr_low(mdpad_port_t p)  {
  return (p == MDPAD_PORT_A) ? MDPAD_BSR_A_TH_LOW  : MDPAD_BSR_B_TH_LOW;
}
static inline uint8_t __mdpad_bsr_high(mdpad_port_t p) {
  return (p == MDPAD_PORT_A) ? MDPAD_BSR_A_TH_HIGH : MDPAD_BSR_B_TH_HIGH;
}

/* ---- Detect pad type  ---- */
static inline mdpad_type_t mdpad_detect(mdpad_port_t port) {
  volatile uint8_t* rd = __mdpad_data_ptr(port);       /* $E9A001 or $E9A003 */
  volatile uint8_t* pc = (volatile uint8_t*)MDPAD_IO_PC; /* $E9A007 */
  const uint8_t low  = __mdpad_bsr_low(port);          /* A:#8 / B:#0A */
  const uint8_t high = __mdpad_bsr_high(port);         /* A:#9 / B:#0B */

  uint8_t d;

  *pc = low;  __mdpad_barrier(); d = __mdpad_read_stable(rd);
  *pc = high; __mdpad_barrier(); d = __mdpad_read_stable(rd);
  *pc = low;  __mdpad_barrier(); d = __mdpad_read_stable(rd);
  *pc = high; __mdpad_barrier(); d = __mdpad_read_stable(rd);
  *pc = low;  __mdpad_barrier(); d = __mdpad_read_stable(rd);  /* final */

  d ^= 0xFF;                 /* active-low ¨ active-high */
  uint8_t id = (uint8_t)(d & 0x0F);   /* use low nibble */

  if (id == 0x0F)                 return MDPAD_MD6;   /* 6-button */
  if ((id & 0x0C) == 0x0C)        return MDPAD_MD3;   /* 3-button */
  return MDPAD_ATARI;                                   /* fallback */
}

/* ---- 3-button read: return normalized mdpad_state_t ----
 * Uses two samples (TH=1 then TH=0) and maps:
 *  TH=1 -> B,C,  TH=0 -> A,START,  U/D/L/R in both.
 */
static inline mdpad_state_t mdpad_read3(mdpad_port_t port) {
  volatile uint8_t* rd = __mdpad_data_ptr(port);
  volatile uint8_t* pc = (volatile uint8_t*)MDPAD_IO_PC;
  const uint8_t low  = __mdpad_bsr_low(port);
  const uint8_t high = __mdpad_bsr_high(port);

  *pc = low;  __mdpad_barrier(); (void)*rd;                              /* dummy */
  *pc = high; __mdpad_barrier(); uint8_t s_hi = __mdpad_read_stable(rd); /* TH=1 */
  *pc = low;  __mdpad_barrier(); uint8_t s_lo = __mdpad_read_stable(rd); /* TH=0 */

  s_hi = (uint8_t)~s_hi;  /* active-low -> pressed=1 */
  s_lo = (uint8_t)~s_lo;

  mdpad_state_t st = {0};

  if (s_hi & (1u << 0)) st.bits |= MDPAD_UP;   /* Direction (bits 0..3 in both reads) */
  if (s_hi & (1u << 1)) st.bits |= MDPAD_DOWN;
  if (s_hi & (1u << 2)) st.bits |= MDPAD_LEFT;
  if (s_hi & (1u << 3)) st.bits |= MDPAD_RIGHT;
  if (s_hi & (1u << 5)) st.bits |= MDPAD_B;    /* TH=1:[B][C]     */
  if (s_hi & (1u << 6)) st.bits |= MDPAD_C;
  if (s_lo & (1u << 5)) st.bits |= MDPAD_A;    /* TH=0:[A][START] */
  if (s_lo & (1u << 6)) st.bits |= MDPAD_START;

  return st;
}

/* ---- 6-button raw read
 * Sequence of 6 reads as: 0/1/0/1/0/1. Results are returned
 * ALREADY INVERTED (pressed=1) in raw[0..5].
 *
 */
static inline mdpad_raw6_t mdpad_read6_raw(mdpad_port_t port) {
  volatile uint8_t* rd = __mdpad_data_ptr(port);
  volatile uint8_t* pc = (volatile uint8_t*)MDPAD_IO_PC;
  const uint8_t low  = __mdpad_bsr_low(port);
  const uint8_t high = __mdpad_bsr_high(port);
  mdpad_raw6_t out;
  *pc = high; __mdpad_barrier(); (void)__mdpad_read_stable(rd);                  // dummy for stable read
  *pc = low;  __mdpad_barrier(); out.raw[0] = (uint8_t)~__mdpad_read_stable(rd); // TH=0
  *pc = high; __mdpad_barrier(); out.raw[1] = (uint8_t)~__mdpad_read_stable(rd); // TH=1
  *pc = low;  __mdpad_barrier(); out.raw[2] = (uint8_t)~__mdpad_read_stable(rd); // TH=0
  *pc = high; __mdpad_barrier(); out.raw[3] = (uint8_t)~__mdpad_read_stable(rd); // TH=1
  *pc = low;  __mdpad_barrier(); out.raw[4] = (uint8_t)~__mdpad_read_stable(rd); // TH=0
  *pc = high; __mdpad_barrier(); out.raw[5] = (uint8_t)~__mdpad_read_stable(rd); // TH=1
  *pc = high; __mdpad_barrier(); (void)__mdpad_read_stable(rd);                  // dummy for stable read
  return out;
}

static inline uint8_t mdpad_th1_merge(const mdpad_raw6_t* s) {
  return (uint8_t)(s->raw[1] | s->raw[3] | s->raw[5]);
}

static inline int mdpad_is_id_nibble(uint8_t v) { return (v & 0x0F) == 0x0F; }

static inline int mdpad_guess_ext_index(const mdpad_raw6_t* s) {
  const uint8_t dir_ref = s->raw[1] & 0x0F;
  const int cand_lo[] = {4, 2};
  for (unsigned i=0;i<sizeof(cand_lo)/sizeof(cand_lo[0]);++i){
    int k = cand_lo[i];
    uint8_t n = s->raw[k] & 0x0F;
    if (n != dir_ref && !mdpad_is_id_nibble(n)) return k;
  }
#if 1
  const int cand_hi[] = {5, 3};
  for (unsigned i=0;i<sizeof(cand_hi)/sizeof(cand_hi[0]);++i){
    int k = cand_hi[i];
    uint8_t n = s->raw[k] & 0x0F;
    if (n != dir_ref && !mdpad_is_id_nibble(n)) return k;
  }
#endif
  return -1;
}

/* ---- 6-button decoded read 
 * - U/D/L/R from TH=1 sample (raw[1])
 * - B,C from TH=1 (raw[1])
 * - A,START from TH=0 (raw[0])
 * - X,Y,Z,MODE from the 3rd TH=0 (raw[4]) where D0..D3 are reused.
 */
static inline mdpad_state_t mdpad_read6(mdpad_port_t port) {
  mdpad_raw6_t s = mdpad_read6_raw(port); 
  int ext_i = mdpad_guess_ext_index(&s);                /* ext idx  */
  uint8_t th1 = 0; /* dir + BC */
  if (ext_i != 1) th1 |= s.raw[1];
  if (ext_i != 3) th1 |= s.raw[3];
  if (ext_i != 5) th1 |= s.raw[5];
  uint8_t th0 = 0; /* dir + AS */
  if (ext_i != 0) th0 |= s.raw[0];
  if (ext_i != 2) th0 |= s.raw[2];
  uint8_t ext = 0;                                      /* ext      */
  if (ext_i >= 0) {
    uint8_t v = s.raw[ext_i] & 0x0F;
    if (!mdpad_is_id_nibble(v)) ext = v;
  }
  mdpad_state_t st = (mdpad_state_t){0};

  if (th1 & (1u << 0)) st.bits |= MDPAD_UP;
  if (th1 & (1u << 1)) st.bits |= MDPAD_DOWN;
  if (th1 & (1u << 2)) st.bits |= MDPAD_LEFT;
  if (th1 & (1u << 3)) st.bits |= MDPAD_RIGHT;

  if (th1 & (1u << 5    )) st.bits |= MDPAD_B;    /* 3B */
  if (th1 & (1u << 6    )) st.bits |= MDPAD_C;
  if (th0 & (1u << 5    )) st.bits |= MDPAD_A;
  if (th0 & (1u << 6    )) st.bits |= MDPAD_START;

  if (ext & (1u << 0)) st.bits |= MDPAD_Z;        /* 6B */
  if (ext & (1u << 1)) st.bits |= MDPAD_Y;
  if (ext & (1u << 2)) st.bits |= MDPAD_X;
  if (ext & (1u << 3)) st.bits |= MDPAD_MODE;
  return st;
}

/* ---- Convenience: read-any (detect once per call) ---- */
static inline mdpad_state_t mdpad_read(mdpad_port_t port) {
  mdpad_type_t t = mdpad_detect(port);
  if (t == MDPAD_MD6) return mdpad_read6(port);
  /* treat others as 3-button/Atari superset */
  return mdpad_read3(port);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
#endif /* MDPAD_H_ */
