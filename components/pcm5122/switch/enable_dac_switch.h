#pragma once

#include "../pcm5122.h"
#include "esphome/components/switch/switch.h"
#include "esphome/core/component.h"

namespace esphome {
namespace pcm5122 {

class EnableDacSwitch : public switch_::Switch, public Component, public Parented<Pcm5122Component> {
public:
public:
  void setup() override;
  void dump_config() override;
  float get_setup_priority() const override { return setup_priority::AFTER_CONNECTION; }

protected:
  void write_state(bool state) override;
};

}  // namespace pcm5122
}  // namespace esphome