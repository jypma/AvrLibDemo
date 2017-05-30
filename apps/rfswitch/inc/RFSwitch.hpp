#pragma once

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
#include "HopeRF/RxState.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace HopeRF;
using namespace Mechanic;
using namespace Passive;
using namespace Streams;
using namespace Dallas;

struct State {
    uint8_t outputs;

    typedef Protobuf::Protocol<State> P;

    typedef P::Message<
        P::Varint<1, uint8_t, &State::outputs>
    > DefaultProtocol;

    constexpr bool operator != (const State &b) { return b.outputs != outputs; }
};

struct RFSwitch {
    typedef RFSwitch This;
    typedef Logging::Log<Loggers::Main> log;

    SPIMaster spi;
    Usart0 usart0 = { 57600 };
    auto_var(pinTX, PinPD1(usart0));
    auto_var(timer0, Timer0::withPrescaler<1024>::inNormalMode());

    auto_var(rt, realTimer(timer0));
    auto_var(pinRFM12_SS, PinPB2());
    auto_var(pinRFM12_INT, PinPD2());

    auto_var(rfm, rfm12(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), RFM12Band::_868Mhz));
    HAL::Atmel::Impl::Power<decltype(rt)> power = { rt };

    auto_var(pinOut0, PinPD4());
    auto_var(pinOut1, PinPD5());
    auto_var(pinOut2, PinPD6());
    auto_var(pinOut3, PinPD7());

    const uint8_t invertMask = read(&EEPROM::inverted);
    const uint16_t nodeId = 'r' << 8 | read(&EEPROM::id);
    RxState<decltype(rfm), State> rxState = { rfm, nodeId, { 0 } };

    typedef Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(rfm), &This::rfm,
            Delegate<This, decltype(pinTX), &This::pinTX>>>
            Handlers;
            
    bool shouldBeHigh(State state, uint8_t bitMask) {
        return ((state.outputs ^ invertMask) & bitMask) != 0;
    }

    void outputState(State state) {
        bool high0 = shouldBeHigh(state, 1);
        bool high1 = shouldBeHigh(state, 2);
        bool high2 = shouldBeHigh(state, 4);
        bool high3 = shouldBeHigh(state, 8);
        log::debug(F("out: "), '0' + high0, '0' + high1, '0' + high2, '0' + high3);
        pinOut0.setHigh(high0);
        pinOut1.setHigh(high1);
        pinOut2.setHigh(high2);
        pinOut3.setHigh(high3);
    }

    void loop() {
        TaskState rfmState = rfm.getTaskState();

        while (rfm.in().hasContent()) {
            if (rxState.isStateChanged()) {
                State state = rxState.getState();
                outputState(state);
            } else {
                log::debug(F("Ign"));
                rfm.in().readStart();
                rfm.in().readEnd();
            }
        }

        log::flush();
        power.sleepUntilTasks(rfmState);
    }

    int main() {
        log::debug(F("RFSwitch "), &EEPROM::id, ' ', dec(&EEPROM::inverted));
        pinOut0.configureAsOutputLow();
        pinOut1.configureAsOutputLow();
        pinOut2.configureAsOutputLow();
        pinOut3.configureAsOutputLow();
        outputState({ 0 });
        while(true) loop();
    }
};
