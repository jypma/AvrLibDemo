#include "HAL/Atmel/Device.hpp"
#include "Time/RealTimer.hpp"
#include "Mechanic/Button.hpp"
#include "HopeRF/RFM12.hpp"
#include "HAL/Atmel/Power.hpp"

using namespace HAL::Atmel;
using namespace Time;
using namespace HopeRF;
using namespace Mechanic;

SPIMaster spi;

static constexpr uint8_t ID1 = 'L';
static constexpr uint8_t ID2 = 'K';

#define DEBUG
#ifdef DEBUG
Usart0 usart0(57600);
auto pinTX = PinPD1(usart0);
#endif
auto timer0 = Timer0().withPrescaler<1024>().inNormalMode();
auto timer1 = Timer1().withPrescaler<1>().inFastPWMMode();
auto rt = realTimer(timer0);
auto pinRFM12_SS = PinPB2();
auto pinRFM12_INT = PinPD2();
auto lampPin = PinPB1(timer1);
auto buttonPin = PinPD3();
auto rfm = rfm12(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), RFM12Band::_868Mhz);

auto btn = button(rt, buttonPin);
auto anim = animator(rt);
auto power = Power<decltype(rt)>(rt);
auto autoOff = deadline(rt, 30_min);

uint16_t unhandledStatusCount = 0;

#ifdef DEBUG
auto printDebug = deadline(rt, 1_s);
#endif

#ifdef DEBUG
mkISRS(timer0, timer1, rt, rfm, usart0, pinTX, btn);
#else
mkISRS(timer0, timer1, rt, rfm);
#endif

enum class Mode: uint8_t { OFF, DIM, ON };
Mode mode = Mode::OFF;

void send(uint8_t q) {
    uint8_t rgb = (q > 0) ? 255 : 0;
    rfm.out_fsk(31) << "foo" << ID1 << ID2 << ' ' << ' ' << uint8_t(6) << rgb << rgb << rgb << q;
}

void turn(Mode m) {
    mode = m;
    switch(mode) {
    case Mode::OFF:
        anim.to(0, 2000_ms, AnimatorInterpolation::EASE_UP);
        send(0);
        break;
    case Mode::ON:
        anim.to(0xFFFF, 1000_ms, AnimatorInterpolation::EASE_UP);
        send(255);
        break;
    case Mode::DIM:
        anim.to(2000, 1000_ms, AnimatorInterpolation::EASE_UP);
        send(4);
        break;
    }
}

int main() {
    uint16_t wakeups = 0;
#ifdef DEBUG
    pinTX.out() << "1 second is " << dec(uint32_t(toCountsOn(rt, 1000_ms))) << endl;
#endif
    lampPin.configureAsOutput();
    lampPin.setLow();
    lampPin.timerComparator().setOutput(FastPWMOutputMode::connected);
    lampPin.timerComparator().setTargetFromNextRun(0);
    while (true) {
#ifdef DEBUG
        if (printDebug.isNow()) {
            pinTX.out() << "i: " << dec(rfm.getInterrupts()) << " r: " << dec(rfm.recvCount) << " u: " << dec(rfm.getUnderruns()) << " m: " << dec(uint8_t(rfm.getMode())) << "im: " << dec(uint8_t(rfm.getMode())) << "s:" << dec(rfm.intStatus) << endl;
            pinTX.out() << "wakeups: " << dec(wakeups) << endl;
            printDebug.reset();
        }
#endif
        if (rfm.unhandledStatusCount > unhandledStatusCount) {
            pinTX.out() << "** unhandled status: " << dec(rfm.unhandledStatus[unhandledStatusCount]) << endl;
            unhandledStatusCount++;
        }

        if (autoOff.isNow() && mode != Mode::OFF) {
#ifdef DEBUG
            pinTX.out() << "Auto turning off." << endl;
#endif
            turn(Mode::OFF);
        }

        const auto animEvt = anim.nextEvent();
        if (animEvt.isChanged()) {
            lampPin.timerComparator().setTargetFromNextRun(animEvt.getValue());
        }

        const auto btnEvt = btn.nextEvent();
        if (btnEvt == ButtonEvent::PRESSED) {
#ifdef DEBUG
            pinTX.out() << "<-- PRESSED" << endl;
#endif
            autoOff.reset();

            switch(mode) {
            case Mode::ON: turn(Mode::OFF); break;
            case Mode::OFF: turn(Mode::DIM); break;
            case Mode::DIM: turn(Mode::ON); break;
            }
        }
        else if (btnEvt == ButtonEvent::RELEASED) {
#ifdef DEBUG
            pinTX.out() << "--> RELEASED" << endl;
#endif
        }

        if (rfm.hasContent()) {
            uint8_t header, sender0, sender1, receiver0, receiver1, r, g, b, q;
            if (rfm.in() >> header >> sender0 >> sender1 >> receiver0 >> receiver1 >> r >> g >> b >> q) {
                if (receiver0 == ID1 && receiver1 == ID2) {
#ifdef DEBUG
                    pinTX.out() << "Received brightness: " << dec(q) << endl;
#endif
                    autoOff.reset();

                    if (q > 126 && mode != Mode::ON) {
                        turn(Mode::ON);
                    } else if (q <= 126 && q > 0 && mode != Mode::DIM) {
                        turn(Mode::DIM);
                    } else if (q == 0 && mode != Mode::OFF) {
                        turn(Mode::OFF);
                    }
                } else {
#ifdef DEBUG
                    pinTX.out() << "Ignoring message for different lamp." << dec(q) << endl;
#endif
                }
            } else {
#ifdef DEBUG
                pinTX.out() << "Ignoring packet." << dec(q) << endl;
#endif
                rfm.in(); // ignore packet
            }
        }

        if (animEvt.isIdle()
                && !rfm.hasContent()
                && rfm.getMode() == RFM12Mode::LISTENING
                && (btnEvt == ButtonEvent::RELEASED || btnEvt == ButtonEvent::UP)) {

            auto target = (animEvt.getValue() == 0 || animEvt.getValue() == 0xFFFF) ? SleepMode::STANDBY : SleepMode::IDLE;
            auto gran = (mode == Mode::OFF) ? SleepGranularity::_8000ms : SleepGranularity::_500ms;
#ifdef DEBUG
//            auto left = autoOff.timeLeft();
//            auto millis = left.toMillisOn<decltype(rt)>().getValue();
//            pinTX.out() << "powering down into mode " << dec(uint8_t(mode)) << " for " << dec(left.getValue()) << "t / " << dec(millis) << "ms" << endl;
            pinTX.flush();
#endif
            //power.sleep(mode);
            power.sleepUntil(autoOff, target, gran); // auto-select mode based on enabled & connected timers
//#ifdef DEBUG
//            pinTX.out() << "awake! " << dec(uint8_t(rfm.getMode())) << endl;
//#endif
            wakeups++;
        }

    }
}
