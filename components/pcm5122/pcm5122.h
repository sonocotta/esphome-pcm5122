#pragma once

#include "esphome/components/audio_dac/audio_dac.h"
#include "esphome/core/component.h"
#include "esphome/components/i2c/i2c.h"
#include "esphome/core/hal.h"
#include "pcm5122_cfg.h"

namespace esphome
{
  namespace pcm5122
  {

    class Pcm5122Component : public audio_dac::AudioDac, public PollingComponent, public i2c::I2CDevice
    {
    public:
      void setup() override;

      void loop() override;
      void update() override;

      void dump_config() override;

      float get_setup_priority() const override { return setup_priority::IO; }

      void set_enable_pin(GPIOPin *enable) { this->enable_pin_ = enable; }

      void enable_dac(bool enable);

      bool is_muted() override { return this->is_muted_; }
      bool set_mute_off() override;
      bool set_mute_on() override;

      float volume() override;
      bool set_volume(float value) override;

      void config_mixer_mode(MixerMode mixer_mode) {this->pcm5122_state_.mixer_mode = mixer_mode; }
      void config_analog_gain(float analog_gain) { this->pcm5122_state_.analog_gain = (int8_t)analog_gain; }
      void config_volume_max(float volume_max) {this->pcm5122_state_.volume_max = (int8_t)(volume_max); }
      void config_volume_min(float volume_min) {this->pcm5122_state_.volume_min = (int8_t)(volume_min); }

    protected:
      GPIOPin *enable_pin_{nullptr};

      bool configure_registers_();

      bool get_analog_gain_(int8_t *gain_db);
      bool set_analog_gain_(int8_t gain_db);

      bool set_deep_sleep_off_();
      bool set_deep_sleep_on_();

      bool get_digital_volume_(uint8_t *raw_volume);
      bool set_digital_volume_(uint8_t new_volume);

      bool get_state_(ControlState *state);
      bool set_state_(ControlState state);

      bool get_mixer_mode_(MixerMode *mode);
      bool set_mixer_mode_(MixerMode mode);

      // low level functions
      bool set_page_(uint8_t page);

      bool pcm5122_read_byte_(uint8_t a_register, uint8_t *data);
      bool pcm5122_read_bytes_(uint8_t a_register, uint8_t *data, uint8_t number_bytes);
      bool pcm5122_write_byte_(uint8_t a_register, uint8_t data);
      bool pcm5122_write_bytes_(uint8_t a_register, uint8_t *data, uint8_t len);

      enum ErrorCode
      {
        NONE = 0,
        CONFIGURATION_FAILED,
      } error_code_{NONE};

      struct Pcm5122State
      {
        int8_t analog_gain = 0;     // configured by YAML, default 0dB
        int8_t volume_max = 24;     // configured by YAML, default 24dB
        int8_t volume_min = -103;   // configured by YAML, default -103dB
        MixerMode mixer_mode;     // configured by YAML
        uint8_t raw_volume_max;     // initialised in setup
        uint8_t raw_volume_min;     // initialised in setup

        ControlState control_state; // initialised in setup
      } pcm5122_state_;

      uint8_t i2c_error_{0};
      uint8_t loop_counter_{0};

      uint16_t count_fast_updates_{0};

      uint16_t number_registers_configured_{0};

      // initialised in loop
      uint32_t start_time_;
    };

  } // namespace pcm5122
} // namespace esphome
