#include "HAL/Atmel/Device.hpp"
#include "Serial/PulseCounter.hpp"
#include "Time/RealTimer.hpp"

using namespace HAL::Atmel;
using namespace Serial;
using namespace Time;

using namespace HAL::Atmel::Timer;
using namespace HAL::Atmel::Info;

auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto timer1 = Timer1().withPrescaler<1024>().inNormalMode();
auto timer2 = Timer2().withPrescaler<1024>().inNormalMode();

Usart0 usart0(57600);
auto pinPD0 = PinPD0(usart0);
auto pinPD1 = PinPD1(usart0);
auto pinPD2 = PinPD2();
auto pinPD3 = PinPD3(timer2);
auto pinPD4 = PinPD4();
auto pinPD5 = PinPD5(timer0);
auto pinPD6 = PinPD6(timer0);
auto pinPD7 = PinPD7();
auto pinPB0 = PinPB0();
auto pinPB1 = PinPB1();
auto pinPB2 = PinPB2();
auto pinPB3 = PinPB3();
auto pinPB4 = PinPB4();
auto pinPB5 = PinPB5();
auto pinA0 = PinPC0().withInterrupt();

auto comp = timer0.comparatorA();
auto counter = pulseCounter(comp, pinA0, 300_us);
auto rt = realTimer(timer0);
auto doPrint = periodic(rt, 1000_ms);

uint8_t intInvocations = 0;

mkISRS(usart0, pinPD0, pinPD1, pinPD2, pinA0, timer0, timer1, timer2, counter, rt);

int main(void) {
    pinPD2.configureAsOutput();
    pinPD3.configureAsOutput();
    pinPD4.configureAsOutput();
    pinPD5.configureAsOutput();
    pinPD6.configureAsOutput();
    pinPD7.configureAsOutput();
    pinPB0.configureAsOutput();
    pinPB1.configureAsOutput();
    pinPB2.configureAsOutput();
    pinPB3.configureAsOutput();
    pinPB4.configureAsOutput();
    pinPB5.configureAsOutput();
    pinA0.configureAsInputWithPullup();
    bool on = true;
    while (true) {
        if (doPrint.isNow()) {
            pinPD1.out() << "ints: " << dec(intInvocations) << endl;
            pinPD1.out() << "PCICR: " << dec(PCICR) << endl;
            pinPD1.out() << "PCMSK1: " << dec(PCMSK1) << endl;
            pinPD1.out() << "time: " << dec(rt.ticks()) << endl;
            pinPD2.setHigh(on);
            pinPD3.setHigh(on);
            pinPD4.setHigh(on);
            pinPD5.setHigh(on);
            pinPD6.setHigh(on);
            pinPD7.setHigh(on);
            pinPB0.setHigh(on);
            pinPB1.setHigh(on);
            pinPB2.setHigh(on);
            pinPB3.setHigh(on);
            pinPB4.setHigh(on);
            pinPB5.setHigh(on);
            on = !on;
        }

        counter.on([] (auto p) {
            pinPD1.out() << dec(p.getDuration()) << " ";
        });

        uint8_t c;
        if (pinPD0.in() >> c) {
            pinPD1.out() << c;
        }

    }
}
