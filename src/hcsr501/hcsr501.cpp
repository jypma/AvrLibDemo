#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "PIR/HCSR501.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace PIR;
using namespace Streams;

Usart0 usart0(57600);
auto pinTX = PinPD1<128>(usart0);
LOGGING_TO(pinTX)

auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto timer1 = Timer1().withPrescaler<1>().inNormalMode();
auto timer2 = Timer2().withPrescaler<8>().inNormalMode();
auto rt = realTimer(timer0);
auto pirPowerPin = PinPD5();
auto pirDataPin = PinPD6().withInterrupt();
auto pir = HCSR501(pirDataPin, pirPowerPin, rt, 5_s);
auto state = pir.getState();

mkISRS(rt, timer0, timer2, pinTX, pirDataPin, pir);

int main(void) {
    pinTX.write(F("Booting."), endl);
    while(true) {
        pir.loop();
        pinTX.flush();

        if (pir.isMotionDetected()) {
            pinTX.write(dec(uint32_t(rt.millis())), F(": motion detected!"), endl);
        }
        auto s = pir.getState();
        if (s != state) {
            state = s;
            pinTX.write(F("state="), dec(uint8_t(s)), endl);
        }
    }
}
