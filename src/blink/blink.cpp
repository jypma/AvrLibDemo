#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"

/*
Program:     654 bytes (2.0% Full)
(.text + .data + .bootloader)

Data:         16 bytes (0.8% Full)
(.data + .bss + .noinit)
 */
using namespace HAL::Atmel;
using namespace Time;

auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto rt = realTimer(timer0);
auto LED = PinPD4(); // D4
auto LED2 = PinPD5(); // D4
Usart0 usart0(57600);
auto pinTX = PinPD1(usart0);

mkISRS(rt, pinTX);

void loop() {
    //LED.setHigh();

    sei();
    pinTX.write(F("here-is-a-string-thats-longer-than-15\n"));

    rt.delay(1000_ms);

    //LED.setLow();

    pinTX.write(F("here-is-another-string-thats-longer-than-15"));

    rt.delay(1000_ms);
}

int main(void) {
    LED.configureAsOutput();
    LED2.configureAsOutput();
    LED.setLow();
    LED2.setLow();

    while(true) {
       loop();
    }
}
