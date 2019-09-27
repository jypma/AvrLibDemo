#pragma once

/*
time  current  mintemp floor
10:28 0.18A
10:32 0.14A    24.1C
10:37 0.10A    23.5C
10:44 0.09A    23.5C
11:19 0.08A    24.0C
12:01 0.8A     24.5C
12:30 0.8A     25.0C
13:30 0.8A     26.3C (lowered pump speed)
14:00 0.08A    26.7C (turned off)
15:00 0A       27.1C
17:45 0A       26.9C
09:07 (next)   26.0C
 */

#include "eeprom.hpp"
#include "Mechanic/Button.hpp"
#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "HopeRF/RFM12.hpp"
#include "HAL/Atmel/Power.hpp"
#include "auto_field.hpp"
#include "HAL/Atmel/InterruptHandlers.hpp"
#include "Passive/SupplyVoltage.hpp"
#include "Dallas/DS18x20.hpp"
#include "Tasks/TaskState.hpp"
#include "Tasks/loop.hpp"
#include "Tasks/Task.hpp"
#include "HopeRF/RxTxState.hpp"
#include "HopeRF/TxState.hpp"
#include "Serial/FrequencyCounter.hpp"
#include "Mechanic/Button.hpp"
#include <avr/sfr_defs.h>

using namespace HAL::Atmel;
using namespace Time;
using namespace HopeRF;
using namespace Mechanic;
using namespace Passive;
using namespace Streams;
using namespace Dallas;
using namespace Serial;

struct State {
  uint16_t values = 0;

  typedef Protobuf::Protocol<State> P;

  typedef P::Message<
    P::Varint<1, uint16_t, &State::values>
    > DefaultProtocol;

  constexpr bool operator != (const State &b) const {
    return b.values != values;
  }
};

struct Command {
  Option<uint16_t> turnOn;
  Option<uint16_t> turnOff;

  typedef Protobuf::Protocol<Command> P;

  typedef P::Message<
    P::Varint<1, Option<uint16_t>, &Command::turnOn>,
    P::Varint<2, Option<uint16_t>, &Command::turnOff>
    > DefaultProtocol;

  constexpr bool operator != (const Command &b) const {
    return (b.turnOn != turnOn) && (b.turnOff != turnOff);
  }
};

struct Heater: public Task {
  typedef Heater This;
  typedef Logging::Log<Loggers::Main> log;

  const uint16_t invertMask = read(&EEPROM::inverted);

  SPIMaster spi;
  Usart0 usart0 = { 57600 };
  PinPD1_t<decltype(usart0), 240> pinTX;

  Timer0::withPrescaler<1024>::Normal timer0;
  RealTimer<decltype(timer0)> rt = { timer0 };
  auto_var(pinRFM12_SS, PinPB2());
  auto_var(pinRFM12_INT, PinPD2());
  RFM12<decltype(spi), decltype(pinRFM12_SS), decltype(pinRFM12_INT), decltype(timer0)::comparatorA_t, true, 100, 100> rfm = {
    spi, pinRFM12_SS, pinRFM12_INT, &timer0.comparatorA(), RFM12Band::_868Mhz };
  HAL::Atmel::Impl::Power<decltype(rt)> power = { rt };

  auto_var(pinOut0, ArduinoPinD3());
  auto_var(pinOut1, ArduinoPinD4());
  auto_var(pinOut2, ArduinoPinD5());
  auto_var(pinOut3, ArduinoPinD6());
  auto_var(pinOut4, ArduinoPinD7());
  auto_var(pinOut5, ArduinoPinD8());
  auto_var(pinOut6, ArduinoPinD9());
  auto_var(pinOut7, ArduinoPinA0());
  auto_var(pinOut8, ArduinoPinA1());
  auto_var(pinOut9, ArduinoPinA2());
  auto_var(pinOut10, ArduinoPinA3());
  auto_var(pinOut11, ArduinoPinA4());

  static constexpr uint8_t N = 12;

  Periodic<decltype(rt), decltype(10_sec)> status = { rt };

  Deadline<decltype(rt), decltype(360_min)> timeouts[N] =
    { {rt}, {rt}, {rt}, {rt}, {rt}, {rt}, {rt}, {rt}, {rt}, {rt}, {rt}, {rt} };

  TxState<decltype(rfm), decltype(rt), State> state = { rfm, rt, { 0 }, uint16_t('e' << 8) | read(&EEPROM::id) };
  RxState<decltype(rfm), Command> command = { rfm, {}, uint16_t('e' << 8) | read(&EEPROM::id) };

  Periodic<decltype(rt), decltype(100_ms)> pollInputs = { rt };

  typedef Delegate<
    This, decltype(rt), &This::rt,
    Delegate<
      This, decltype(rfm), &This::rfm,
      Delegate<
        This, decltype(power), &This::power,
        Delegate<
          This, decltype(pinTX), &This::pinTX>>>> Handlers;

  bool shouldBeHigh(State outputs, uint16_t bitMask) {
    return ((outputs.values ^ invertMask) & bitMask) != 0;
  }

  void printStatus() {
    State outputs = state.get();
    bool high0 = shouldBeHigh(outputs, 1);
    bool high1 = shouldBeHigh(outputs, 2);
    bool high2 = shouldBeHigh(outputs, 4);
    bool high3 = shouldBeHigh(outputs, 8);
    bool high4 = shouldBeHigh(outputs, 16);
    bool high5 = shouldBeHigh(outputs, 32);
    bool high6 = shouldBeHigh(outputs, 64);
    bool high7 = shouldBeHigh(outputs, 128);
    bool high8 = shouldBeHigh(outputs, 256);
    bool high9 = shouldBeHigh(outputs, 512);
    bool high10 = shouldBeHigh(outputs, 1024);
    bool high11 = shouldBeHigh(outputs, 2048);
    log::debug(F("out: "), '0' + high0, '0' + high1, '0' + high2, '0' + high3, '0' + high4, '0' + high5,
                           '0' + high6, '0' + high7, '0' + high8, '0' + high9, '0' + high10, '0' + high11);
  }

  void applyOutput() {
    printStatus();
    State outputs = state.get();
    bool high0 = shouldBeHigh(outputs, 1);
    bool high1 = shouldBeHigh(outputs, 2);
    bool high2 = shouldBeHigh(outputs, 4);
    bool high3 = shouldBeHigh(outputs, 8);
    bool high4 = shouldBeHigh(outputs, 16);
    bool high5 = shouldBeHigh(outputs, 32);
    bool high6 = shouldBeHigh(outputs, 64);
    bool high7 = shouldBeHigh(outputs, 128);
    bool high8 = shouldBeHigh(outputs, 256);
    bool high9 = shouldBeHigh(outputs, 512);
    bool high10 = shouldBeHigh(outputs, 1024);
    bool high11 = shouldBeHigh(outputs, 2048);
    pinOut0.setHigh(high0);
    pinOut1.setHigh(high1);
    pinOut2.setHigh(high2);
    pinOut3.setHigh(high3);
    pinOut4.setHigh(high4);
    pinOut5.setHigh(high5);
    pinOut6.setHigh(high6);
    pinOut7.setHigh(high7);
    pinOut8.setHigh(high8);
    pinOut9.setHigh(high9);
    pinOut10.setHigh(high10);
    pinOut11.setHigh(high11);
  }

  void turnOff(uint8_t n) {
    uint16_t s = state.get().values;
    s &= ~(1 << n);
    state.set( { s } );
    applyOutput();
  }

  uint16_t lastRecvState = 0;
  void scheduleTimeouts() {
    uint16_t s = state.get().values;
    for (uint8_t i = 0; i < N; i++) {
      uint16_t mask = (1 << i);
      if (((s & mask) != 0) && ((lastRecvState & mask) == 0)) {
        timeouts[i].schedule();
      }
    }
  }

  void applyCommand() {
    auto v = state.get().values;
    for (auto on: command.get().turnOn) {
      v |= on;
    }
    for (auto off: command.get().turnOff) {
      v &= ~off;
    }
    if (v != state.get().values) {
      state.set({ v });
    }
  }

  void loop() {
    while (rfm.in().hasContent()) {
      if (command.isStateChanged()) {
        applyCommand();
        command.reset();
        scheduleTimeouts();
        applyOutput();
      } else {
        log::debug(F("Ign"));
        rfm.in().readStart();
        rfm.in().readEnd();
      }
    }

    for (uint8_t i = 0; i < N; i++) {
      if (timeouts[i].isNow()) {
        log::debug(F("Timeout on channel "), dec(i));
        turnOff(i);
      }
    }
  }

public:
  void main() {
    log::debug(F("Heater "), dec(&EEPROM::id), ' ', dec(&EEPROM::inverted));
    log::flush();
    for (uint8_t i = 0; i < N; i++) {
      timeouts[i].cancel();
    }
    pinOut0.configureAsOutputLow();
    pinOut1.configureAsOutputLow();
    pinOut2.configureAsOutputLow();
    pinOut3.configureAsOutputLow();
    pinOut4.configureAsOutputLow();
    pinOut5.configureAsOutputLow();
    pinOut6.configureAsOutputLow();
    pinOut7.configureAsOutputLow();
    pinOut8.configureAsOutputLow();
    pinOut9.configureAsOutputLow();
    pinOut10.configureAsOutputLow();
    pinOut11.configureAsOutputLow();
    applyOutput();
    auto statusTask = status.invoking<This, &This::printStatus>(*this);
    while(true) {
      loopTasks(power, rfm, state, statusTask, *this);
    }
  }
};
