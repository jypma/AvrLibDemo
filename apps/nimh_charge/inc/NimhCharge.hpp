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
#include "HopeRF/RxTxState.hpp"
#include "Tasks/loop.hpp"
#include "Tasks/Task.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Passive;
using namespace Streams;
using namespace Dallas;
using namespace HopeRF;

enum ChargeState {
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

struct NimhCharge: public Task {
    typedef NimhCharge This;
    typedef Logging::Log<Loggers::Main> log;

    SPIMaster spi;
    ADConverter<uint16_t> adc;

    Usart0 usart0 = { 57600 };
    auto_var(pinTX, PinPD1<250>(usart0));
    auto_var(timer0, Timer0::withPrescaler<1024>::inNormalMode());

    auto_var(rt, realTimer(timer0));
    auto_var(power, Power(rt));

    auto_var(pinDisable, JeeNodePort3D());
    auto_var(pinSupply, JeeNodePort3A());
    auto_var(pinSolar, JeeNodePort4A());
    auto_var(pinRFM12_INT, PinPD2());
    auto_var(pinRFM12_SS, PinPB2());
    auto_var(pinTemp, JeeNodePort4D());
    auto_var(pinOut1, JeeNodePort1D());
    auto_var(pinOut2, JeeNodePort2D());

    auto_var(pinSupplyCurrent, JeeNodePort2A());
    auto_var(pinSupplyCurrentSign, JeeNodePort1A());

    auto_var(rfm, (rfm12<200,200>(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), RFM12Band::_868Mhz)));
    auto_var(outputs, (RxTxState<decltype(rfm), decltype(rt), Outputs>(rfm, rt, {}, 'n' << 8 | read(&EEPROM::id))));

    auto_var(supplyVoltage, (SupplyVoltage<10000, 1000, &EEPROM::bandgapVoltage>(adc, pinSupply)));
    auto_var(solarVoltage, (SupplyVoltage<10000, 500, &EEPROM::bandgapVoltage>(adc, pinSolar)));

    auto_var(tempWire, OneWireParasitePower(pinTemp, rt));
    auto_var(temp, SingleDS18x20(tempWire));

    auto_var(chargeTick, deadline(rt));
    auto_var(radioTick, deadline(rt));
    auto_var(tempTick, periodic(rt, 1_min));

    ChargeState state = CHARGE;
    uint16_t lastSupply = 5;
    Option<int16_t> lastTemp = none();
    uint16_t charges = 0;
    bool isRadioOn = false;

    static constexpr uint16_t current = 700;  // mA
    static constexpr uint16_t capacity = 2000; // mAh

    static constexpr auto chargeTime = 10_sec;
    static constexpr auto waitTime = 30_min;   // wait this long before deciding to top up again
    static constexpr auto radioOnTime = 1_sec;
    static constexpr auto radioOffTime = 59_sec;

    static constexpr uint16_t cutoffSupply = 6300; // mV

    typedef Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(pinTX), &This::pinTX,
            Delegate<This, decltype(rfm), &This::rfm,
            Delegate<This, decltype(power), &This::power>>>>
            Handlers;

    void done(uint8_t reason) {
        pinDisable.setHigh();
        state = WAIT;
        chargeTick.schedule(waitTime);

        ChargeStatus m;
        m.sender = 'n' << 8 | read(&EEPROM::id);
        m.charges = charges;
        m.temp = lastTemp;
        m.stopReason = reason;
        rfm.write_fsk(42, &m);
        rfm.write_fsk(42, &m);
        rfm.write_fsk(42, &m);

        lastSupply = 5;
        charges = 0;
        lastTemp = none();
    }

    void charge() {
        log::debug(F("ON!"));
        charges++;
        if (charges > ((capacity / current) * 3600 * 2 /* guess 0.5 of max current given Danish sun */ / chargeTime.getAmount()))  {
            log::debug(F("  timeout!"));
            done(TIMEOUT);
            return;
        } else {
            state = CHARGE;
            pinDisable.setLow();
            chargeTick.schedule(chargeTime);
        }
    }

    void cooldown() {
        log::debug(F("off"));
        state = COOLDOWN;
        pinDisable.setHigh();
        chargeTick.schedule(2_sec);

        temp.measure();
        while (temp.isMeasuring()) ;
        auto t = temp.getTemperature();
        log::debug(F("Temp: "), dec(t));
        if (tempTick.isNow()) {
            if ((lastTemp + 11) <= t) {   // 1.1°C / min
                log::debug(F("  dT!"));
                lastTemp = t.get();
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

        // radio packets don't always make it. So, tx 3 times, and idle before charging again.
        rfm.write_fsk(42, &m);
        rfm.write_fsk(42, &m);
        rfm.write_fsk(42, &m);
        uint16_t timeout = 60000;
        while (!rfm.isIdle() && (timeout-- > 1)) ;

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

        if (supply < cutoffSupply) {
            pinOut1.setLow();
            pinOut2.setLow();
        }
    }

    void radioOn() {
        log::debug(F("rOn"));
        rfm.onIdleListen();
        isRadioOn = true;
        outputs.requestLatest();
        radioTick.schedule(radioOnTime);
    }

    void radioOff() {
        log::debug(F("rOff"));
        rfm.onIdleSleep();
        isRadioOn = false;
        radioTick.schedule(radioOffTime);
    }

    uint8_t ints = 0;
    void loop() {
        supplyVoltage.stopOnLowBattery(6000, [this] {
            log::debug(F("Oh-oh"));
            rfm.onIdleSleep();
            pinOut1.setLow();
            pinOut2.setLow();
            uint16_t timeout = 60000;
            while (timeout-- > 0 && !rfm.isIdle()) ;
            log::flush();
        });

        if (outputs.isStateChanged()) {
            auto out = outputs.get().outputs;
            log::debug('o', dec(out));
            bool enable = (lastSupply < 100 || lastSupply >= cutoffSupply);
            pinOut1.setHigh(enable && ((out & 1) != 0));
            pinOut2.setHigh(enable && ((out & 2) != 0));
        }
        if (isRadioOn && outputs.isSynchronized()) {
            log::flush();
            log::debug('s');
            log::flush();
            radioOff();
        }

        if (rfm.in().hasContent()) {
            log::debug('i');
            // ignore all other incoming RFM packets
            rfm.in().readStart();
            rfm.in().readEnd();
        }
    }

  void onCharge() {
            switch(state) {
            case CHARGE: cooldown(); break;
            case COOLDOWN: measure(); break;
            case WAIT:
                temp.measure();
                while (temp.isMeasuring()) ;
                auto t = temp.getTemperature();
                if (t.isDefined()) {
                    lastTemp = t.get();
                }
                measure();
                break;
            }
  }

  void onRadio() {
            if (isRadioOn) {
                radioOff();
            } else {
                radioOn();
            }
    }

    int main() {
        log::debug(F("Charger "));
        radioOn();
        temp.measure();
        pinDisable.configureAsOutputHigh();
        pinOut1.configureAsOutputLow();
        pinOut2.configureAsOutputLow();
        log::debug(F("waiting"));
        log::flush();
        while (temp.isMeasuring()) ;
        log::debug(F("getting"));
        log::flush();
        auto t = temp.getTemperature();
        log::debug(F("Temp: "), dec(t));
        log::flush();
        if (t.isDefined()) {
            lastTemp = t.get();
        }
        measure();
        log::debug(F("Init done"));
        log::flush();

        auto chargeTask = chargeTick.invoking<This, &This::onCharge>(*this);
        auto radioTask = radioTick.invoking<This, &This::onRadio>(*this);
        while(true) {
          loopTasks(power, *this, rfm, chargeTask, radioTask);
        }
    }
};
