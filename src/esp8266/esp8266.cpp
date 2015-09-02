#include "Pin.hpp"
#include "Espressif/ESP8266.hpp"
#include <avr/io.h>
#include "RealTimer.hpp"
#include "EEPROM.hpp"
#include "Strings.hpp"
#include "Serial/PulseCounter.hpp"
#include "FS20/FS20Decoder.hpp"
#include "Visonic/VisonicDecoder.hpp"
#include "HopeRF/RFM12.hpp"

using namespace Espressif;
using namespace TimeUnits;
using namespace FS20;
using namespace Visonic;
using namespace Serial;
using namespace HopeRF;

struct EEPROM {
    char apn[32];
    char password[64];
    char remoteIP[15];
    uint16_t remotePort;
};

Timer1_Normal<ExtPrescaler::_8> tm1;
Timer2_Normal<IntPrescaler::_1024> tm2;
auto rt = realTimer(tm2);
Usart0 usart0(115200);
PinD0<Usart0,254> pinD0(usart0);
PinD1<Usart0,254> pinD1(usart0);
PinD2 pinD2;
PinD3<> pinD3;
PinD4 pinD4;
PinD7 pinD7;
PinD10<> pinD10;
PinA1 pinA1;
PinA2 pinA2;
PinA3 pinA3;
PinA4 pinA4;
PinA5 pinA5;
SPIMaster spi;

auto constexpr band = RFM12Band::_868Mhz;
auto comp = tm1.comparatorB();
auto rfm = rfm12(spi, pinD10, pinD2, tm1.comparatorA(), band);
auto counter = pulseCounter<typeof comp, typeof pinD3, 250>(comp, pinD3, (30_us).template toCounts<typeof tm1>());
auto esp = esp8266<&EEPROM::apn, &EEPROM::password, &EEPROM::remoteIP, &EEPROM::remotePort>(pinD1, pinD0, pinD7);
auto fs20 = fs20Decoder(counter);
auto visonic = visonicDecoder(counter);

auto pingInterval = periodic(rt, 5000_ms);
auto rfmWatchdog = deadline(rt, 60000_ms);

uint8_t packets = 0;

enum PacketType: uint8_t {
    TYPE_RFM12 = 'R',
    TYPE_FS20 = 'F',
    TYPE_VISONIC = 'V',
    TYPE_PING = 'P',
    TYPE_MSG = 'M'
};


int main() {
    uint8_t count = 0;
    bool blink = false;
    pinD4.configureAsOutput();
    pinA2.configureAsOutput();
    pinA3.configureAsOutput();
    pinA4.configureAsOutput();
    pinA5.configureAsOutput();

    while(true) {
        uint8_t state = static_cast<uint8_t>(esp.getState());
        pinA2.setHigh(state & 1);
        pinA3.setHigh(state & 2);
        pinA4.setHigh(state & 4);
        pinA5.setHigh(state & 8);
        counter.onMax(10, [] (Pulse pulse) {
            fs20.apply(pulse);
            visonic.apply(pulse);
        });

        esp.loop();

        FS20Packet fs20Packet;
        if (fs20.in() >> fs20Packet) {
            esp.out() << TYPE_FS20 << fs20Packet;
            rfm.reset(band); // TODO this doesn't work, but the 1min watchdog does seem to repair the RFM12...
        }

        VisonicPacket visonicPacket;
        if (visonic.in() >> visonicPacket) {
            esp.out() << TYPE_VISONIC << visonicPacket;
            rfm.reset(band);
        }

        if (rfm.hasContent()) {
            rfmWatchdog.reset();
            esp.out() << TYPE_RFM12 << rfm.in();
        }

        if (pingInterval.isNow()) {
            blink = !blink;
            pinD4.setHigh(blink);
            count++;
            esp.out() << TYPE_PING << dec(count) << " " << dec(packets);
        }

        if (rfmWatchdog.isNow()) {
            rfm.reset(band);
            rfmWatchdog.reset();
        }

        auto in = esp.in();
        if (in) {
            uint8_t type;
            if (in >> type) switch (type) {
            case TYPE_FS20: {
                FS20Packet inPacket;
                if (in >> inPacket) {
                    rfm.out_fs20(inPacket);
                    packets++;
                }
                break;
            }
            case TYPE_RFM12:
                rfm.out() << in;
                packets++;
                break;
            }
        }
    }
}
