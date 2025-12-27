#include "driver/pcnt.h"

#define ENCODER_A 22
#define ENCODER_B 23

const int PPR = 24;
const float DEG_PER_PULSE = 360.0 / PPR;

#define PCNT_UNIT PCNT_UNIT_0

void setup() {
  Serial.begin(115200);

  pcnt_config_t pcnt_config;
  pcnt_config.pulse_gpio_num = ENCODER_A;     // Channel A → pulse
  pcnt_config.ctrl_gpio_num  = ENCODER_B;     // Channel B → direction control
  pcnt_config.lctrl_mode     = PCNT_MODE_REVERSE;  // jika B LOW → reverse
  pcnt_config.hctrl_mode     = PCNT_MODE_KEEP;     // jika B HIGH → keep direction
  pcnt_config.pos_mode       = PCNT_COUNT_INC;     // rising A → tambah
  pcnt_config.neg_mode       = PCNT_COUNT_DEC;     // falling A → kurang
  pcnt_config.counter_h_lim  = 32767;
  pcnt_config.counter_l_lim  = -32768;
  pcnt_config.unit           = PCNT_UNIT;
  pcnt_config.channel        = PCNT_CHANNEL_0;

  pcnt_unit_config(&pcnt_config);

  // filter 1000 = 12.5 µs × 1000 ≈ 12.5 µs → buang bouncing kecil
  pcnt_set_filter_value(PCNT_UNIT, 1500);  // 18 µs filter
  pcnt_filter_enable(PCNT_UNIT);

  // Reset dan mulai PCNT
  pcnt_counter_pause(PCNT_UNIT);
  pcnt_counter_clear(PCNT_UNIT);
  pcnt_counter_resume(PCNT_UNIT);

  Serial.println("PCNT Ready.");
}

void loop() {
  int16_t count = 0;
  pcnt_get_counter_value(PCNT_UNIT, &count);

  static int16_t lastCount = 0;

  if (count != lastCount) {
    float angle = count * DEG_PER_PULSE;

    Serial.print("Count: ");
    Serial.print(count);
    Serial.print(" | Sudut: ");
    Serial.print(angle);
    Serial.println(" derajat");

    lastCount = count;
  }
}
