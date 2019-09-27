#pragma once

#include "auto_field.hpp"

#include "HAL/Atmel/Device.hpp"
#include "HAL/Atmel/Power.hpp"
#include "Tasks/loop.hpp"
#include "Time/RealTimer.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Streams;

enum WaterState {
  PAUSE = 0,
  TURNING_ON = 1,
  WATERING = 2,
  TURNING_OFF = 3,
  SLEEPING = 4
};

struct GardenWater {
  typedef GardenWater This;
  typedef Logging::Log<Loggers::Main> log;

  Usart0 usart0 = { 57600 };
  PinPD1_t<decltype(usart0), 250> pinTX;
  Timer0::withPrescaler<1024>::Normal timer0;
  RealTimer<decltype(timer0)> rt = { timer0 };
  HAL::Atmel::Impl::Power<decltype(rt)> power = { rt };
  VariableDeadline<decltype(rt)> timeout = { rt };

  auto_var(pinValvePower, JeeNodePort1D());
  auto_var(pinValveOn, JeeNodePort2D());
  auto_var(pinValveOff, JeeNodePort3D());

  Periodic<decltype(rt), decltype(1_min)> print = { rt };

  WaterState state = PAUSE;
  const uint8_t waterPerDay = 6;
  uint8_t waterCount = 0;

  typedef Delegate<This, decltype(rt), &This::rt,
          Delegate<This, decltype(pinTX), &This::pinTX,
          Delegate<This, decltype(power), &This::power>>>
          Handlers;

  void setTimeout() {
    switch (state) {
      case PAUSE: timeout.schedule(1_min); break;
      case TURNING_ON: timeout.schedule(500_ms); break;
      case WATERING: timeout.schedule(5_min); break;
      case TURNING_OFF: timeout.schedule(500_ms); break;
      case SLEEPING: timeout.schedule(1200_min); break; // 20 hours
    }
  }

  void onTimeout() {
    log::debug(F("onTimeout, state"), dec(uint8_t(state)));
    pinValvePower.configureAsOutputLow();
    pinValveOn.configureAsInputWithoutPullup();
    pinValveOff.configureAsInputWithoutPullup();
    switch (state) {
      case PAUSE:
        state = TURNING_ON;
        pinValveOn.configureAsOutputHigh();
        pinValvePower.configureAsOutputHigh();
        break;
      case TURNING_ON:
        state = WATERING;
        break;
      case WATERING:
        state = TURNING_OFF;
        pinValveOff.configureAsOutputHigh();
        pinValvePower.configureAsOutputHigh();
        break;
      case TURNING_OFF:
        waterCount++;
        if (waterCount >= waterPerDay) {
          waterCount = 0;
          state = SLEEPING;
        } else {
          state = PAUSE;
        }
        break;
      case SLEEPING:
        state = PAUSE;
        break;
    }
    log::debug(F("-> "), dec(uint8_t(state)));
    setTimeout();
  }

  void onPrint() {
    uint32_t millis = uint32_t(timeout.timeLeftIfScheduled().getOrElse(Milliseconds(0)));
    log::debug(dec(uint8_t(state)), F(" for "),
               dec(millis / 60000),
               F("min"));
  }

  int main() {
    pinValvePower.configureAsOutputLow();
    pinValveOn.configureAsInputWithoutPullup();
    pinValveOff.configureAsInputWithoutPullup();

    setTimeout();
    auto timeoutTask = timeout.invoking<This, &This::onTimeout>(*this);
    auto printTask = print.invoking<This, &This::onPrint>(*this);
    log::debug(F("GardenWater"));
    while (true) {
      loopTasks(power, timeoutTask, printTask);
    }
  }
};
