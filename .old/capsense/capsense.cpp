#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "Mechanic/TouchSensor.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace Streams;
using namespace Mechanic;

Usart0 usart0(57600);
auto pinTX = PinPD1<128>(usart0);
LOGGING_TO(pinTX)
typedef Logging::Log<Loggers::Main> l;

auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto rt = realTimer(timer0);
auto measureInterval = periodic(rt, 200_ms);

auto pinSense = JeeNodePort2D();
auto touch = TouchSensor(pinSense);

mkISRS(usart0, pinTX, rt);


int main() {
    l::debug(F("Starting"));
    while(true) {
        if (measureInterval.isNow()) {
            uint8_t m = touch.measure();
            l::debug(F("n: "), dec(m));
        }
    }
}
