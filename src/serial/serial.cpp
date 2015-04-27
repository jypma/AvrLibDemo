#include "Pin.hpp"
#include "Timer.hpp"
#include "Usart.hpp"
#include "RealTimer.hpp"
#include "PulseCounter.hpp"
#include "ChunkedFifo.hpp"
#include "FS20Decoder.hpp"

Timer0_Normal<ExtPrescaler::_64> tm0;
Timer1_Normal<ExtPrescaler::_64> tm1;
Timer2_Normal<IntPrescaler::_1024> tm2;
auto comp = tm1.comparatorA();

PinA0 pina0;
typedef PulseCounter<typeof tm0, &tm0, typeof comp, &comp, typeof pina0, &pina0, 254> counter_t;
counter_t counter;
RealTimer<typeof tm2, &tm2> rt;
PinD1<254> pind1;
auto decoder = FS20Decoder<counter_t,254>();

uint8_t changes;

void onChange(volatile void *ctx) {
    changes++;
}

bool handlePulse() {
    counter_t::PulseEvent evt;
    if (counter.in() >> evt) {

        /*
        if (evt.getLength() > 15 && evt.getLength() < 50 ) {
            switch(evt.getType()) {
              case PulseType::LOW: pind1.out() << " ^" << dec(evt.getLength()) << " "; break;
              case PulseType::HIGH: pind1.out() << " _" << dec(evt.getLength()) << " "; break;
              default: break;
            }
        } else {
            pind1.out() << endl;
        }
        */

        decoder.apply(evt);
        return true;
    } else {
        //FS20Packet packet;
        //decoder.in() >> packet;
        uint8_t b;
        if (decoder.in() >> b) {
            pind1.out() << ">" << dec(b) << endl;
            return true;
        }
    }

    return false;
}

int writing = -1;
void loop() {
/*
    if (decoder.bitCount > 10) {
        writing = 100;
    }
    if (writing > 0) {
        pind1.out() << " [ " << dec(uint8_t(decoder.state)) << " " << dec(decoder.bitCount) << " " << dec(decoder.byteCount) << "]" << endl;
        writing--;
    }

    if (!decoder.logOn) {
        for (int i = 0; i < 255; i++) {
            pind1.out() << dec(decoder.log[i]) << " ";
        }
        pind1.out() << endl;
    }
*/
    //rt.delayMillis(50);
    //pind1.out() << dec(decoder2.count) << " / " << dec(uint8_t(decoder2.state)) << " / " << dec(decoder2.command) << endl;

    /*
    pind1.out() << "tm1: " << dec(TCNT1) << " ocr1a: " << dec(OCR1A) << endl;
    pind1.out() << "a0: " << (pina0.isHigh() ? "high" : "low") << endl;
    pind1.out() << "changes: " << dec(changes) << endl;

    pind1.out() << "tm0: " << dec(TCNT0) << endl;
    pind1.out() << "tm2: " << dec(TCNT2) << endl;

    pind1.out() << "rt: " << dec(uint16_t(rt.counts() >> 16)) << endl;
    */

    while (handlePulse()) ;
 }

int main(void) {
  pina0.configureAsInputWithPullup();
  pind1.out() << "hahaha" << endl;
  pind1.out() << dec(decoder.zero_length) << " / " << dec(decoder.one_length) << endl;


  while(true) {
    loop();
  }
}

