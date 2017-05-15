#include "HAL/Atmel/Device.hpp"
#include "Serial/PulseCounter.hpp"
#include "Time/RealTimer.hpp"
#include "FS20/FS20Decoder.hpp"
#include "Visonic/VisonicDecoder.hpp"

using namespace HAL::Atmel;
using namespace Serial;
using namespace Time;
using namespace FS20;
using namespace Visonic;

auto timer0 = Timer0().withPrescaler<64>().inNormalMode();
Usart0 usart0(57600);
auto pinTX = PinPD1(usart0);
auto pinOOK = PinPD3();
//auto pinOOK = JeeNodePort1A::withInterrupt();
auto pinOOK_EN = PinPD4();

auto comp = timer0.comparatorA();
auto counter = pulseCounter(comp, pinOOK, 150_us);
auto rt = realTimer(timer0);
auto every1sec = periodic(rt, 1000_ms);

auto fs20 = fs20Decoder(counter);
auto visonic = visonicDecoder(counter);

mkISRS(usart0, timer0, pinTX, pinOOK, counter, rt);


constexpr uint16_t eventMax = 300;
constexpr uint8_t pulsesPerLoop = 4;
uint16_t eventSeq[eventMax+pulsesPerLoop];
uint32_t pulses = 0;
uint16_t eventPos = 0;
uint8_t eventLongs = 0;
bool printNext = false;
uint8_t hours, minutes, seconds;

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
        pinTX.out() << dec(eventPos) << ": ";
        for (uint16_t i = 0; i < eventPos; i++) {
            pinTX.out() << dec(eventSeq[i]) << " ";
        }
        pinTX.out() << endl;
        printNext = false;
        eventPos = 0;
        eventLongs = 0;
    } else if (eventPos >= eventMax) {
        eventPos = 0;
        eventLongs = 0;
    }


}

bool callPrint;
void handlePulse() {

    //if (doPrint.isNow()) {
    //    pinTX.out() << " p: " << dec(pulses) << " o: " << dec(counter.getOverflows()) << endl;
    //    pinTX.out() << "d3: " << pinOOK.isHigh() << endl;
   // }

    callPrint = false;
    counter.onMax(pulsesPerLoop, [] (auto pulse) {

        pulses++;

        //pinTX.out() << (pulse.isHigh() ? " ^" : " _") << dec(pulse.getDuration());

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

    /*
    uint8_t reason;
    uint8_t state;
    uint8_t byteCount;
    int8_t bitCount;
    if (fs20.failures.in() >> reason >> state >> byteCount >> bitCount) {
        pinTX.out() << ":( " << dec(reason) << " " << dec(state) << " " << dec(byteCount) << " " << dec(bitCount) << endl;
    }
*/

    if (callPrint) {
        printEvents();
    }



        FS20Packet fs20Packet;
        if (fs20.in() >> fs20Packet) {
            pinTX.out() << dec(hours) << ":" << dec(minutes) << ":" << dec(seconds) << " FS20: " << dec(fs20Packet.houseCodeHi) << ", " << dec(fs20Packet.houseCodeLo) << ", " << dec(fs20Packet.address) << ", " << dec(fs20Packet.command) << endl;
        }

        VisonicPacket visonicPacket;
        if (visonic.in() >> visonicPacket) {
            pinTX.out() << dec(hours) << ":" << dec(minutes) << ":" << dec(seconds) << " Viso: " << dec(visonicPacket.data[0]) << ", " << dec(visonicPacket.data[1]) << ", " << dec(visonicPacket.data[2]) << ", " << dec(visonicPacket.data[3]) << ", " << dec(visonicPacket.data[4]) << "(" << dec(visonicPacket.lastBit) << "," << (visonicPacket.flipped ? '1' : '0') << ")" << endl;

        }

}

int main(void) {
    pinOOK_EN.configureAsOutput();
    pinOOK_EN.setHigh();
    pinOOK.configureAsInputWithPullup();
    pinTX.out() << "ookdecode. 100ms="
            << dec(uint8_t(toCountsOn(timer0, 300_us))) << " TCCR0A="
            << dec(TCCR0A) << endl;

    while (true) {
        if (every1sec.isNow()) {
            seconds++;
            if (seconds >= 59) {
                seconds = 0;
                minutes++;
                if (minutes >= 59) {
                    minutes = 0;
                    hours++;
                }
            }
            //pinTX.out() << "bits: " << dec(visonic.totalBits) << endl;
        }
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
