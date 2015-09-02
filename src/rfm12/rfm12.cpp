#include "HopeRF/RFM12.hpp"
#include "Timer.hpp"
#include "RealTimer.hpp"
#include "SPI.hpp"
#include "ChunkedFifo.hpp"

using namespace HopeRF;
using namespace FS20;

Timer0_Normal<ExtPrescaler::_256> tm0;
auto rt = realTimer(tm0);
auto comp = tm0.comparatorA();
Usart0 usart0(115200);
PinD1<Usart0> pind1(usart0);
PinD10<> pinD10;
PinD9<> pinD9;
PinD8 pinD8;
PinD7 pinD7;
PinD2 pinD2;
SPIMaster spi;
RFM12<typeof spi,typeof pinD10, typeof pinD2, typeof comp, true,254,32> rfm(spi, pinD10, pinD2, comp, RFM12Band::_868Mhz);

uint32_t loops = 0;

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


    uint8_t packets = 0;
    while (rfm.hasContent()) {
        packets++;
        auto in = rfm.in();
        uint8_t length = in.getReadAvailable();
        pind1.out() << dec(length) << " bytes, ";
        for (uint8_t i = 0; i < length; i++) {
            uint8_t value;
            in >> value;
            pind1.out() << " " << dec(value);
        }
        pind1.out() << endl;
    }

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
    pind1.out() << "sending FSK" << endl;
    {
        auto out = rfm.out();
        for (int i = 0; i < 10; i++) {
            out << uint8_t(83);
        }
    }

    send_length++;
    if (send_length > 10) {
        send_length = 1;
    }
    //rt.delayMillis(1000);

}

void sendOOK() {
    pind1.out() << "sending OOK" << endl;
    uint8_t command = 18;
    FS20Packet packet (0b00011011, 0b00011011, 0b00000000, command, 0);
    rfm.out_fs20(packet);

}

int main(void) {
    pind1.out() << "Initialized." << endl;

    while(true) {
        //sendFSK();
        //rt.delayMillis(400);
        //sendOOK();
        //rt.delayMillis(300);

        loop();
    }
}

