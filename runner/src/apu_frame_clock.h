#pragma once
#include <stdint.h>

#define RTL_MASTER_CYCLES_PER_FRAME 357368ull
#define RTL_APU_CYCLES_PER_FRAME 17088ull

/* A host iteration can include an NMI-disabled loader spanning many hardware
 * frames. Keep its elapsed time when beginning the following iteration. */
typedef struct RtlApuFrameClock {
  uint64_t start_master, start_guest, next_guest, last_duration;
} RtlApuFrameClock;

static inline void rtl_apu_clock_begin(RtlApuFrameClock *clock, uint64_t master) {
  clock->start_master = master;
  clock->start_guest = clock->next_guest;
}

static inline uint64_t rtl_apu_clock_now(const RtlApuFrameClock *clock,
                                        uint64_t master) {
  /* RESET may initialize CPU state inside the first host iteration. */
  uint64_t within = master >= clock->start_master ? master - clock->start_master : 0;
  return clock->start_guest + within * RTL_APU_CYCLES_PER_FRAME /
                               RTL_MASTER_CYCLES_PER_FRAME;
}

static inline uint64_t rtl_apu_clock_finish_minimum(RtlApuFrameClock *clock,
                                                   uint64_t master,
                                                   uint64_t minimum_duration) {
  uint64_t end = rtl_apu_clock_now(clock, master);
  uint64_t minimum = clock->start_guest + minimum_duration;
  if (end < minimum) end = minimum;  /* WAI still advances a hardware frame. */
  clock->last_duration = end - clock->start_guest;
  clock->next_guest = end;
  /* Leave the current origin intact: raster IRQs can still touch APU ports
   * between the completed game iteration and the next clock_begin. */
  return end;
}

static inline uint64_t rtl_apu_clock_finish(RtlApuFrameClock *clock,
                                           uint64_t master) {
  return rtl_apu_clock_finish_minimum(clock, master, RTL_APU_CYCLES_PER_FRAME);
}

/* A beam scheduler already advances parked time and carries CPU/DMA overshoot
 * into the next field. Short CPU deltas repay that overshoot; clamping them
 * to a full field would produce audio time twice. */
static inline uint64_t rtl_apu_clock_finish_beam(RtlApuFrameClock *clock,
                                                uint64_t master) {
  return rtl_apu_clock_finish_minimum(clock, master, 0);
}
