#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "Mechanic/Button.hpp"
#include "HAL/Atmel/Power.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Mechanic;
using namespace Streams;

#define DEBUG
#ifdef DEBUG
Usart0 usart0(57600);
auto pinTX = PinPD1(usart0);
#endif
auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto timer1 = Timer1().withPrescaler<1>().inFastPWMMode();
auto rt = realTimer(timer0);
auto lampPin = PinPB1(timer1);
auto button1Pin = PinPC0().withInterrupt();
auto button2Pin = PinPC1().withInterrupt();
auto button3Pin = PinPC2().withInterrupt();
auto button4Pin = PinPC3().withInterrupt();

auto btn1 = button(rt, button1Pin);
auto btn2 = button(rt, button2Pin);
auto btn3 = button(rt, button3Pin);
auto btn4 = button(rt, button4Pin);
auto anim = animator(rt);
auto power = Power();
auto autoOff = deadline(rt, 30_min);

#ifdef DEBUG
//mkISRS(timer0, timer1, rt, usart0, pinTX);
#else
//mkISRS(timer0, timer1, rt);
#endif

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

int main() {
#ifdef DEBUG
    pinTX.out() << "1 second is " << dec(uint32_t(toCountsOn(rt, 1000_ms))) << endl;
#endif
    button1Pin.interruptOnChange();
    button2Pin.interruptOnChange();
    button3Pin.interruptOnChange();
    button4Pin.interruptOnChange();
    lampPin.configureAsOutput();
    lampPin.setLow();
    lampPin.timerComparator().setOutput(FastPWMOutputMode::connected);
    lampPin.timerComparator().setTargetFromNextRun(0);
    while (true) {
        auto evt = anim.nextEvent();
        if (evt.isChanged()) {
#ifdef DEBUG
            pinTX.out() << "val: " << dec(evt.getValue()) << endl;
#endif
            lampPin.timerComparator().setTargetFromNextRun(evt.getValue());
        }

        if (evt.isCompleted() && mode == Mode::OFF) {
            power.powerDown();
        }

        if (autoOff.isNow() && mode != Mode::OFF) {
            turn(Mode::OFF);
        }

        if (btn1.nextEvent() == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.out() << "----------button1-----------------" << endl;
#endif
            autoOff.reset();
            turn(Mode::OFF);
        }

        if (btn2.nextEvent() == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.out() << "----------button2-----------------" << endl;
#endif
            autoOff.reset();
            turn(Mode::DIM1);
        }

        if (btn3.nextEvent() == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.out() << "----------button3-----------------" << endl;
#endif
            autoOff.reset();
            turn(Mode::DIM2);
        }

        if (btn4.nextEvent() == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.out() << "----------button4-----------------" << endl;
#endif
            autoOff.reset();
            turn(Mode::ON);
        }

    }
}
