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
  PinPD1_t<decltype(usart0), 250> pinTX;
  // FIXME timers should be types to make this simpler.
  Timer0::withPrescaler<1024>::Normal timer0;
  Timer1::withPrescaler<1>::FastPWM timer1;
  RealTimer<decltype(timer0)> rt = { timer0 };
  HAL::Atmel::Impl::Power<decltype(rt)> power = { rt };


    auto_var(pinRFM12_INT, PinPD2());
    auto_var(pinRFM12_SS, PinPB2());
  PinPB1_t<decltype(timer1)> pinLED = { timer1 };
  //    auto_var(pinLED, PinPB1(timer1)); // arduino pin D9
  RFM12<decltype(spi), decltype(pinRFM12_SS), decltype(pinRFM12_INT), decltype(timer0)::comparatorA_t, true, 4, 100> rfm = {
    spi, pinRFM12_SS, pinRFM12_INT, &timer0.comparatorA(), RFM12Band::_868Mhz };

  Animator<decltype(rt)> anim = { rt };

    uint8_t oldInts = 0;

  RF12Mon() {

  }

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

    void main() {
        log::debug(F("RF12Mon"));
        log::flush();
        pinLED.configureAsOutputLow();
        pinLED.timerComparator().setOutput(FastPWMOutputMode::connected);
        pinLED.timerComparator().setTargetFromNextRun(0);

        while(true) loop();
    }
};
