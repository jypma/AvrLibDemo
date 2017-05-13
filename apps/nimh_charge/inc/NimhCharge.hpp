#pragma once

#include "eeprom.hpp"
#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "HAL/Atmel/Power.hpp"
#include "auto_field.hpp"
#include "HAL/Atmel/InterruptHandlers.hpp"
#include "Passive/SupplyVoltage.hpp"
#include "Tasks/TaskState.hpp"
#include "Dallas/DS18x20.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Passive;
using namespace Streams;
using namespace Dallas;

enum State {
    CHARGE, COOLDOWN, STOP
};

struct NimhCharge {
    typedef NimhCharge This;
    typedef Logging::Log<Loggers::Main> log;

    ADConverter<uint16_t> adc;

    Usart0 usart0 = { 57600 };
    auto_var(pinTX, PinPD1<250>(usart0));
    auto_var(timer0, Timer0::withPrescaler<1024>::inNormalMode());

    auto_var(rt, realTimer(timer0));

    auto_var(pinDisable, JeeNodePort3D());
    auto_var(pinSupply, JeeNodePort3A());
    auto_var(supplyVoltage, (SupplyVoltage<10000, 1000, &EEPROM::bandgapVoltage>(adc, pinSupply)));

    auto_var(tempPin, JeeNodePort4D());
    auto_var(tempWire, OneWireParasitePower(tempPin, rt));
    auto_var(temp, SingleDS18x20(tempWire));

    auto_var(stateTick, deadline(rt));
    auto_var(tempTick, periodic(rt, 1_min));

    State state = CHARGE;
    uint16_t lastSupply = 0;
    int16_t lastTemp = 1000;
    uint16_t charges = 0;

    static constexpr uint16_t current = 1000;  // mA
    static constexpr uint16_t capacity = 2000; // mAh
    static constexpr auto chargeTime = 10_sec;

    typedef Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(pinTX), &This::pinTX>>
            Handlers;

    void done() {
        pinDisable.setHigh();
        state = STOP;
        stateTick.cancel();
    }

    void charge() {
        log::debug(F("ON!"));
        charges++;
        if (charges > ((capacity / current) * 3600 / chargeTime.getAmount()))  {
            log::debug(F("  timeout!"));
            done();
        }
        state = CHARGE;
        pinDisable.setLow();
        stateTick.schedule(chargeTime);
    }

    void cooldown() {
        log::debug(F("off"));
        state = COOLDOWN;
        pinDisable.setHigh();
        stateTick.schedule(2_sec);

        temp.measure();
        while (temp.isMeasuring()) ;
        auto t = temp.getTemperature();
        log::debug(F("Temp: "), dec(t));
        if (tempTick.isNow()) {
            if (t > int16_t(lastTemp + 10)) {
                log::debug(F("  dT!"));
                done();
            }
        }

        if (t > 400) {
            log::debug(F("  Tmax!"));
            done();
        }
    }

    void measure() {
        for (int i = 0; i < 100; i++) supplyVoltage.get();
        uint32_t supply = 0;
        for (int i = 0; i < 128; i++) supply += supplyVoltage.get();
        supply >>= 7;
        log::debug(dec(supply), F("mV"));
        if (supply < lastSupply) {
            log::debug(F("  -dV!"));
            done();
        } else {
            lastSupply = supply;
            charge();
        }
    }

    void loop() {
        if (stateTick.isNow()) {
            switch(state) {
            case CHARGE: cooldown(); break;
            case COOLDOWN: measure(); break;
            case STOP: break;
            }
        }
    }

    int main() {
        log::debug(F("Charger "));
        temp.measure();
        pinDisable.configureAsOutputHigh();
        measure();
        while (temp.isMeasuring()) ;
        auto t = temp.getTemperature();
        log::debug(F("Temp: "), dec(t));
        log::debug(F("Done"));
        while(true) loop();
    }
};
