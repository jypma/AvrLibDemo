#pragma once

#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "HAL/Atmel/Power.hpp"
#include "auto_field.hpp"
#include "HAL/Atmel/InterruptHandlers.hpp"
#include "Passive/SupplyVoltage.hpp"
#include "Tasks/TaskState.hpp"
#include "HopeRF/RFM12.hpp"
#include "HopeRF/RxState.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Passive;
using namespace Streams;
using namespace HopeRF;

struct RF12Mon {
    typedef RF12Mon This;
    typedef Logging::Log<Loggers::Main> log;

    SPIMaster spi;
    ADConverter<uint16_t> adc;

    Usart0 usart0 = { 57600 };
    auto_var(pinTX, PinPD1<250>(usart0));
    auto_var(timer0, Timer0::withPrescaler<1024>::inNormalMode());
    auto_var(timer1, Timer1::withPrescaler<1>::inFastPWMMode());

    auto_var(rt, realTimer(timer0));
    auto_var(power, Power(rt));

    auto_var(pinRFM12_INT, PinPD2());
    auto_var(pinRFM12_SS, PinPB2());
    auto_var(pinLED, PinPB1(timer1)); // arduino pin D9

    auto_var(rfm, (rfm12<4,100>(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), RFM12Band::_868Mhz)));

    auto_var(anim, animator(rt));

    uint8_t oldInts = 0;

    typedef Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(pinTX), &This::pinTX,
            Delegate<This, decltype(rfm), &This::rfm,
            Delegate<This, decltype(power), &This::power>>>>
            Handlers;

    void loop() {
        if (rfm.getInterrupts() != oldInts) {
            oldInts = rfm.getInterrupts();
            log::debug(dec(oldInts));
            anim.from(255).to(0, 500_ms, AnimatorInterpolation::EASE_UP);
        }

        AnimatorEvent evt = anim.nextEvent();
        if (evt.isChanged()) {
            pinLED.timerComparator().setTargetFromNextRun(evt.getValue());
        }
        //power.sleepUntilTasks(rfmState, anim);
    }

    int main() {
        pinLED.configureAsOutputLow();
        pinLED.timerComparator().setOutput(FastPWMOutputMode::connected);
        pinLED.timerComparator().setTargetFromNextRun(0);

        while(true) loop();
    }
};
