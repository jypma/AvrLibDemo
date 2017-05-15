#include "HAL/Atmel/Device.hpp"

using namespace HAL;

int main() {
	DDRB |= DDB0 | DDB2;
	DDRB |= DDB1;
	DDRC |= DDC0;
}

/*
#include "Pin.hpp"
#include "Timer.hpp"
#include "Usart.hpp"
#include "RealTimer.hpp"
#include "PulseCounter.hpp"
#include "ChunkedFifo.hpp"
#include "IRDecoder.hpp"

Timer0_Normal<ExtPrescaler::_256> tm0;
Timer1_Normal<ExtPrescaler::_256> tm1;
Timer2_Normal<IntPrescaler::_1024> tm2;
auto comp = tm1.comparatorA();

PinA0 pina0;
typedef PulseCounter<typeof tm1, tm1, typeof comp, comp, typeof pina0, pina0, 254> counter_t;
counter_t counter;
RealTimer<typeof tm0, &tm0> rt;
PinD1<> pind1;
auto decoder1 = IRDecoder_NEC<counter_t>();
auto decoder2 = IRDecoder_Samsung<counter_t>();

bool handlePulse() {
    counter_t::PulseEvent evt;
    if (counter.in() >> evt) {

        decoder1.apply(evt);
        decoder2.apply(evt);
        return true;
    } else {
        IRCode code;
        if (decoder1.in() >> code) {
            pind1.out() << "nec : " << dec(uint8_t(code.getType())) << "," << dec(code.getCommand()) << endl;
            return true;
        } else if (decoder2.in() >> code) {
            pind1.out() << "sams: " << dec(uint8_t(code.getType())) << "," << dec(code.getCommand()) << endl;
            return true;
        } else {
            return false;
        }
    }
}

void loop() {
    //rt.delayMillis(50);
    //pind1.out() << dec(decoder2.count) << " / " << dec(uint8_t(decoder2.state)) << " / " << dec(decoder2.command) << endl;
    while (handlePulse()) ;
 }

int main(void) {
  pind1.out() << "ready." << endl;

  while(true) {
    loop();
  }
}

*/
