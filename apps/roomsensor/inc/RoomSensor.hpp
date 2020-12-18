#pragma once

#include "eeprom.hpp"
#include "HAL/Atmel/Device.hpp"
#include "HAL/Atmel/Power.hpp"
#include "Time/RealTimer.hpp"
#include "Passive/SupplyVoltage.hpp"
#include "HopeRF/RFM12.hpp"
#include "Dallas/DS18x20.hpp"
#include "Time/Units.hpp"
#include "DHT/DHT22.hpp"
#include "Option.hpp"
#include "ROHM/BH1750.hpp"
#include "Ambient/TSL2561.hpp"
#include "PIR/HCSR501.hpp"
#include "Tasks/Task.hpp"
#include "Tasks/loop.hpp"

#define auto_var(name, expr) decltype(expr) name = expr

volatile uint8_t hcr_ints = 0;

namespace HAL { namespace Atmel {
  uint8_t admux0, admux1, admux2;
}}

using namespace HAL::Atmel;
using namespace Time;
using namespace Streams;
using namespace Passive;
using namespace HopeRF;
using namespace Dallas;
using namespace DHT;
using namespace ROHM;
using namespace PIR;
using namespace Ambient;

struct Measurement {
	Option<int16_t> temp;
	Option<uint16_t> humidity;
	Option<uint16_t> supply;
	uint8_t seq;
	uint16_t sender;
	Option<uint32_t> lux;
	Option<uint8_t> motion;

    typedef Protobuf::Protocol<Measurement> P;

    typedef P::Message<
		P::Varint<1, uint16_t, &Measurement::sender>,
        P::Varint<8, uint8_t, &Measurement::seq>,
		P::Varint<9, Option<uint16_t>, &Measurement::supply>,
		P::Varint<10, Option<int16_t>, &Measurement::temp>,
		P::Varint<11, Option<uint16_t>, &Measurement::humidity>,
		P::Varint<12, Option<uint32_t>, &Measurement::lux>,
	    P::Varint<13, Option<uint8_t>, &Measurement::motion>
	> DefaultProtocol;
};

struct RoomSensor: public Task {
    typedef RoomSensor This;
    typedef Logging::Log<Loggers::Main> log;

    Usart0 usart0 = { 115200 };
    auto_var(pinTX, PinPD1<240>(usart0));
    auto_var(pinRX, PinPD0());

    SPIMaster spi;
    ADConverter<uint16_t> adc;
    TWI<> twi = {};

    auto_var(timer0, Timer0::withPrescaler<64>::inNormalMode());
    auto_var(timer1, Timer1::withPrescaler<1>::inNormalMode());
    auto_var(timer2, Timer2::withPrescaler<64>::inNormalMode());
    auto_var(rt, realTimer(timer0));
    auto_var(nextMeasurement, deadline(rt, 60_s));

    auto_var(pinRFM12_INT, PinPD2());
    auto_var(pinRFM12_SS, PinPB2());
  #define JEENODE_HARDWARE
#ifdef JEENODE_HARDWARE
  auto_var(pinSupply, JeeNodePort2A());
  auto_var(pinDHT, JeeNodePort2D().withInterrupt());  // confirmed
  auto_var(pinDHTPower, JeeNodePort3A());             // confirmed
  auto_var(pinPIRPower, JeeNodePort4D());             // not connected, always on
  auto_var(pinPIRData, PinPD3()); // JeeNodePortI(), confirmed
  auto_var(pinDS, JeeNodePort1D());
#else
  auto_var(pinSupply, ArduinoPinA0());
  auto_var(pinDHT, ArduinoPinD8().withInterrupt());
  auto_var(pinDHTPower, ArduinoPinD2()); // TODO DHT without hardware power switching
  auto_var(pinPIRPower, ArduinoPinD2()); // TODO PIR without hardware power switching
  auto_var(pinPIRData, ArduinoPinD9().withInterrupt());
  auto_var(pinDS, ArduinoPinD7());
#endif

    auto_var(rfm, (rfm12<4,100>(spi, pinRFM12_SS, pinRFM12_INT, timer0.comparatorA(), RFM12Band::_868Mhz)));
    auto_var(supplyVoltage, (SupplyVoltage<4700, 1000, &EEPROM::bandgapVoltage>(adc, pinSupply)));
    auto_var(power, Power(rt));
    auto_var(dht, DHT22(pinDHT, pinDHTPower, timer0.comparatorB(), rt));
  auto_var(onewire, OneWireUnpowered(pinDS, rt));
  auto_var(ds, SingleDS18x20(onewire));
    auto_var(pir, HCSR501(pinPIRData, pinPIRPower, rt));
    auto_var(tsl, tsl2561(twi, rt));

 	uint8_t seq = 0;

    typedef Delegate<This, decltype(pinTX), &This::pinTX,
            Delegate<This, decltype(rt), &This::rt,
			Delegate<This, decltype(dht), &This::dht,
            Delegate<This, decltype(rfm), &This::rfm,
			Delegate<This, decltype(twi), &This::twi,
			Delegate<This, decltype(pir), &This::pir,
            Delegate<This, decltype(power), &This::power
			>>>>>>> Handlers;

  bool measuring = false;

    void measure() {
        log::debug(F("Measuring"));
        log::flush();
        ds.measure();
        dht.measure();
        tsl.measure();
        measuring = true;
    }

    RoomSensor() {
    	pinRX.configureAsInputWithPullup();
        log::debug(F("Starting"));
        rfm.onIdleSleep();
    }

  void loop() {
    //supplyVoltage.stopOnLowBattery(3000, [&] {
    //    log::debug(F("**LOW**"));
    //    log::flush();
    //  });

    if (pir.isMotionDetected()) {
      Measurement m = {};
      log::debug(F("ints: "), dec(pir.getInts()));
      log::debug(F("Motion!"));
      m.motion = 1;
      for (int i = 0; i < 10; i++) supplyVoltage.get();
      m.supply = supplyVoltage.get();
      log::debug(F("Suppl: "), dec(m.supply));
      if (m.supply > 4700U) {
        // Don't send supply if we're on AC
        m.supply = none();
      }

      seq++;
      m.seq = seq;
      m.sender = 'Q' << 8 | read(&EEPROM::id);
      if (m.supply >= uint16_t(3300)) {
        // PIR sensor gets unreliable when dropping below 3.3V
        rfm.write_fsk(42, &m);
      }
    }

    if (rfm.isIdle()) {
      pir.enable();
    }

    if (measuring && dht.isIdle() && tsl.isIdle() && ds.isIdle()) {
      for (int i = 0; i < 100; i++) supplyVoltage.get();
      log::flush();

      Measurement m = {};
      log::debug(F("ints: "), dec(pir.getInts()));
      pinTX.flush();
      measuring = false;

      m.humidity = dht.getHumidity();
      log::debug(F("Hum  : "), dec(m.humidity));

      m.temp = dht.getTemperature();
      log::debug(F("Temp : "), dec(m.temp));
      log::flush();

      auto altTemp = ds.getTemperature();
      log::debug(F("AltTemp : "), dec(altTemp));
      log::flush();

      m.lux = tsl.getLightLevel();
      log::debug(F("Lux: "), dec(m.lux));
      log::flush();

      m.supply = supplyVoltage.get();
      //log::debug(F("ADMUX: "), dec(ADMUX.get()));
      //log::debug(F("ADCSRA: "), dec(ADCSRA.get()));
      log::debug(F("Suppl: "), dec(m.supply));
      log::flush();

      if (m.supply > 4700U) {
        // Don't send supply if we're on AC
        m.supply = none();
      }

      seq++;
      m.seq = seq;
      m.sender = 'Q' << 8 | read(&EEPROM::id);
      pir.disable();
      rfm.write_fsk(42, &m);
      nextMeasurement.schedule();
    }
  }

  int main() {
    log::debug(F("RoomSensor "), dec(read(&EEPROM::id)));
    log::flush();

    auto measureTask = nextMeasurement.invoking<This, &This::measure>(*this);
    measure();
    while(true) {
      //auto v = supplyVoltage.get();
      /*
      adc.measure(pinSupply);
      log::debug(dec(admux0));
      log::debug(dec(admux1));
      log::debug(dec(admux2));
      auto v = adc.awaitValue();
      */
      /*
      log::debug(F("ADMUX: "), dec(ADMUX.get()));
      log::debug(F("ADCSRA: "), dec(ADCSRA.get()));
      log::debug(dec(v));
      log::flush();
      */
      //rt.delay(1_s);
      loopTasks(power, measureTask, dht, tsl, ds, rfm, pir, *this);
    }

  }
};
