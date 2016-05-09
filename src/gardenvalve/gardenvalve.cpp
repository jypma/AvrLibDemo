#include "HAL/Atmel/Device.hpp"
#include "HAL/Atmel/Power.hpp"
#include "Time/RealTimer.hpp"
#include "HopeRF/RFM12.hpp"
#include "Passive/SupplyVoltage.hpp"

struct EEPROM {
    uint16_t bandgapVoltage;
};


using namespace HAL::Atmel;
using namespace Time;
using namespace Streams;
using namespace HopeRF;
using namespace Passive;

Usart0 usart0(57600);
auto pinTX = PinPD1<128>(usart0);
LOGGING_TO(pinTX)
typedef Logging::Log<Loggers::Main> log;

auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto timer2 = Timer2().withPrescaler<8>().inNormalMode();
auto rt = realTimer(timer0);
typedef decltype(rt) rt_t;
auto autoOff = deadline(rt, 2_min);
auto resendPing = deadline(rt, 1_s);
auto deepsleep = deadline(rt, 120_min);
auto adc = ADConverter();

auto pinRFM12_INT = PinPD2();
auto pinRFM12_SS = PinPB2();
auto pinValve = JeeNodePort1D();
auto pinSupply = JeeNodePort1A();

SPIMaster spi;
auto supply = SupplyVoltage<100, 10, &EEPROM::bandgapVoltage>(adc, pinSupply);
auto power = Power(rt);
auto rfm = rfm12(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), RFM12Band::_868Mhz);

mkISRS(usart0, pinTX, rt, rfm, power);

bool on = false;
uint16_t currentSupply = 0;
uint8_t sendAttempts = 10;

void measureSupply() {
    currentSupply = supply.get();
//    log::debug(F("Supply is "), dec(currentSupply), F("mV"));
}

void sendState() {
    measureSupply();
    rfm.write_fsk(31, 'V', '1', Padding(2),
            uint8_t(1),          // state
            uint8_t(on ? 1 : 0), // currently on valve (0 = off)
            currentSupply);
}

void sendPing() {
    measureSupply();
    rfm.onIdleListen();
    log::debug(F("Ping..."));
    rfm.write_fsk(31, 'V', '1', Padding(2),
            uint8_t(0),  // ping
            uint8_t(1),  // number of valves
            currentSupply);
    resendPing.schedule();
}

void turnOff() {
    log::debug(F("Turning off"));
    on = false;
    pinValve.setLow();
    autoOff.cancel();
    rfm.onIdleSleep();
    deepsleep.schedule();
    sendState();
}

void turnOn() {
    log::debug(F("Turning on"));
    on = true;
    pinValve.setHigh();
    autoOff.schedule();
    sendState();
}

int main() {
    log::debug(F("Starting"));
    measureSupply();
    measureSupply();
    measureSupply();
    measureSupply();
    measureSupply();
    autoOff.cancel();
    deepsleep.cancel();
    pinValve.configureAsOutput();
    pinValve.setLow();
    sendPing();
    rfm.onIdleListen();
    while(true) {
        uint8_t state = 255;
        while (rfm.hasContent()) {
            rfm.readStart();
            if (rfm.read(
                Padding(3),  // header + sender
                'V', '1',    // recipient. TODO make this in EEPROM
                FB(2),       // expected state
                &state
            )) {
            }
            rfm.readEnd();
        }
        if (state == 0 || state == 1) {
            resendPing.cancel();
            if (state == 0) {
                turnOff();
            } else {
                turnOn();
            }
        }

        if (resendPing.isNow()) {
            sendAttempts--;
            if (sendAttempts >= 0) {
                sendPing();
            } else {
                turnOff();
            }
        } else if (autoOff.isNow()) {
            log::debug(F("Auto-turn off kicking in"));
            turnOff();
        } else if (deepsleep.isNow()) {
            sendAttempts = 10;
            sendPing();
        } else if (rfm.isIdle()) {
            auto time1 = toMillisOn<rt_t>(autoOff.timeLeft()).getValue();
            auto time2 = toMillisOn<rt_t>(resendPing.timeLeft()).getValue();
            auto time3 = toMillisOn<rt_t>(deepsleep.timeLeft()).getValue();
            log::debug(F("Times:"), dec(time1), ' ', dec(time2), ' ', dec(time3));
            pinTX.flush();
            auto mode = (rfm.isSleeping() ? SleepMode::POWER_DOWN : SleepMode::STANDBY);
            power.sleepUntilAny(mode, autoOff, resendPing, deepsleep);
        }
    }
}
