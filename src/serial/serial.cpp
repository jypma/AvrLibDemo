#include "Pin.hpp"
#include "Timer.hpp"
#include "Usart.hpp"
#include "RealTimer.hpp"

const Timer0<ExtPrescaler61Hz> tm0;
const RealTimer<typeof tm0, &tm0> rt;
const PinD1<> pind1;

void loop() {
    pind1 << "ho" << endl;

    rt.delayMillis(1000);
 }

int main(void) {
  pind1 << "hahaha" << endl;

  while(true) {
    loop();
  }
}

