URC-RF-base
===========

Tool to receive RF commands from Universal Remote Control's remote and translate it for Lirc.


Summary
-------

Universal Remote Controller (URC, http://www.universalremote.com) utilizes different
proprieraty RF protocols to communicate it's remotes with respective base stations. 

One of these protocols is based on narrowband 418MHz (or 433MHz in international
version) radio transmission. This protocol is used in Complete Control series of products 
and is one-way, from remote to base.

URC bases using this protocol come with separate receiver module, named RFX-250 or 
RFX-250i. Compatible bases are:

- MRF-260
- MRF-350
- MSC-400
- MRX-1

RFX-250 outputs demodulated logic level (0-5V) signal, hiding RF frequency from the user.

The goal of this project is to adopt URC protocol for standard IR remote control 
software, such as LIRC.

Protocol structure
------------------

Each RF transmission consists of header and body. Simple URC base station looks at 
the header and desides whether it should ignore the body or pass it to some IR 
outputs. URC controller (MSC-400) can also receive triggers, which are sent 
in the body of a transmission.

In the body part, the signal is suitable for direct driving IR emitting diodes, i.e. it can be
modulated with different frequencies up to 455KHz. Header part is modulated at most 
common IR frequency around 38KHz. Idle level is high (5V).

Header structure is as follows:

 - Leading pulse (low) - 5000uS
 - Leading space (high) - 500uS
 - Pulse - 500uS
 - Space - 500uS
 - 33 bits of address information, space encoded.
 - Trailing pulse - 250uS
 - Trailing space - 9500uS
 
Each bit of address information consists of:

 - Pulse - 250uS
 - Space - 250uS for 0, 500uS for 1.
 
33 bits represents address block of 11 bits, repeated three times. Receiving side should
compare these 3 addresses and act only if the same address had been received at least
twice.

The address consists of RF ID and channel mask:

	x6	x5	x4	x3	x2	x1	x0	y3	y2	y1	y0

Here, y3..y0 is RF ID (from 1 to 15), most significant bit first. Channel mask consists
of x0..x6 and depends on base station model. For MRF-350, "1" in x1..x6 enables respective
IR outputs 1..6, and x0 controls IR Blaster.

Sample header looks like this (lirc mode2 output):

     4918      514      486      508      245      256  	-> HH0
      236      259      240      258      244      259  	-> 000
      232      259      240      511      239      267  	-> 010
      225      261      238      511      243      529  	-> 011
      210      260      238      260      238      273  	-> 000
      223      272      225      261      237      264  	-> 000
      235      510      244      254      250      244  	-> 100
      240      510      236      510      238      276  	-> 110
      227      253      237      260      239      261  	-> 000
      268      227      238      262      259      499  	-> 001
      230      255      236      281      218      515  	-> 001
      231      513      234      262      236	  9490		-> 10

Which can be decoded to sequence "0000010 0110 0000010 0110 0000010 0110" (0x9813026 in hex), 
or "channel 1, id 6".

LIRC integration
----------------

LIRC is able to learn and recognize demodulated IR signals. It deals with pulse and space length
lying in range from approximately 100uS and longer. Therefore, to make it happy, we need to
demodulate incoming RF pulses.

And then we need to strip off URC RF header, to make derived signal 100% compatible with it's 
IR representation. Without this step, LIRC is virtually unable to learn IR commands with irrecord.
But we can still receive IR commands learned from another source.

LIRC can also receive unstripped URC header with a config like this:

	begin remote

		name  	URC
		flags 	SPACE_ENC
		eps     30
		aeps    100

		header 	500		500
		one     250   	500
		zero    250   	250
		bits    33
		ptrail  250
		gap     9000

		begin codes

			id6_ch1 0x9813026

		end codes

	end remote


Note that this remote description is missing first pulse/space pair from the header, because 
it cannot be correctly described in LIRC terms. Of course, you can use RAW_CODES also.

Again, all this is not needed when using this project in regular way.

Hardware part
-------------

This project uses Atmel Attiny13 for signal transcoding and interfacing with LIRC.
From the output side, it looks exactly as an IR receiver like TSOP4838; output is demodulated
and pulse is represented by low state (active-low).

In my case, Raspberry Pi is running LIRC, communicates with the Attiny via lirc_rpi and, as 
an additional feature, the microcontroller can be programmed from the Raspberry via SPI 
interface (avrdude with linuxspi).

Firmware features
-----------------

There are few configurables in the firmware.

RF ID can be configured to a number from 1 to 15. If so, this project will 
pass only commands sent to this ID. Also, at least one channel in the address
field must match configured channel mask.

If RF ID is set to 0, commands for every address will be transmitted to output.

If RF ID is configured to 255, the project will pass through all pulses.

When RF ID is 254, only URC RF headers will pass through.

