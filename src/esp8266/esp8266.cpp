#include "HAL/Atmel/Device.hpp"
#include "Espressif/ESP8266.hpp"
#include "Time/RealTimer.hpp"
#include "EEPROM.hpp"
#include "Strings.hpp"
#include "Serial/PulseCounter.hpp"
#include "FS20/FS20Decoder.hpp"
#include "Visonic/VisonicDecoder.hpp"
#include "HopeRF/RFM12.hpp"
#include "HAL/Atmel/InterruptVectors.hpp"

#define RFM

using namespace HAL::Atmel;
using namespace Espressif;
using namespace Time;
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

auto timer0 = Timer0().withPrescaler<64>().inNormalMode();
auto rt = realTimer(timer0);
Usart0 usart0(115200);
auto pinRX = PinPD0(usart0);
auto pinTX = PinPD1(usart0);
auto pinRFM12_INT = PinPD2();
auto pinOOK = PinPD3();
auto pinOOK_EN = PinPD4();
auto pinLED0 = PinPD5();
auto pinLED1 = PinPD6();
auto pinESP_PD = PinPD7();
auto pinLED2 = PinPB0();
auto pinLED3 = PinPB1();
auto pinRFM12_SS = PinPB2();

SPIMaster spi;

auto constexpr band = RFM12Band::_868Mhz;
auto comp = timer0.comparatorB();
#ifdef RFM
auto rfm = rfm12(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), band);
#endif
auto counter = pulseCounter(comp, pinOOK, 150_us);
auto esp = esp8266<&EEPROM::apn, &EEPROM::password, &EEPROM::remoteIP, &EEPROM::remotePort>(pinTX, pinRX, pinESP_PD, rt);
auto fs20 = fs20Decoder(counter);
auto visonic = visonicDecoder(counter);

auto pingInterval = periodic(rt, 30_s);
#ifdef RFM
auto rfmWatchdog = deadline(rt, 60000_ms);
uint8_t rfmWatchdogCount = 0;
#endif

mkISRS(usart0, timer0, pinTX, pinRX, pinOOK, counter, rt, esp, rfm);

uint8_t packets = 0;

enum PacketType: uint8_t {
    TYPE_RFM12 = 'R',
    TYPE_FS20 = 'F',
    TYPE_VISONIC = 'V',
    TYPE_PING = 'P',
    TYPE_MSG = 'M'
};

void send() {
    static uint8_t count = 0;

    FS20Packet fs20Packet;
    if (fs20.in() >> fs20Packet) {
        esp.out() << TYPE_FS20 << fs20Packet;
    }

    VisonicPacket visonicPacket;
    if (visonic.in() >> visonicPacket) {
        esp.out() << TYPE_VISONIC << visonicPacket;
    }
#ifdef RFM
    if (rfm.hasContent()) {
        rfmWatchdog.reset();
        esp.out() << TYPE_RFM12 << rfm.in();
    }
#endif
    if (pingInterval.isNow()) {
        count++;
        // TODO include here: OOK overflows, ESP watchdog fires, NODE ID (4 bytes)
        // TODO describe protocol in a struct, properly.
        esp.out() << TYPE_PING << esp.getMACAddress() << "," << dec(count) << "," << dec(packets) << "," << dec(esp.getWatchdogCount()) << "," << dec(rfmWatchdogCount);
    }
}

int main() {
    auto blink = deadline(rt);
    blink.reset(400_ms);
    bool blinkOn = false;
    uint8_t blinkIdx = 0;
    pinOOK.configureAsInputWithPullup();
    pinOOK_EN.configureAsOutput();
    pinOOK_EN.setHigh();
    pinLED0.configureAsOutput();
    pinLED1.configureAsOutput();
    pinLED2.configureAsOutput();
    pinLED3.configureAsOutput();

    while(true) {

        if (blink.isNow()) {
            blinkOn = !blinkOn;
            if (blinkOn) {
                blink.reset(100_ms);
            } else {
                blink.reset(400_ms);
                blinkIdx = blinkIdx + 1;
                if (blinkIdx > 3) {
                    blinkIdx = 0;
                }
            }
        }

        uint8_t state = static_cast<uint8_t>(esp.getState());
        if (blinkOn) {
            state ^= (1 << blinkIdx);
        }
        pinLED0.setHigh(state & 1);
        pinLED1.setHigh(state & 2);
        pinLED2.setHigh(state & 4);
        pinLED3.setHigh(state & 8);
        counter.onMax(10, [] (Pulse pulse) {
            fs20.apply(pulse);
            visonic.apply(pulse);
        });

        esp.loop();

        if (esp.isMACAddressKnown()) {
            send();
        }

#ifdef RFM
        if (rfmWatchdog.isNow()) {
            rfmWatchdogCount++;
            rfm.reset(band);
            rfmWatchdog.reset();
        }
#endif
        auto in = esp.in();
        if (in) {
            uint8_t type;
            if (in >> type) switch (type) {
            case TYPE_FS20: {
                FS20Packet inPacket;
                if (in >> inPacket) {
#ifdef RFM
                    rfm.out_fs20(inPacket);
#endif
                    packets++;
                }
                break;
            }
            case TYPE_RFM12:
#ifdef RFM
                if (in.getReadAvailable() >= 1) {
                    uint8_t header;
                    in.uncheckedRead(header);
                    rfm.out_fsk(header) << in;
                }
#endif
                packets++;
                break;
            }
        }
    }
}
