#pragma once

#include "eeprom.hpp"
#include "auto_field.hpp"
#include "HAL/Atmel/Device.hpp"
#include "Espressif/ESP8266.hpp"
#include "Time/RealTimer.hpp"
#include "EEPROM.hpp"
#include "Strings.hpp"
#include "Serial/PulseCounter.hpp"
#include "Serial/RS232Tx.hpp"
#include "FS20/FS20Decoder.hpp"
#include "Visonic/VisonicDecoder.hpp"
#include "HopeRF/RFM12.hpp"

using namespace HAL::Atmel;
using namespace Espressif;
using namespace Time;
using namespace FS20;
using namespace Visonic;
using namespace Serial;
using namespace HopeRF;
using namespace Streams;

struct Ping {
    EthernetMACAddress mac;
    uint8_t seq = 0;
    uint16_t packets_out = 0;
    uint16_t packets_in = 0;
    uint8_t rfmWatchdog = 0;
    uint8_t espWatchdog = 0;

    void reset() {
        seq++;
        packets_out = 0;
        packets_in = 0;
        rfmWatchdog = 0;
        espWatchdog = 0;
    }

    typedef Protobuf::Protocol<Ping> P;
    typedef P::Message<
        P::SubMessage<1, EthernetMACAddress, &Ping::mac, EthernetMACAddress::BinaryProtocol>,
        P::Varint<2, uint8_t, &Ping::seq>,
        P::Varint<3, uint16_t, &Ping::packets_out>,
        P::Varint<4, uint16_t, &Ping::packets_in>,
        P::Varint<5, uint8_t, &Ping::rfmWatchdog>,
        P::Varint<6, uint8_t, &Ping::espWatchdog>
    > DefaultProtocol;
};

struct RFRelay {
    typedef RFRelay This;
    typedef Logging::Log<Loggers::Main> log;

    SPIMaster spi;
    static auto constexpr band = RFM12Band::_868Mhz;

    Usart0 usart0 = { 115200 };
    auto_var(pinTX, PinPD1<250>(usart0));
    auto_var(pinRX, PinPD0<250>(usart0));

    auto_var(timer0, Timer0::withPrescaler<64>::inNormalMode());
    auto_var(timer1, Timer1::withPrescaler<64>::inNormalMode());
    auto_var(rt, realTimer(timer0));

    auto_var(pinLOG, PinPB1(timer1)); // Arduino D9
    auto_var(logger, (RS232Tx<9600, 240>(pinLOG)));

    auto_var(pinRFM12_INT, PinPD2());
    auto_var(pinOOK, PinPD3());
    auto_var(pinOOK_EN, PinPD4());
    auto_var(pinLED0, PinPD5());
    auto_var(pinLED1, PinPD6());
    auto_var(pinESP_PD, PinPD7());
    auto_var(pinLED2, PinPB0());
    auto_var(pinRFM12_SS, PinPB2());
    auto_var(rfm, (rfm12<128,128>(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), band)));
    auto_var(counter, pulseCounter(timer0.comparatorB(), pinOOK, 150_us));
    auto_var(esp, (esp8266<&EEPROM::apn,&EEPROM::password,&EEPROM::remoteIP,&EEPROM::remotePort>(pinTX, pinRX, pinESP_PD, rt)));
    auto_var(fs20, fs20Decoder(counter));
    auto_var(visonic, visonicDecoder(counter));
    auto_var(pingInterval, periodic(rt, 10_s));
    auto_var(rfmWatchdog, deadline(rt, 60000_ms));

    typedef Delegate<This, decltype(pinTX), &This::pinTX,
    		Delegate<This, decltype(pinRX), &This::pinRX,
            Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(rfm), &This::rfm,
            Delegate<This, decltype(logger), &This::logger,
            Delegate<This, decltype(counter), &This::counter>>>>>> Handlers;

    auto_var(blink, deadline(rt));
    bool blinkOn = false;
    uint8_t blinkIdx = 0;
    Ping ping = {};

    typedef uint8_t PacketType;
    static constexpr PacketType TYPE_RFM12 = 'R';
    static constexpr PacketType TYPE_FS20 = 'F';
	static constexpr PacketType TYPE_VISONIC = 'V';
	static constexpr PacketType TYPE_PING = 'P';
	static constexpr PacketType TYPE_MSG = 'M';
    static constexpr PacketType TYPE_PING2 = 'Q';

    void send() {
        FS20Packet fs20Packet;
        if (fs20.read(&fs20Packet)) {
            ping.packets_in++;
            esp.write(TYPE_FS20, &fs20Packet);
        }

        VisonicPacket visonicPacket;
        if (visonic.read(&visonicPacket)) {
            ping.packets_in++;
            esp.write(TYPE_VISONIC, &visonicPacket);
        }

        if (rfm.in().hasContent()) {
            ping.packets_in++;
            log::debug('i',dec(rfm.in().getSize()));
            rfmWatchdog.schedule();
            rfm.in().readStart();
            log::debug('r',dec(rfm.in().getReadAvailable()),'o',dec(esp.getSpace()));
            esp.write(TYPE_RFM12, rfm.in());
            rfm.in().readEnd();
        }

        if (pingInterval.isNow()) {
            log::debug(F("ping"));
            ping.espWatchdog = esp.getWatchdogCountAndReset();
            ping.mac = esp.getMACAddress();
            esp.write(TYPE_PING2, &ping);
            ping.reset();
        }
    }

    RFRelay() {
        log::debug(F("Boot"));
        blink.schedule(400_ms);
        pinOOK.configureAsInputWithPullup();
        pinOOK_EN.configureAsOutput();
        pinOOK_EN.setHigh();
        pinLED0.configureAsOutput();
        pinLED1.configureAsOutput();
        pinLED2.configureAsOutput();
    }

    void loop() {
        if (blink.isNow()) {
            blinkOn = !blinkOn;
            if (blinkOn) {
                blink.schedule(100_ms);
            } else {
                blink.schedule(400_ms);
                blinkIdx = blinkIdx + 1;
                if (blinkIdx > 3) {
                    blinkIdx = 0;
                }
            }
        }

        pinLED0.setHigh(esp.isConnecting() ? blinkOn : esp.isConnected());
        pinLED1.setHigh(esp.isSending() || rfm.isReceiving());
        pinLED2.setHigh(esp.isReceiving() || rfm.isSending());

        counter.onMax(10, [&] (Pulse pulse) {
            fs20.apply(pulse);
            visonic.apply(pulse);
        });
        esp.loop();

        if (esp.isConnected()) {
            send();
        }

        if (rfmWatchdog.isNow()) {
            ping.rfmWatchdog++;
            rfm.reset(band);
            rfmWatchdog.schedule();
        }

        if (esp.in().hasContent()) {
            esp.in().read(Nested([&] (auto read) -> ReadResult {
                uint8_t type;
                if (!read(&type)) {
                    log::debug('e', dec(type));
                    return ReadResult::Valid;
                }

                switch(type) {
                    case TYPE_FS20: {
                        FS20Packet inPacket;
                        if (!read(&inPacket)) {
                            log::debug('e','f');
                            return ReadResult::Valid;
                        }
                        rfm.write_fs20(inPacket);
                        ping.packets_out++;
                        break;
                    }
                    case TYPE_RFM12: {
                        uint8_t header;
                        if (!read(&header)) {
                            log::debug('e','r');
                            return ReadResult::Valid;
                        }
                        rfm.write_fsk(header, read);
                        ping.packets_out++;
                        break;
                    }
                }
                return ReadResult::Valid;
            }));
        }
    }

    int main() {
        while(true) {
        	loop();
        }
    }
};

