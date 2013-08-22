
 // Must be write before call delay.h
 #include <avr/io.h>
 #include <util/delay.h>
 #include <avr/interrupt.h>
 
 #define RF PB3 // RFS-250 (input) at PB3
 #define RF_PCINT PCINT3 // Pin change interrupt
 #define RPI PB4 // Raspberry PI (output) at PB4
 
 #define TC0_PRESCALER 8 // timer prescaler
 #define F_TC0 (F_CPU/TC0_PRESCALER)

 #define US(x) ((uint32_t)((unsigned long long)x*F_TC0/1000000))
 #define MS(x) ((uint32_t)((unsigned long long)x*F_TC0/1000))

 #define OUTDELAY 50 // output delay in microseconds
 #define OUTDELAY_TICKS US(OUTDELAY)
 
 #define TOLERANCE_MIN 0.80
 #define TOLERANCE_MAX 1.2
 #define URC_RF_HEADER1 5000
 #define URC_RF_HEADER2 500
 #define URC_RF_PULSE 250
 #define URC_RF_ZERO 250
 #define URC_RF_ONE 500
 #define URC_RF_TRAIL 9500
 #define URC_RF_GAP_TICKS US(200000)
 
 
 #define STATE_IDLE 0
 #define STATE_HEADER 1
 #define STATE_BYPASS 20
 #define STATE_PASSTHROUGH 21
 #define STATE_INVALID 100
 
 #define MODE_SPACE 0
 #define MODE_PULSE 1
 
 #define LEN_IN_RANGE(x) (out_last_length>=(uint32_t)(US(x)*TOLERANCE_MIN) && out_last_length<=(uint32_t)(US(x)*TOLERANCE_MAX))

 volatile uint32_t tc_counter;
 
 #define RF_ACTIVE 1
 #define RF_ACTIVE_WAITING 2
 
 //register uint8_t rf_state asm ("r2");
 volatile uint8_t rf_state;

 #define URC_ID 6
 #define URC_CHANNEL_MASK 0b1111111
 
 ISR(TIM0_COMPA_vect) {
  // If we get here, RF_ACTIVE is not set and RF_ACTIVE_WAITING is set.
  rf_state ^= RF_ACTIVE | RF_ACTIVE_WAITING; // Invert them.
  TIMSK0 &= ~_BV(OCIE0A); // One-shot interrupt
 }

 ISR(TIM0_COMPB_vect) {
  // This states the end of the pulse.
  rf_state = 0; // reset all flags, ever.
  TIMSK0 &= ~(_BV(OCIE0A) | _BV(OCIE0B)); // And disable further interrupts.
 }
 
 
 ISR(TIM0_OVF_vect) {
 
  tc_counter++;
 }

 ISR(PCINT0_vect) {
 // We accept interrupts only on pin RF 
 // And we monitor activity only, level is irrelevant. 
  
  if (!rf_state) {
   OCR0A = TCNT0 + OUTDELAY_TICKS;
   OCR0B = TCNT0 + OUTDELAY_TICKS;
   TIFR0 = _BV(OCF0A) | _BV(OCF0B);
   TIMSK0 |= _BV(OCIE0A) | _BV(OCIE0B);
   rf_state |= RF_ACTIVE_WAITING;
  }
  else {
   OCR0B = TCNT0 + OUTDELAY_TICKS;
   TIFR0 = _BV(OCF0B);
  }
 }

static uint32_t now(void) {
 
 uint32_t time;
 
// uint8_t tmp_sreg;
 
// tmp_sreg = SREG;
 cli();
 time = TCNT0;
 if (time == 0 && bit_is_set(TIFR0, TOV0)) // advance the timer as it just has overflowed
  {
  time += 256;
  }
 time += (tc_counter<<8);
 
 sei();
 
 return time;
 }
 

static void printdebug(uint32_t debug) {
  uint8_t i;
  
  cli();
  _delay_us(3000);
  for (i=0; i<32; i++) {
   PORTB ^= _BV(RPI);
   _delay_us(500);
   if (debug&(1UL<<31)) {
    _delay_us(500);
   }
   debug <<= 1; 
  }
  PORTB ^= _BV(RPI);
  _delay_us(2000);
  PORTB |= _BV(RPI);
  _delay_us(2000);
  sei();
 }
 
 
 int main(void) {

 uint8_t next_mode = MODE_SPACE;
 uint8_t last_mode = MODE_SPACE;
 uint8_t state = STATE_IDLE;
 uint32_t current_time;
 uint32_t out_change_timestamp=0;
 uint32_t out_last_length=0;
 uint8_t header_counter=0;
 uint16_t urc_address;
 uint16_t urc_address_tmp=0;
 
 DDRB &= ~(1 << RF); // Set input direction on RF
 DDRB |= (1 << RPI); // Set output direction on RPI
 PORTB |= (1 << RPI) | (1 << RF) | (1 << PB0) | (1 << PB1) | (1 << PB2); // Output is high, input is pullup
 
 
 TCCR0A = 0b00000000; // normal operation mode
 TCCR0B = 0b00000010; // prescaler is f/8
 TIMSK0 |= _BV(TOIE0); // enable interrupt on overflow
 
 PCMSK |= _BV(RF_PCINT); // enable interrupt on pin change
 GIMSK |= _BV(PCIE); // enable pin change interrupts
 
 sei();
 
 for (;;) // Endless loop
 {

#ifdef DEBUG
  // debug if broken
  if (state == STATE_INVALID) {
   printdebug(header_counter);
   printdebug(urc_address_tmp);
   printdebug(out_last_length);
   state = STATE_BYPASS;
  }
#endif /* DEBUG */

//  for ( uint32_t wake_time=now()+US(3000); (int16_t)(wake_time-now())>=0; ) {
//  }
//  PORTB ^= _BV(RPI);
 
  while (next_mode == last_mode) {
   next_mode = ( (volatile uint8_t)rf_state & RF_ACTIVE ) ? MODE_PULSE : MODE_SPACE;
  }
//  PORTB ^= _BV(RPI);
//  last_mode = next_mode;
//  continue;
  
  // just switched!
  current_time = now();
  out_last_length = current_time - out_change_timestamp;
  out_change_timestamp = current_time;

  //printdebug(current_time);
  

  // if the pulse came after long space...
  if ((last_mode == MODE_SPACE) && (out_last_length > URC_RF_GAP_TICKS)) {
   state = STATE_IDLE;
  }

 switch (state) {
  case STATE_IDLE:
   if (last_mode == MODE_PULSE && LEN_IN_RANGE(URC_RF_HEADER1)) {
	 state = STATE_HEADER; // just received 1st pulse in the header
	 header_counter = 0;
	 urc_address = 0;
	}
   break;
  case STATE_HEADER:
   // next bit just came in. 
   header_counter++;
   // Now, the counter is odd after space and even after pulse.
   // It is the number of the current bit, starting at 0

   // if after pulse and odd counter, or after space and even counter - die.
   if ((last_mode == MODE_SPACE && !(header_counter & 1)) || (last_mode == MODE_PULSE && (header_counter & 1 ))) {
	urc_address_tmp = 0x5555; // magic number
    state = STATE_INVALID;
    break;
   }
  if (header_counter < 4) { // 1, 2, 3
    if (!LEN_IN_RANGE(URC_RF_HEADER2)) {
     state = STATE_INVALID;
    }
   }
   else { // 4..70
	if (header_counter == 4 || header_counter == 26 || header_counter == 48) {
	 urc_address_tmp = 1;
	}
    if (!(header_counter & 1)) { // after pulse
     if (!LEN_IN_RANGE(URC_RF_PULSE)) {
      state = STATE_INVALID;
	 }
	}
    else { // after space
     urc_address_tmp <<= 1;
     if (LEN_IN_RANGE(URC_RF_ZERO)) {
	  // bit is 0
	 }
	 else if (LEN_IN_RANGE(URC_RF_ONE)) {
	  // bit is 1
	  urc_address_tmp |= 1;
	 }
	 else {
	  // invalid bit
	  state = STATE_INVALID;
	 }
	}
	if (urc_address_tmp & (1<<11)) {
	 // save the address
	 urc_address = urc_address_tmp;
	}
    if (header_counter == 70) { // Magic number of pulses and spaces in the header
	 // Analyze URC address and set state accordingly
#if URC_ID!=0
	 if ((urc_address & 0xF) == URC_ID && (urc_address & (URC_CHANNEL_MASK << 4))) {
	  state = STATE_PASSTHROUGH;
	 }
	 else {
	  state = STATE_BYPASS;
	 }
#else /* URC_ID!=0 */
	 state = STATE_PASSTHROUGH;
#endif /* URC_ID!=0 */
    }
   }
   break;
  case STATE_PASSTHROUGH:
   switch (next_mode) {
    case MODE_SPACE: 
	 PORTB |= _BV(RPI); // Set RPI passive (high)
	 break;
	case MODE_PULSE:
	 PORTB &= ~_BV(RPI); // Set 0 on RPI (active low pulse)
	 break;
   }
  
  case STATE_BYPASS:
   break;
   
  }
  
  last_mode = next_mode;
 }
 
 return 0;
 }
 