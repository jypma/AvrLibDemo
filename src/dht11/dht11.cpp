#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "DHT/DHT22.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace DHT;

Usart0 usart0(57600);
auto pinTX = PinPD1<128>(usart0);
LOGGING_TO(pinTX)

auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto timer1 = Timer1().withPrescaler<1>().inNormalMode();
auto timer2 = Timer2().withPrescaler<8>().inNormalMode();
auto rt = realTimer(timer0);
auto dhtDataPin = PinPC5::withInterrupt();
auto dhtPowerPin = PinPD4();
auto dht = DHT22(dhtDataPin, dhtPowerPin, timer2.comparatorA(), rt);
auto measure = periodic(rt, 5_s);

mkISRS(rt, timer0, timer2, pinTX, dhtDataPin, dht);

int main(void) {
    bool measuring = true;
    pinTX.write(F("20us = "), dec(uint16_t(toCountsOn(timer2, 20_us))), endl);
    while(true) {
        dht.loop();
        pinTX.flush();

        if (measuring && dht.isIdle()) {
            measuring = false;
            pinTX.write(F("Temp is "), dec(dht.getTemperature()), F(", Humidity is "), dec(dht.getHumidity()), F(" err="), dec(dht.getLastFailure()), endl);
            dht.powerOff();
        }

        if (measure.isNow()) {
            pinTX.write(F("Measuring..."), endl);
            dht.measure();
            measuring = true;
            pinTX.write(dec(debugTimingsCount), ':', Streams::Decimal(debugTimings, 0, debugTimingsCount), endl);
            Logging::printTimings();
        }
    }
}
