#include "HAL/Atmel/Device.hpp"
#include "HopeRF/RFM12.hpp"
#include "Time/RealTimer.hpp"
#include "SPI.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace FS20;
using namespace Serial;
using namespace HopeRF;

Usart0 usart0(115200);
SPIMaster spi;

auto timer0 = Timer0().withPrescaler<64>().inNormalMode();
auto rt = realTimer(timer0);
auto comp = timer0.comparatorA();
auto pinTX = PinPD1<254>(usart0);
auto pinRFM12_INT = PinPD2();
auto pinRFM12_SS = PinPB2();

auto rfm = rfm12(spi, pinRFM12_SS, pinRFM12_INT, comp, RFM12Band::_868Mhz);

mkISRS(usart0, timer0, comp, pinTX, rt, rfm);

uint32_t packets = 0;
auto onPing = periodic(rt, 1000_ms);

void log(const char *msg) {
    pinTX.out() << msg << endl;
}

void loop() {
    //loops++;
    /*
    if (loops > 100000) {
      pind1.out() << "loop, " << dec(rfm12.ints) << endl;
      loops = 0;
      pind1.out() << "recvCount: " << dec(rfm12.recvCount);
      pind1.out() << "lastLen: " << dec(rfm12.lastLen);
      pind1.out() << "underruns: " << dec(rfm12.underruns);
      pind1.out() << "lens: " << dec(rxLengths.getSize());
      pind1.out() << "data: " << dec(rxData.getSize());
      pind1.out() << endl;
    }
    */
    //if (rx.isWriting()) {
    //    pind1 << "writing" << endl;
   // }
    //if (rfm12.ints > 2) {

    //}

    auto in = rfm.in();
    if (in) {
        packets++;
        pinTX.out() << "[";
        pinTX.out() << dec(in);
        pinTX.out() << "]" << endl;
    };

    //if (onPing.isNow()) {
    //    pinTX.out() << dec(packets) << " " << dec(rfm.getRecvCount()) << " " << dec(rfm.getUnderruns()) << " mode: " << dec(uint8_t(rfm.getMode())) << "cont: " << rfm.hasContent() << endl;
    //}

    /*
    if (packets > 0) {
        pind1.out() << "--- " << dec(packets) << " packets." << endl;
    }
    */
    //rt.delayMillis(1000);
    //pind1.out() << "** " << dec(uint8_t(rfm12.getMode())) << " ints:" << dec(rfm12.ints) << " recv:" << dec(rfm12.recvCount) << " ooks:" << dec(rfm12.ooks) << " ook_bits:" << dec(rfm12.ook_bits) << " " << dec(rfm12.ook_done) << "**" << endl;
}

volatile uint8_t send_idx = 0;
volatile uint8_t send_length = 1;

void sendFSK() {
    pinTX.out() << "sending FSK, length " << dec(send_length) << endl;
    {
        auto out = rfm.out_fsk(42);
        for (int i = 0; i < send_length; i++) {
            out << uint8_t(83);
        }
    }

    send_length++;
    if (send_length > 10) {
        send_length = 1;
    }
}

void sendOOK() {
    pinTX.out() << "sending OOK p:" << dec(rfm.getPulses()) << endl;
    uint8_t command = 18;
    FS20Packet packet (0b00011011, 0b00011011, 0b00000000, command, 0);
    rfm.out_fs20(packet);

}

int main(void) {
    //Logging::onMessage = &log;

    pinTX.out() << "Initialized." << endl;
    //sendOOK();
    bool ook = true;

    while(true) {
        //pinTX.out() << "i: " << dec(rfm.getInterrupts()) << " p: " << dec(rfm.pulses) << endl;
        //sendFSK();
        //rt.delay(400_ms);
        if (onPing.isNow()) {
            //if (ook) {
            //    sendOOK();
            //} else {
            //    sendFSK();
            //}
            ook = !ook;
        }
        //t.delay(400_ms);

        loop();
    }
}

