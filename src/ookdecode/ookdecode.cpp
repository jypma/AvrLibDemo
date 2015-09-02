#include "Pin.hpp"
#include "Timer.hpp"
#include "Usart.hpp"
#include "RealTimer.hpp"
#include "Serial/PulseCounter.hpp"
#include "FS20/FS20Decoder.hpp"
#include "Visonic/VisonicDecoder.hpp"
#include "Logging.hpp"

using namespace TimeUnits;
using namespace FS20;
using namespace Visonic;

Timer0_Normal<ExtPrescaler::_64> tm0;
Timer1_Normal<ExtPrescaler::_8> tm1;
Timer2_Normal<IntPrescaler::_1024> tm2;
auto comp = tm0.comparatorA();

Usart0 usart0;
PinD0<Usart0, 254> pinD0 (usart0);
PinD1<Usart0, 254> pinD1 (usart0);
PinD3<> pulsePin;
PinD9<> pinD9;
PinA0 pinA0;
PinA1 pinA1;
auto counter = pulseCounter<typeof comp, typeof pulsePin, 250>(comp, pulsePin, (30_us).template toCounts<typeof tm0>());
auto rt = realTimer(tm2);
auto fs20 = fs20Decoder(counter);
auto visonic = visonicDecoder(counter);
auto every100ms = periodic(rt, 100_ms);
auto every1sec = periodic(rt, 1000_ms);

constexpr uint16_t eventMax = 300;
constexpr uint8_t pulsesPerLoop = 4;
uint16_t eventSeq[eventMax+pulsesPerLoop];
uint32_t pulses = 0;
uint16_t eventPos = 0;
uint8_t eventLongs = 0;
bool printNext = false;

void printEvents() {
    /*
    bool print = false;
    if (printNext) {
        print = true;
        printNext = false;
    }
    if (eventPos > 30 && eventLongs > 0) {
        print = true;
        printNext = true;
    }

    if (print) {
    */
    if (eventLongs > 10) {
        pinD1.out() << dec(eventPos) << ": ";
        for (uint16_t i = 0; i < eventPos; i++) {
            pinD1.out() << dec(eventSeq[i]) << " ";
        }
        pinD1.out() << endl;
        printNext = false;
        eventPos = 0;
        eventLongs = 0;
    } else if (eventPos >= eventMax) {
        eventPos = 0;
        eventLongs = 0;
    }


}

extern uint8_t int1_invocations;

bool callPrint;
void handlePulse() {

    if (every1sec.isNow()) {
        pinD1.out() << "v: " << dec(visonic.totalBits) << " p: " << dec(pulses) << " o: " << dec(counter.getOverflows()) << "i: " << dec(int1_invocations) << endl;
        pinD1.out() << "d3: " << pulsePin.isHigh() << endl;
    }


    callPrint = false;
    counter.onMax(pulsesPerLoop, [] (auto pulse) {

        pulses++;

        //if (evt.getLength() > (100_us).template toCounts<typeof tm1>() && evt.getLength() < (1000_us).template toCounts<typeof tm1>() ) {

/*
        if (pulse.getDuration() > (100_us).template toCounts<typeof tm0>()) {
            if (pulse.getDuration() > (600_us).template toCounts<typeof tm0>()) {
                eventLongs++;
            }
            eventSeq[eventPos] = pulse.getDuration();
            eventPos++;
            if (eventPos >= eventMax) {
                callPrint = true;
            }
        } else {
            eventSeq[eventPos] = pulse.getDuration();
            eventPos++;
            callPrint = true;
        }
*/
        fs20.apply(pulse);
/*
        if (fs20.state == State::SYNC && fs20.bitCount > 2) {
            pinD1.out() << ' ' << dec(fs20.bitCount) ;
        }
*/
        visonic.apply(pulse);
    });


    if (callPrint) {
        printEvents();
    }



        FS20Packet fs20Packet;
        if (fs20.in() >> fs20Packet) {
            pinD1.out() << "FS20: " << dec(fs20Packet.houseCodeHi) << ", " << dec(fs20Packet.houseCodeLo) << ", " << dec(fs20Packet.address) << ", " << dec(fs20Packet.command) << endl;
        }

        VisonicPacket visonicPacket;
        if (visonic.in() >> visonicPacket) {
            pinD1.out() << "Viso: " << dec(visonicPacket.data[0]) << ", " << dec(visonicPacket.data[1]) << ", " << dec(visonicPacket.data[2]) << ", " << dec(visonicPacket.data[3]) << ", " << dec(visonicPacket.data[4]) << endl;

        }

}

int main(void) {
  pinD9.configureAsOutput();
  pinD1.out() << "ookdecode. 100ms=" << dec((100_us).template toCounts<typeof tm1>()) << endl;

  while(true) {
    handlePulse();
  }
}

/*

Viso: 55, 18, 28, 221, 6   <-- while movement in stue...really?
Viso: 52, 18, 28, 221, 6   <-- shows repeatedly while being in stue.
Viso: 203, 237, 19, 221, 6
Viso: 104, 36, 56, 186, 13
Viso: 105, 36, 56, 186, 13
 */
