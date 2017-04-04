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

using namespace HAL::Atmel;
using namespace Time;
using namespace HopeRF;
using namespace Mechanic;
using namespace Passive;
using namespace Streams;
using namespace Dallas;

struct Message {
    uint8_t seq;
    uint16_t sender;
    Option<uint16_t> supply;
    Option<uint8_t> open;
    Option<int16_t> temperature;

    typedef Protobuf::Protocol<Message> P;

    typedef P::Message<
        P::Varint<1, uint16_t, &Message::sender>,
        P::Varint<8, uint8_t, &Message::seq>,
        P::Varint<9, Option<uint16_t>, &Message::supply>,
        P::Varint<10, Option<uint8_t>, &Message::open>,
        P::Varint<11, Option<int16_t>, &Message::temperature>
    > DefaultProtocol;
};

struct DoorSensor {
    typedef DoorSensor This;
    typedef Logging::Log<Loggers::Main> log;

    SPIMaster spi;
    ADConverter<uint16_t> adc;

    Usart0 usart0 = { 57600 };
    auto_var(pinTX, PinPD1(usart0));
    auto_var(timer0, Timer0::withPrescaler<1024>::inNormalMode());

    auto_var(rt, realTimer(timer0));
    auto_var(pinRFM12_SS, PinPB2());
    auto_var(pinRFM12_INT, PinPD2());
    auto_var(sensorPin, PinPB1().withInterrupt());
    auto_var(sensor, Button(rt, sensorPin, 200_ms));

    auto_var(tempPin, PinPD4());
    auto_var(tempWire, OneWireParasitePower(tempPin, rt));
    auto_var(temp, SingleDS18x20(tempWire));

    auto_var(rfm, rfm12(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), RFM12Band::_868Mhz));
    auto_var(power, Power(rt));
    auto_var(pinSupply, PinPC0());
    auto_var(supplyVoltage, (SupplyVoltage<4700, 1000, &EEPROM::bandgapVoltage>(adc, pinSupply)));

    auto_var(resend, periodic(rt, 10_sec));

    uint8_t seq;
    bool measuring;

    typedef Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(rfm), &This::rfm,
            Delegate<This, decltype(pinTX), &This::pinTX,
            Delegate<This, decltype(sensor), &This::sensor,
            Delegate<This, decltype(power), &This::power>>>>>
            Handlers;

    void send(bool open) {
        log::debug(F("Open: "), '0' + open);
        seq++;
        Message m;
        m.seq = seq;
        m.sender = 'd' << 8 | read(&EEPROM::id);
        m.temperature = temp.getTemperature();
        log::debug(F("Temp: "), dec(m.temperature));
        for (int i = 0; i < 10; i++) supplyVoltage.get();
        m.supply = supplyVoltage.get();
        log::debug(F("Supl: "), dec(m.supply));
        m.open = open ? 1 : 0;
        rfm.write_fsk(42, &m);
        resend.reschedule();
    }

    void loop() {
        auto rfmState = rfm.getTaskState();
        TaskState tempState = temp.getTaskState();
        auto resendState = TaskState(resend, SleepMode::POWER_DOWN);

        if (resend.isNow()) {
            log::debug(F("Go"));
            measuring = true;
            temp.measure();
        }
        if (measuring && tempState.isIdle()) {
            measuring = false;
            send(sensor.isUp());
        }

        switch (sensor.nextEvent()) {
            case ButtonEvent::PRESSED:
                send(false);
                break;
            case ButtonEvent::RELEASED:
                send(true);
                break;
            default: break;
        }

        log::flush();
        power.sleepUntilTasks(rfmState, tempState, resendState);
    }

    int main() {
        sensorPin.configureAsInputWithoutPullup();
        rfm.onIdleSleep();
        log::debug(F("DoorSensor "));
        for (int i = 0; i < 100; i++) supplyVoltage.get();
        while(true) loop();
    }
};
