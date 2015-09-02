#include "Pin.hpp"
#include "Timer.hpp"
#include "Usart.hpp"
#include "RealTimer.hpp"
#include "Serial/PulseCounter.hpp"
#include "ChunkedFifo.hpp"
#include "FS20/FS20Decoder.hpp"
#include "Serial/SerialTx.hpp"
#include "Serial/RS232.hpp"
#include "Serial/SimplePulseTx.hpp"

using namespace Serial;
using namespace FS20;

Timer0_Normal<ExtPrescaler::_8> tm0;
Timer1_Normal<ExtPrescaler::_8> tm1;
Timer2_Normal<IntPrescaler::_1024> tm2;
auto comp = tm1.comparatorA();

PinA0 pinA0;
auto counter = pulseCounter(comp, pinA0, 1);
auto rt = realTimer(tm2);
Usart0 usart0;
PinD0<Usart0,16> pinD0(usart0);
PinD1<Usart0,254> pinD1(usart0);
//auto decoder = fs20Decoder(counter);

//PinD5<typeof tm0> serial_pin(tm0);
PinD6<> pinD6;
PinD9<> pinD9;
//SerialConfig cfg_8n1 = RS232::_8n1<typeof serial_pin, 9600>::serialconfig;
Fifo<32> srcFifo;
//auto src = StreamPulseSource(srcFifo, cfg_8n1);
//auto src = SimplePulseTxSource<254>(true);
//auto softSerial = pulseTx(serial_pin, src);

uint8_t changes;
//ADConverter adc;

void onChange(volatile void *ctx) {
    changes++;
}

//bool handlePulse() {
    //PulseEvent<Timer1Info::value_t> evt;
    //if (counter.in() >> evt) {

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
/*
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

    return false;*/
//}

int writing = -1;
void loop() {
    //for (uint8_t i = 0; i < debugTimingsCount; i++ ){
    //    pind1.out() << dec(debugTimings[i]) << " ";
    // }
    uint8_t ch;
    if (pinD0.in() >> ch) {
        pinD1.out() << "got: " << ch << endl;
        pinD1.flush();
    }
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

    //while (handlePulse()) ;
    //pind1.out() << "hi " << dec(softSerial.pulsesHigh) << " lo " << dec(softSerial.pulsesLow) << endl;

    //pinD6.setHigh();
    //rt.sleepMillis(1000);
    //pinD6.setLow();
/*
    pind1.out() << "!" << endl;

    //TCCR0A = (TCCR0A | (1 << COM0B1)) & ~(1 << COM0B0); // low on match
    //TCCR0B |= (1 << FOC0B);  // force output OC0B, to low.

    //auto target = serial_pin.timer().getValue() + serial_pin.timer().template microseconds2counts<17>();

    cli();
    serial_pin.setLow();
    auto before = serial_pin.timer().getValue();
    auto target = before + 34;
    serial_pin.timerComparator().setTarget(target);
    serial_pin.timerComparator().setOutput(NonPWMOutputMode::high_on_match);
    sei();
    auto after = serial_pin.timer().getValue();
    pind1.out() << dec(before) << " " << dec(after) << endl;
    //TCCR0A = (TCCR0A | (1 << COM0B1)) | (1 << COM0B0); // high on match

    rt.delayMillis(100);
    //adc.start(pinA0);
    //uint16_t value = adc.awaitValue();
    //pind1.out() << " ADC: " << dec(value) << endl;
     *
     */
 }

int main(void) {
  //serial_pin.setHigh(); // make this part of the setup, to use the serial config's idle value when doing confireAsOutput()
  //constexpr uint32_t usecs = 1000000l / 57600l;
  //constexpr uint8_t counts = tm1.template microseconds2counts<usecs>();

  pinD6.configureAsOutput();
  pinD9.configureAsOutput();

  //pinA0.configureAsInputWithPullup();
  pinD1.out() << "hello, software" << endl;
  //pinD1.flush();
  //pinD1.out() << dec(TCCR0B) << endl;
  //pind1.out() << dec(cfg_8n1.one_a.getDuration()) << endl;
  //pinD1.out() << dec(usecs) << endl;
  //pind1.out() << dec(counts) << endl; // 34 counts, for prescaler 8


  //srcFifo.out() << "yyyyyy" << endl;
/*  src.append(Pulse(false, 125));
  src.append(Pulse(false, 125));
  src.append(Pulse(true, 125));
  src.append(Pulse(false, 125));
  src.append(Pulse(true, 125));
  src.append(Pulse(false, 125));
  src.append(Pulse(false, 125));
  src.append(Pulse(false, 125));
  src.append(Pulse(true, 125));
  */
  //softSerial.sendFromSource();
  /*
  src.append(Pulse(20,true));
  src.append(Pulse(10,false));
  src.append(Pulse(20,true));
  src.append(Pulse(10,false));
  src.append(Pulse(20,true));
  src.append(Pulse(10,false));
  src.append(Pulse(20,true));
  src.append(Pulse(10,false));
  src.append(Pulse(20,true));
  src.append(Pulse(10,false));
  */
  while(true) {
    loop();
  }
}

