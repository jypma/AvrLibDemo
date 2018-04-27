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
#include <avr/cpufunc.h>
#include "Tasks/loop.hpp"
#include "Tasks/Task.hpp"
#include "HopeRF/RxTxState.hpp"
#include "HopeRF/TxState.hpp"
#include "Serial/FrequencyCounter.hpp"
#include "Mechanic/Button.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace HopeRF;
using namespace Mechanic;
using namespace Passive;
using namespace Streams;
using namespace Dallas;
using namespace Serial;

struct State {
    uint8_t values;

    typedef Protobuf::Protocol<State> P;

    typedef P::Message<
        P::Varint<1, uint8_t, &State::values>
    > DefaultProtocol;

    constexpr bool operator != (const State &b) const {
        return b.values != values;
    }
};

enum ToggleMode: uint8_t {
    OFF, AC_TOGGLE, DC_TOGGLE, DC_TOGGLE_LOW
};

struct RFSwitch: public Task {
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

    auto_var(pinOut0, PinPD3());
    auto_var(pinOut1, PinPD4());
    auto_var(pinOut2, PinPD5());
    auto_var(pinOut3, PinPD6());

    auto_var(pinIn0, PinPC0::withInterruptOnChange());
    auto_var(pinIn1, PinPC1::withInterruptOnChange());
    auto_var(pinIn2, PinPC2::withInterruptOnChange());
    auto_var(pinIn3, PinPC3::withInterruptOnChange());
/*
    FrequencyCounter<decltype(pinIn0), decltype(rt)> freqCnt0 = { pinIn0, rt };
    FrequencyCounter<decltype(pinIn1), decltype(rt)> freqCnt1 = { pinIn1, rt };
    FrequencyCounter<decltype(pinIn2), decltype(rt)> freqCnt2 = { pinIn2, rt };
    FrequencyCounter<decltype(pinIn3), decltype(rt)> freqCnt3 = { pinIn3, rt };
*/
    auto_var(btn0, Button(rt, pinIn0));
    auto_var(btn1, Button(rt, pinIn1));
    auto_var(btn2, Button(rt, pinIn2));
    auto_var(btn3, Button(rt, pinIn3));

    const uint8_t invertMask = read(&EEPROM::inverted);
    const ToggleMode toggleMode[4] = {
        static_cast<ToggleMode>(read(&EEPROM::auto_toggle) & 3),
        static_cast<ToggleMode>((read(&EEPROM::auto_toggle) >> 2) & 3),
        static_cast<ToggleMode>((read(&EEPROM::auto_toggle) >> 4) & 3),
        static_cast<ToggleMode>((read(&EEPROM::auto_toggle) >> 6) & 3),
    };
    RxTxState<decltype(rfm), decltype(rt), State> outputState = { rfm, rt, { 0 }, uint16_t('r' << 8) | read(&EEPROM::id) };
    TxState<decltype(rfm), decltype(rt), State> inputState = { rfm, rt, { 0 }, uint16_t('f' << 8) | read(&EEPROM::id) };

    auto_var(pollInputs, periodic(rt, 100_ms));

    typedef Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(rfm), &This::rfm,
            Delegate<This, decltype(power), &This::power,
            Delegate<This, decltype(pinTX), &This::pinTX,
            //Delegate<This, decltype(freqCnt0), &This::freqCnt0,
            //Delegate<This, decltype(freqCnt1), &This::freqCnt1,
            //Delegate<This, decltype(freqCnt2), &This::freqCnt2,
            //Delegate<This, decltype(freqCnt3), &This::freqCnt3,
            Delegate<This, decltype(btn0), &This::btn0,
            Delegate<This, decltype(btn1), &This::btn1,
            Delegate<This, decltype(btn2), &This::btn2,
            Delegate<This, decltype(btn3), &This::btn3
    >>>>>>>> Handlers;

    bool shouldBeHigh(State outputs, uint8_t bitMask) {
        return ((outputs.values ^ invertMask) & bitMask) != 0;
    }

    void applyOutput(State outputs) {
        bool high0 = shouldBeHigh(outputs, 1);
        bool high1 = shouldBeHigh(outputs, 2);
        bool high2 = shouldBeHigh(outputs, 4);
        bool high3 = shouldBeHigh(outputs, 8);
        log::debug(F("out: "), '0' + high0, '0' + high1, '0' + high2, '0' + high3);
        pinOut0.setHigh(high0);
        pinOut1.setHigh(high1);
        pinOut2.setHigh(high2);
        pinOut3.setHigh(high3);
    }
/*
    void readACInputs() {
        auto oldIn = inputState.get();
        uint8_t inputs = (freqCnt0.getFrequency().isDefined() ? 1 : 0) |
                         (freqCnt1.getFrequency().isDefined() ? 2 : 0) |
                         (freqCnt2.getFrequency().isDefined() ? 4 : 0) |
                         (freqCnt3.getFrequency().isDefined() ? 8 : 0);
        auto out = outputState.get();
        for (uint8_t bit = 0; bit <= 3; bit++) {
            uint8_t mask = (1 << bit);
            if ((toggleMode[bit] == AC_TOGGLE) && ((inputs & mask) != (oldIn.values & mask))) {
                out.values ^= bit;
            }
        }
        inputState.set({ inputs });
        outputState.set(out);
        applyOutput(out);
    }
*/
    template <uint8_t idx, typename btn_t>
    void readButton(btn_t &btn) {
        if (toggleMode[idx] == DC_TOGGLE || toggleMode[idx] == DC_TOGGLE_LOW) {
            auto evt = btn.nextEvent();
            if (evt == ButtonEvent::PRESSED || evt == ButtonEvent::RELEASED) {
                auto oldIn = inputState.get();
                if (evt == ButtonEvent::PRESSED) {
                    inputState.set({ uint8_t( oldIn.values | (1 << idx) ) });
                } else {
                    inputState.set({ uint8_t( oldIn.values & ~(1 << idx) ) });
                }
                if ((toggleMode[idx] == DC_TOGGLE_LOW && evt == ButtonEvent::PRESSED) || (toggleMode[idx] == DC_TOGGLE)) {
                    auto out = outputState.get();
                    out.values ^= (1 << idx);
                    outputState.set(out);
                    applyOutput(out);
                }
            }
        }
    }

    void loop() {
        readButton<0>(btn0);
        readButton<1>(btn1);
        readButton<2>(btn2);
        readButton<3>(btn3);

        while (rfm.in().hasContent()) {
            if (outputState.isStateChanged()) {
                applyOutput(outputState.get());
            } else {
                log::debug(F("Ign"));
                rfm.in().readStart();
                rfm.in().readEnd();
            }
        }
    }

public:
    void main() {
        log::debug(F("RFSwitch "), &EEPROM::id, ' ', dec(&EEPROM::inverted));
        log::flush();
        pinOut0.configureAsOutputLow();
        pinOut1.configureAsOutputLow();
        pinOut2.configureAsOutputLow();
        pinOut3.configureAsOutputLow();
        pinIn0.configureAsInputWithPullup();
        pinIn1.configureAsInputWithPullup();
        pinIn2.configureAsInputWithPullup();
        pinIn3.configureAsInputWithPullup();
        applyOutput(outputState.get());
        const bool needACInputs =
            (toggleMode[0] == AC_TOGGLE) ||
            (toggleMode[1] == AC_TOGGLE) ||
            (toggleMode[2] == AC_TOGGLE) ||
            (toggleMode[3] == AC_TOGGLE);
        while(true) {
            //if (needACInputs) {
            //    loopTasks(power, rfm, pollInputs.invoking<This, &This::readACInputs>(*this), *this);
            //} else {
                loopTasks(power, rfm, *this);
            //}
        }
    }
};
