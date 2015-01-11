#include "Pin.hpp"
#include "RealTimer.hpp"

/*
 Program:    1622 bytes (4.9% Full)
(.text + .data + .bootloader)

Data:         50 bytes (2.4% Full)
(.data + .bss + .noinit)
 */
RealTimer<IntPrescaler> rt(timer2, TimerMode::fastPWM, IntPrescaler61Hz);

void loop() {
    pinD9.setHigh();

    rt.delayMillis(1000);

    pinD9.setLow();

    rt.delayMillis(1000);
}

int main(void) {
    pinD9.configureAsOutput();

    while(true) {
       loop();
    }
}
