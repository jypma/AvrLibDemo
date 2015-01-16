#include "Pin.hpp"
#include "Timer.hpp"
#include "RealTimer.hpp"

/*
Program:     654 bytes (2.0% Full)
(.text + .data + .bootloader)

Data:         16 bytes (0.8% Full)
(.data + .bss + .noinit)
 */

const Timer0<ExtPrescaler61Hz> tm0;
const RealTimer<typeof tm0, &tm0> rt;
const PinD9 pinD9;

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
