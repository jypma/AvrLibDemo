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
#include "HopeRF/RFM12.hpp"
#include "HopeRF/RxState.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Passive;
using namespace Streams;
using namespace Dallas;
using namespace HopeRF;

enum State {
    CHARGE, COOLDOWN, WAIT
};

enum StopReason {
    TIMEOUT = 0,
    TEMP_DT = 1,
    TEMP_MAX = 2,
    NIGHT = 3,
    VOLTAGE_DV = 4
};

struct ChargeStatus {
    uint16_t sender;
    Option<uint16_t> supplyVoltage;
    Option<uint16_t> solarVoltage;
    Option<uint16_t> charges;
    Option<int16_t> temp;
    Option<uint8_t> stopReason;

    typedef Protobuf::Protocol<ChargeStatus> P;

    typedef P::Message<
        P::Varint<1, uint16_t, &ChargeStatus::sender>,
        P::Varint<2, Option<uint16_t>, &ChargeStatus::supplyVoltage>,
        P::Varint<3, Option<uint16_t>, &ChargeStatus::solarVoltage>,
        P::Varint<4, Option<uint16_t>, &ChargeStatus::charges>,
        P::Varint<5, Option<int16_t>, &ChargeStatus::temp>,
        P::Varint<6, Option<uint8_t>, &ChargeStatus::stopReason>
    > DefaultProtocol;
};

struct Outputs {
    uint8_t outputs = 0;

    constexpr inline bool operator!= (const Outputs &that) const {
        return outputs != that.outputs;
    }

    typedef Protobuf::Protocol<Outputs> P;
    typedef P::Message<
        P::Varint<1, uint8_t, &Outputs::outputs>
    > DefaultProtocol;
};

struct NimhCharge {
    typedef NimhCharge This;
    typedef Logging::Log<Loggers::Main> log;

    SPIMaster spi;
    ADConverter<uint16_t> adc;

    Usart0 usart0 = { 57600 };
    auto_var(pinTX, PinPD1<250>(usart0));
    auto_var(timer0, Timer0::withPrescaler<1024>::inNormalMode());

    auto_var(rt, realTimer(timer0));
    HAL::Atmel::Impl::Power<decltype(rt)> power = { rt };

    auto_var(pinDisable, JeeNodePort3D());
    auto_var(pinSupply, JeeNodePort3A());
    auto_var(pinSolar, JeeNodePort4A());
    auto_var(pinRFM12_INT, PinPD2());
    auto_var(pinRFM12_SS, PinPB2());
    auto_var(pinTemp, JeeNodePort4D());
    auto_var(pinOut1, JeeNodePort1D());
    auto_var(pinOut2, JeeNodePort2D());

    auto_var(rfm, (rfm12<4,100>(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), RFM12Band::_868Mhz)));
    auto_var(outputs, (RxState<decltype(rfm), Outputs>(rfm, 'n' << 8 | read(&EEPROM::id), {})));

    auto_var(supplyVoltage, (SupplyVoltage<10000, 1000, &EEPROM::bandgapVoltage>(adc, pinSupply)));
    auto_var(solarVoltage, (SupplyVoltage<10000, 500, &EEPROM::bandgapVoltage>(adc, pinSolar)));

    auto_var(tempWire, OneWireParasitePower(pinTemp, rt));
    auto_var(temp, SingleDS18x20(tempWire));

    auto_var(stateTick, deadline(rt));
    auto_var(tempTick, periodic(rt, 1_min));

    State state = CHARGE;
    uint16_t lastSupply = 5;
    int16_t lastTemp = 1000;
    uint16_t charges = 0;

    static constexpr uint16_t current = 700;  // mA
    static constexpr uint16_t capacity = 2000; // mAh

    static constexpr auto chargeTime = 10_sec;
    static constexpr auto waitTime = 60_min;   // wait this long before deciding to top up again

    typedef Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(pinTX), &This::pinTX,
            Delegate<This, decltype(power), &This::power>>>
            Handlers;

    void done(uint8_t reason) {
        pinDisable.setHigh();
        state = WAIT;
        stateTick.schedule(waitTime);

        ChargeStatus m;
        m.sender = 'n' << 8 | read(&EEPROM::id);
        m.charges = charges;
        m.temp = lastTemp;
        m.stopReason = reason;
        rfm.write_fsk(42, &m);

        lastSupply = 5;
        charges = 0;
        lastTemp = 1000;
    }

    void charge() {
        log::debug(F("ON!"));
        charges++;
        if (charges > ((capacity / current) * 3600 / chargeTime.getAmount()))  {
            log::debug(F("  timeout!"));
            done(TIMEOUT);
            return;
        } else {
            state = CHARGE;
            pinDisable.setLow();
            stateTick.schedule(chargeTime);
        }
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
                done(TEMP_DT);
            }
        }

        if (t > 400) {
            log::debug(F("  Tmax!"));
            done(TEMP_MAX);
        }
    }

    void measure() {
        for (int i = 0; i < 100; i++) supplyVoltage.get();
        uint32_t supply = 0;
        for (int i = 0; i < 128; i++) supply += supplyVoltage.get();
        supply >>= 7;

        for (int i = 0; i < 100; i++) solarVoltage.get();
        uint32_t solar = 0;
        for (int i = 0; i < 128; i++) solar += solarVoltage.get();
        solar >>= 7;

        log::debug(F("Bat: "), dec(supply), F("mV"));
        log::debug(F("Sol: "), dec(solar), F("mV"));

        ChargeStatus m;
        m.sender = 'n' << 8 | read(&EEPROM::id);
        m.charges = charges;
        m.solarVoltage = solar;
        m.supplyVoltage = supply;
        m.temp = lastTemp;
        rfm.write_fsk(42, &m);

        if (solar < supply + 20) {
            log::debug(F("  night!"));
            done(NIGHT);
        } else if (supply < lastSupply - 2) {
            log::debug(F("  -dV!"));
            done(VOLTAGE_DV);
        } else {
            lastSupply = supply;
            charge();
        }
    }

    void loop() {
        TaskState rfmState = rfm.getTaskState();
        TaskState mainState = TaskState(stateTick, SleepMode::POWER_DOWN);

        if (outputs.isStateChanged()) {
            auto out = outputs.getState().outputs;
            pinOut1.setHigh((out & 1) != 0);
            pinOut2.setHigh((out & 2) != 0);
        }

        if (stateTick.isNow()) {
            switch(state) {
            case CHARGE: cooldown(); break;
            case COOLDOWN: measure(); break;
            case WAIT: measure(); break;
            }
        }

        power.sleepUntilTasks(rfmState, mainState);
    }

    int main() {
        log::debug(F("Charger "));
        temp.measure();
        pinDisable.configureAsOutputHigh();
        pinOut1.configureAsOutputLow();
        pinOut2.configureAsOutputLow();
        while (temp.isMeasuring()) ;
        auto t = temp.getTemperature();
        log::debug(F("Temp: "), dec(t));
        if (t.isDefined()) {
            lastTemp = t.get();
        }
        measure();
        log::debug(F("Init done"));
        while(true) loop();
    }
};
