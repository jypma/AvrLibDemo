#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "Mechanic/Button.hpp"
#include "HAL/Atmel/Power.hpp"
#include "Passive/SupplyVoltage.hpp"

struct EEPROM {
    uint16_t bandgapVoltage;
};

using namespace HAL::Atmel;
using namespace Time;
using namespace Mechanic;
using namespace Streams;
using namespace Passive;

#define DEBUG

Usart0 usart0(57600);
auto pinTX = PinPD1(usart0);
auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto timer1 = Timer1().withPrescaler<1>().inFastPWMMode();
auto rt = realTimer(timer0);
auto lampPin = PinPB1(timer1);
auto button1Pin = PinPC1().withInterrupt(); // strange pairwise twisted wiring of the ebay button strip
auto button2Pin = PinPC0().withInterrupt();
auto button3Pin = PinPC3().withInterrupt();
auto button4Pin = PinPC2().withInterrupt();

auto btn1 = button(rt, button1Pin);
auto btn2 = button(rt, button2Pin);
auto btn3 = button(rt, button3Pin);
auto btn4 = button(rt, button4Pin);
auto anim = animator(rt);
auto power = Power(rt);
auto adc = ADConverter();
auto supply1Pin = PinPC4();
auto supply1 = SupplyVoltage<10030, 679, &EEPROM::bandgapVoltage>(adc, supply1Pin);

mkISRS(timer0, timer1, rt, button1Pin, button2Pin, button3Pin, button4Pin, btn1, btn2, btn3, btn4, usart0, pinTX, power, adc, supply1Pin, supply1);

auto autoOff = deadline(rt, 30_min);
auto checkSupply = periodic(rt, 1_min);

enum class Mode: uint8_t { OFF, DIM1, DIM2, ON };
Mode mode = Mode::OFF;

void turn(Mode m) {
    mode = m;
    switch(mode) {
    case Mode::OFF:
        anim.to(0, 2000_ms, AnimatorInterpolation::EASE_UP);
        break;
    case Mode::ON:
        anim.to(0xFFFF, 1000_ms, AnimatorInterpolation::EASE_UP);
        break;
    case Mode::DIM1:
        anim.to(2000, 1000_ms, AnimatorInterpolation::EASE_UP);
        break;
    case Mode::DIM2:
        anim.to(10000, 1000_ms, AnimatorInterpolation::EASE_UP);
        break;
    }
}

void doSupply() {
    auto supply = supply1.get();
    pinTX.write(F("Supply 1 is "), dec(supply), F("mV."), endl);
}

int main() {
#ifdef DEBUG
    pinTX.write(F("1 second is "), dec(uint32_t(toCountsOn(rt, 1000_ms))), endl);
    pinTX.write(F("Vbandgap is "), dec(read(&EEPROM::bandgapVoltage)), endl);
#endif
    lampPin.configureAsOutput();
    lampPin.setLow();
    lampPin.timerComparator().setOutput(FastPWMOutputMode::connected);
    lampPin.timerComparator().setTargetFromNextRun(0);
    while (true) {
        auto animEvt = anim.nextEvent();
        if (animEvt.isChanged()) {
#ifdef DEBUG
            //pinTX.write(F("val: "), dec(animEvt.getValue()), endl);
#endif
            lampPin.timerComparator().setTargetFromNextRun(animEvt.getValue());
            //doSupply();
        }

        if (autoOff.isNow() && mode != Mode::OFF) {
            turn(Mode::OFF);
        }

        auto btnEvt1 = btn1.nextEvent();
        if (btnEvt1 == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.write(F("----------button1-----------------"), endl);
#endif
            autoOff.reset();
            turn(Mode::OFF);
        }

        auto btnEvt2 = btn2.nextEvent();
        if (btnEvt2 == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.write(F("----------button2-----------------"), endl);
#endif
            autoOff.reset();
            turn(Mode::DIM1);
        }

        auto btnEvt3 = btn3.nextEvent();
        if (btnEvt3 == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.write(F("----------button3-----------------"), endl);
#endif
            autoOff.reset();
            turn(Mode::DIM2);
        }

        auto btnEvt4 = btn4.nextEvent();
        if (btnEvt4 == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.write(F("----------button4-----------------"), endl);
#endif
            autoOff.reset();
            turn(Mode::ON);
        }

        if (animEvt.isIdle()
                && (btnEvt1 == ButtonEvent::RELEASED || btnEvt1 == ButtonEvent::UP)
                && (btnEvt2 == ButtonEvent::RELEASED || btnEvt2 == ButtonEvent::UP)
                && (btnEvt3 == ButtonEvent::RELEASED || btnEvt3 == ButtonEvent::UP)
                && (btnEvt4 == ButtonEvent::RELEASED || btnEvt4 == ButtonEvent::UP)) {

            auto target = (animEvt.getValue() == 0 || animEvt.getValue() == 0xFFFF) ? SleepMode::POWER_DOWN : SleepMode::IDLE;
#ifdef DEBUG
            pinTX.flush();
#endif
            power.sleepUntil(autoOff, target);
        }

    }
}
