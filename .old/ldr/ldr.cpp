#include "HAL/Atmel/Device.hpp"
#include "HAL/Atmel/Power.hpp"
#include "Time/RealTimer.hpp"
#include "Passive/LogResistor.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Streams;
using namespace Passive;

Usart0 usart0(57600);
auto pinTX = PinPD1<128>(usart0);
LOGGING_TO(pinTX)
typedef Logging::Log<Loggers::Main> log;

auto timer0 = Timer0().withPrescaler<8>().inNormalMode();
auto timer2 = Timer2().withPrescaler<8>().inNormalMode();
auto rt = realTimer(timer0);
typedef decltype(rt) rt_t;
auto nextMeasurement = periodic(rt, 200_ms);

auto pinOut = JeeNodePort2D();
auto pinIn = JeeNodePort2A().withInterrupt();

auto soil = LogResistor<2006, 500, 10000000>(rt, pinOut, pinIn);

mkISRS(usart0, pinTX, rt, pinIn, soil);

int main() {
    bool measuring = false;
    log::debug(F("Starting"));
    log::debug(F("min: "), dec(soil.counts_min));
    log::debug(F("max: "), dec(soil.counts_max));
    while(true) {
        if (measuring && !soil.isMeasuring()) {
            measuring = false;
            log::debug(F("Time : "), dec(soil.getTime()));
            log::debug(F("Value: "), dec(soil.getValue()));
        } else if (nextMeasurement.isNow()) {
            soil.measure();
            measuring = true;
        }
    }
}
