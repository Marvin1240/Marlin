#ifdef __PLAT_NATIVE_SIM__

#include <mutex>
#include <fstream>
#include <cmath>
#include <random>
#include "Gpio.h"

#include <GL/glew.h>
#include <GL/gl.h>

#include "ST7796Device.h"

#include "../../tft/xpt2046.h"

#define ST7796S_CASET      0x2A // Column Address Set
#define ST7796S_RASET      0x2B // Row Address Set
#define ST7796S_RAMWR      0x2C // Memory Write

ST7796Device::ST7796Device(pin_type clk, pin_type mosi, pin_type cs, pin_type dc, pin_type beeper, pin_type enc1, pin_type enc2, pin_type enc_but, pin_type kill)
  : clk_pin(clk), mosi_pin(mosi), cs_pin(cs), dc_pin(dc), beeper_pin(beeper), enc1_pin(enc1), enc2_pin(enc2), enc_but_pin(enc_but), kill_pin(kill), touch(TOUCH_SCK_PIN, TOUCH_MOSI_PIN, TOUCH_CS_PIN, TOUCH_MISO_PIN)
  {

  Gpio::attach(clk_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(cs_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(dc_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(beeper_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(kill_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc_but_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc1_pin, [this](GpioEvent& event){ this->interrupt(event); });
  Gpio::attach(enc2_pin, [this](GpioEvent& event){ this->interrupt(event); });

  glGenTextures(1, &texture_id);
  glBindTexture(GL_TEXTURE_2D, texture_id);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
  glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
  glBindTexture(GL_TEXTURE_2D, 0);
}

ST7796Device::~ST7796Device() {}

void ST7796Device::process_command(Command cmd) {
  if (cmd.cmd == ST7796S_CASET) {
    xMin = (cmd.data[0] << 8) + cmd.data[1];
    xMax = (cmd.data[2] << 8) + cmd.data[3];
    graphic_ram_index_x = xMin;
  }
  else if (cmd.cmd == ST7796S_RASET) {
    yMin = (cmd.data[0] << 8) + cmd.data[1];
    yMax = (cmd.data[2] << 8) + cmd.data[3];
    graphic_ram_index_y = yMin;
  }
  else if (cmd.cmd == ST7796S_RAMWR) {
    for(int i = 0; i < cmd.data.size(); i += 2) {
      auto pixel = (cmd.data[i] << 8) + cmd.data[i+1];
      graphic_ram[graphic_ram_index_x + (graphic_ram_index_y * 480)] = pixel;
      if (graphic_ram_index_x >= xMax) {
        graphic_ram_index_x = xMin;
        graphic_ram_index_y++;
      }
      else {
        graphic_ram_index_x++;
      }
    }
    if (graphic_ram_index_y >= yMax && graphic_ram_index_x >= xMax) {
      dirty = true;
    }
  }
}

void ST7796Device::update() {
  auto now = clock.now();
  float delta = std::chrono::duration_cast<std::chrono::duration<float>>(now - last_update).count();

  if (dirty && delta > 1.0 / 30.0) {
    last_update = now;
    glBindTexture(GL_TEXTURE_2D, texture_id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 480, 320, 0, GL_RGB, GL_UNSIGNED_SHORT_5_6_5, graphic_ram);
    glBindTexture(GL_TEXTURE_2D, 0);
  }
}

static uint8_t command = 0;
static std::vector<uint8_t> data;

void ST7796Device::interrupt(GpioEvent& ev) {
  if (ev.pin_id == clk_pin && ev.event == GpioEvent::FALL && Gpio::pin_map[cs_pin].value == 0) {
    incomming_byte = (incomming_byte << 1) | Gpio::pin_map[mosi_pin].value;
    if (++incomming_bit_count == 8) {
      if (Gpio::pin_map[dc_pin].value) {
        //data
        data.push_back(incomming_byte);
      }
      else {
        //command
        command = incomming_byte;
      }
      incomming_bit_count = 0;
    }
  } else if (ev.pin_id == cs_pin && ev.event == GpioEvent::RISE) {
    //end of transaction, execute pending command
    incomming_bit_count = incomming_byte_count = incomming_byte = 0;
    process_command({command, data});
    data.clear();
  } else if (ev.pin_id == beeper_pin) {
    if (ev.event == GpioEvent::RISE) {
      // play sound
    } else if (ev.event == GpioEvent::FALL) {
      // stop sound
    }
  } else if (ev.pin_id == dc_pin && ev.event == GpioEvent::FALL) {
    //start new command, execute last one
    process_command({command, data});
    data.clear();
  }
}

void ST7796Device::ui_callback(UiWindow* window) {
  if (ImGui::IsWindowFocused()) {
    touch.ui_callback(window);
  }
}

#endif
