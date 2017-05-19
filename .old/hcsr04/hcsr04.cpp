#include "Pin.hpp"
#include "SimplePulseTx.hpp"
#include "RealTimer.hpp"
#include "PulseCounter.hpp"

PinD1<> pind1;
Timer0_Normal<ExtPrescaler::_1024> tm0;
Timer1_Normal<ExtPrescaler::_1024> tm1;
Timer2_Normal<IntPrescaler::_32> tm2;
RealTimer<typeof tm0, &tm0> rt;
PinD3<typeof tm2> pinD3(tm2);
PinA0 pinA0;

auto tx = simplePulseTx(pinD3, false);
auto comp = tm0.comparatorA();
typedef PulseCounter<typeof tm1, tm1, typeof comp, comp, typeof pinA0, pinA0, 32, 1> counter_t;
counter_t rx;

uint16_t distance = 0;

void loop() {
    using namespace TimeUnits;

    rt.delayMillis(250);
    tx.send(highPulse(10_us));
    counter_t::PulseEvent evt;
    while (rx.in() >> evt) {
        if (evt.getType() == PulseType::LOW) {
            distance += 0.2f * (int16_t(evt.getLength()) - distance);
            pind1.out() << dec(evt.getLength()) << endl;
        }
    }
}

int main(void) {
    pind1.out() << "Initialized." << endl;
    pinA0.configureAsInputWithPullup();

    while(true) {
        loop();
    }
}
