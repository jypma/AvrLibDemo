#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "Dallas/DS18x20.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Streams;
using namespace Dallas;

Usart0 usart0(57600);
auto pinTX = PinPD1<128>(usart0);
LOGGING_TO(pinTX)
typedef Logging::Log<Loggers::Main> log;

auto timer0 = Timer0().withPrescaler<256>().inNormalMode();
auto timer2 = Timer2().withPrescaler<1024>().inNormalMode();
auto rt = realTimer(timer0);

auto pinDS = JeeNodePort2D();
auto wire = OneWireParasitePower(pinDS, rt);
auto ds = SingleDS18x20(wire);
auto measure = periodic(rt, 2_s);

mkISRS(usart0, pinTX, rt, ds);

int main() {
    log::debug(F("Starting"));
    while(true) {
        if (measure.isNow()) {
            ds.measure();
        } else if (ds.isMeasureDone()) {
            auto temp = ds.getTemperature();
            log::debug(F("Temp: "), dec(temp));
        }
    }
}
