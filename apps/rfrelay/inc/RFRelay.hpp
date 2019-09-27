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
  PinPD1_t<decltype(usart0), 250> pinTX;
  PinPD0_t<decltype(usart0), 250> pinRX;

  Timer0::withPrescaler<64>::Normal timer0;
  Timer1::withPrescaler<64>::Normal timer1;
  RealTimer<decltype(timer0)> rt = { timer0 };

  PinPB1_t<decltype(timer1)> pinLOG = { timer1 }; // Arduino D9
  ::Serial::Impl::RS232Tx<decltype(pinLOG), 9600, 240> logger = { pinLOG };

    auto_var(pinRFM12_INT, PinPD2());
    auto_var(pinOOK, PinPD3());
    auto_var(pinOOK_EN, PinPD4());
    auto_var(pinLED0, PinPD5());
    auto_var(pinLED1, PinPD6());
    auto_var(pinESP_PD, PinPD7());
    auto_var(pinLED2, PinPB0());
    auto_var(pinRFM12_SS, PinPB2());
  RFM12<decltype(spi), decltype(pinRFM12_SS), decltype(pinRFM12_INT), decltype(timer0)::comparatorA_t, true, 100, 100> rfm = {
    spi, pinRFM12_SS, pinRFM12_INT, &timer0.comparatorA(), RFM12Band::_868Mhz };

  MinPulseCounter<decltype(timer0)::comparatorB_t, decltype(pinOOK)> counter = { timer0.comparatorB(), pinOOK, 150_us };
  ESP8266<&EEPROM::apn, &EEPROM::password, &EEPROM::remoteIP, &EEPROM::remotePort,
          decltype(pinTX), decltype(pinRX), decltype(pinESP_PD), decltype(rt)> esp = { pinTX, pinRX, pinESP_PD, rt };
  FS20Decoder<decltype(counter)> fs20 = { };
  VisonicDecoder<decltype(counter)> visonic = { };
  Periodic<decltype(rt), decltype(10_s)> pingInterval = { rt };
  Deadline<decltype(rt), decltype(60_s)> rfmWatchdog = { rt };

    typedef Delegate<This, decltype(pinTX), &This::pinTX,
    		Delegate<This, decltype(pinRX), &This::pinRX,
            Delegate<This, decltype(rt), &This::rt,
            Delegate<This, decltype(rfm), &This::rfm,
            Delegate<This, decltype(logger), &This::logger,
            Delegate<This, decltype(counter), &This::counter>>>>>> Handlers;

  VariableDeadline<decltype(rt)> blink = { rt };
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
            log::debug(F("if"));
            esp.write(TYPE_FS20, &fs20Packet);
        }

        VisonicPacket visonicPacket;
        if (visonic.read(&visonicPacket)) {
            ping.packets_in++;
            log::debug(F("iv"));
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
                        log::debug(F("of"));
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
                        log::debug(F("or"));
                        rfm.write_fsk(header, read);
                        ping.packets_out++;
                        break;
                    }
                }
                return ReadResult::Valid;
            }));
        }
    }

    void main() {
      while(true) {
        loop();
      }
    }
};

