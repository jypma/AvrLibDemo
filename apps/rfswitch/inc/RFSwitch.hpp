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
#include "HopeRF/RxTxState.hpp"
#include "Serial/FrequencyCounter.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace HopeRF;
using namespace Mechanic;
using namespace Passive;
using namespace Streams;
using namespace Dallas;
using namespace Serial;

struct State {
    uint8_t outputs;
    uint8_t inputs;

    typedef Protobuf::Protocol<State> P;

    typedef P::Message<
        P::Varint<1, uint8_t, &State::outputs>,
        P::Varint<2, uint8_t, &State::inputs>
    > DefaultProtocol;

    constexpr bool operator != (const State &b) const {
        return b.outputs != outputs || b.inputs != b.outputs;
    }

    State setInputs(uint8_t i) {
        return { outputs, i };
    }
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
    auto_var(power, Power(rt));

    auto_var(pinOut0, PinPD4());
    auto_var(pinOut1, PinPD5());
    auto_var(pinOut2, PinPD6());
    auto_var(pinOut3, PinPD7());

    auto_var(pinIn0, PinPC0::withInterruptOnChange());
    auto_var(pinIn1, PinPC1::withInterruptOnChange());
    auto_var(pinIn2, PinPC2::withInterruptOnChange());
    auto_var(pinIn3, PinPC3::withInterruptOnChange());

    FrequencyCounter<decltype(pinIn0), decltype(rt)> freqCnt0 = { pinIn0, rt };
    FrequencyCounter<decltype(pinIn1), decltype(rt)> freqCnt1 = { pinIn1, rt };
    FrequencyCounter<decltype(pinIn2), decltype(rt)> freqCnt2 = { pinIn2, rt };
    FrequencyCounter<decltype(pinIn3), decltype(rt)> freqCnt3 = { pinIn3, rt };

    const uint8_t invertMask = read(&EEPROM::inverted);
    const uint8_t toggleMask = read(&EEPROM::auto_toggle);
    const uint16_t nodeId = 'r' << 8 | read(&EEPROM::id);
    RxTxState<decltype(rfm), decltype(rt), State> rxState = { rfm, rt, { 0, 0 }, nodeId };

    auto_var(pollInputs, periodic(rt, 100_ms));

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

        if (pollInputs.isNow()) {
            uint8_t inputs = (freqCnt0.getFrequency().isDefined() ? 1 : 0) |
                             (freqCnt1.getFrequency().isDefined() ? 2 : 0) |
                             (freqCnt2.getFrequency().isDefined() ? 4 : 0) |
                             (freqCnt3.getFrequency().isDefined() ? 8 : 0);
            auto old = rxState.get();
            for (int bit = 1; bit <= 8; bit <<= 1) {
                if ((toggleMask & bit) && ((inputs & bit) != (old.inputs & bit))) old.outputs ^= bit;
            }
            rxState.set(old.setInputs(inputs));
        }

        while (rfm.in().hasContent()) {
            if (rxState.isStateChanged()) {
                State state = rxState.get();
                outputState(state);
            } else {
                log::debug(F("Ign"));
                rfm.in().readStart();
                rfm.in().readEnd();
            }
        }

        power.sleepUntilTasks(rfmState, pollInputs);
    }

    int main() {
        log::debug(F("RFSwitch "), &EEPROM::id, ' ', dec(&EEPROM::inverted));
        pinOut0.configureAsOutputLow();
        pinOut1.configureAsOutputLow();
        pinOut2.configureAsOutputLow();
        pinOut3.configureAsOutputLow();
        pinIn0.configureAsInputWithPullup();
        pinIn1.configureAsInputWithPullup();
        pinIn2.configureAsInputWithPullup();
        pinIn3.configureAsInputWithPullup();
        outputState(rxState.get());
        while(true) loop();
    }
};
