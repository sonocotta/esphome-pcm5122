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

      // void config_analog_gain(float analog_gain) { this->pcm5122_state_.analog_gain = analog_gain; }

      void enable_dac(bool enable);

      bool is_muted() override { return this->is_muted_; }
      bool set_mute_off() override;
      bool set_mute_on() override;

      float volume() override;
      bool set_volume(float value) override;

      void config_volume_max(float volume_max) {this->pcm5122_state_.volume_max = (int8_t)(volume_max); }
      void config_volume_min(float volume_min) {this->pcm5122_state_.volume_min = (int8_t)(volume_min); }

    protected:
      GPIOPin *enable_pin_{nullptr};

      bool configure_registers_();

      // bool get_analog_gain_(uint8_t *raw_gain);
      // bool set_analog_gain_(float gain_db);

      // bool get_dac_mode_(DacMode *mode);
      // bool set_dac_mode_(DacMode mode);

      bool set_deep_sleep_off_();
      bool set_deep_sleep_on_();

      bool get_digital_volume_(uint8_t *raw_volume);
      bool set_digital_volume_(uint8_t new_volume);

      bool get_state_(ControlState *state);
      bool set_state_(ControlState state);

      // low level functions
      bool set_book_and_page_(uint8_t book, uint8_t page);

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
        // bool               is_muted;                   // not used as esphome AudioDac component has its own is_muted variable
        // bool               is_powered;                 // currently not used
        // float analog_gain;      // configured by YAML
        int8_t volume_max;      // configured by YAML
        int8_t volume_min;      // configured by YAML
        uint8_t raw_volume_max; // initialised in setup
        uint8_t raw_volume_min; // initialised in setup

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
