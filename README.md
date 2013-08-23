URC-RF-base
===========

Tool to receive RF commands from Universal Control Remote and translate it for Lirc.


Summary
=======

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
==================

Each RF transmission consists of header and body. Simple URC base station looks at 
the header and desides whether it should ignore the body or pass it to some IR 
outputs. URC controller (MSC-400) can also receive triggers, which are sent 
in the body of a transmission.

In the body part, the signal is suitable for direct driving IR emitting diodes, i.e. it can be
modulated with different frequencies up to 455KHz. Header part is modulated at most 
common IR frequency around 38KHz. Idle level is high (5V).

Header structure is as follows:

 - Leading pulse (low) - 9000uS
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

LIRC integration
================

Lirc is able to learn and recognize demodulated IR signals. It deals with pulse and space length
lying in range from approximately 100uS and longer. Therefore, to make it happy, we need to
demodulate incoming IR pulses.

And then we need to strip off URC RF header, to make derived signal 100% compatible with it's 
IR representation.

Hardware part
=============

This project uses Atmel Attiny13 for signal transcoding and interfacing with LIRC.
From the output side, it looks exactly as IR receiver like TSOP4838; output is demodulated
and pulse is represented by low state (active-low).

In my case, Raspberry Pi is running LIRC, communicates with the Attiny via lirc_rpi and, as 
an additional feature, the microcontroller can be programmed from the Raspberry via SPI 
interface (avrdude with linuxspi).
