# DMX timing

## Basics

This folder contains some documentation and an explanation of the DMX signal
timings and a tool to compute and visualize a configuration for specific
needs.

DMX uses a standard 8N2 byte encoding with 1 start bit (low), 8 data bits
(LSB first) and 2 stop bits (high) and sends repeatedly and continuously a 
block of up to 512 bytes over a serial RS-485 line at a rate of 250.000 bits/s.
The whole block is called a DMX packet and the single bytes are referred 
to as "frames" or "slots". Every frame corresponds to a DMX channel except for the
first at index 0 which in normal DMX operation has a value of 0 and is called 
the start byte.

Besides this, the DMX standard defines breaks and gaps between the packets
as well as the frames. A good explanation can be found here: 

https://www.erwinrol.com/page/articles/dmx512

## Timing problems

When trying to control a cheap, china made moving head fixture with this
sketch in UART mode, it turned out that this fixture principally reacted to the 
DMX signal, however, showed many small random moves, jitter, and unpredictable 
changes in color, strobe, etc.

By contrast, when attached to a commercial "computer lamp table controller" 
or DMX512 controller, the reaction was absolute as expected and smooth.

A comparison of the signals with an oscilloscope and a logic analyzer showed
a significant difference in the timing parameters. Most obviously there was
a long (60 μs) high signal after the last stop bit.
This can be considered as many extra stop bits or an interframe break,
also referred to as "mark time between slots".

The following image shows a comparison of the signal of the commercial
controller (D0), the original implementation using the microcontroller's 
UART (D2) and the signal using the I2S mode (D1).
The timing parameters of the last one were chosen for this comparison to match
the signal of the commercial controller as close as possible.
It is not perfectly possible to do so, because a closer look reveals that the
duration of the inter-frame breaks is not constant but varies slightly from
frame to frame. 

Presumably, this is caused by the fact that the controller scans the 
linear potentiometers in sequence and the ADC (analog-digital converter) 
does not guarantee a fixed conversion time.

# ![](Compare_Manual_I2S_UART.png)

## Timing parameter calculation

The physical signals D0-D2 in the above image were sampled with the open
source application PuleView and an 8-channel USB logic analyzer.
I bought mine here https://www.az-delivery.de/en/products/saleae-logic-analyzer
but there is plenty of such devices in the bay or your favorite china store.

When encountering timing problems, I started to analyze the signal with an 
oscilloscope first, but finding the right trigger or pressing the hold button 
at the right time felt like trying to catch a fly with chopsticks.
Whereas doing the same thing with a logic analyzer is merely child's play.
 
The screenshot contains also the VisualizeDMX.java tool that you can find 
in this directory and which simulates how the signal is expected to look like 
on an oscilloscope screen and also computes the maximum achievable packet rate.

Note that the DMX channels 1 - 8 were set to powers of 2 represented by a single
bit from LSB to MSB to assert that the bit order is correct as well as endianness.

This was a problematic issue in the first implementation of I2S mode, because
the way I2S works (32 bits, little endian, MSB first) differs substantially 
from the way DMX works (8-bit LSB first) and things can quickly become counter
intuitive.

The class BitArray takes care of the bit shifting, flipping, masking, and byte 
indexing operations necessary to make sure the DMX values set will end up in 
the correct position and order in the long and confusing stream of bits.
It comes as a C++ as well as Java implementation because the latter was
easier to develop, debug, test, and visualize and ported afterward.

You can also find some tests for these classes that might help to understand
how they work if necessary.

## Recommended timing parameters and byte alignment

Using I2S mode and almost the same timing parameters as the commercial
controller, the problematic fixture worked flawlessly. 
However, because of the extremely long breaks and marks, the overall 
throughput dropped from 40 packets/s to below 30.

The following image shows a drill into the aforementioned signal analysis.
It becomes apparent that using the UART, there will be no interframe break at
all and is also impossible to implement, because the microprocessors's
hardware does not allow to send more than 2 stop bits.

# ![](Interframe.png)

Experimenting a little bit with these parameters showed that the number of
stop bits and the length of the "space for break" were the game changers.
The fixture needed at least 44 μs and 3 stop bits to work smoothly. 
Your mileage however may var with your own fixtures. 
For this reason, the default parameters in the DmxArray class are more 
generous and assert byte alignment of the channels in RAM.

Although I did not encounter any problems so far, (having no) alignment 
might become an issue with fixtures that e.g. incorporate moving parts 
as a result of a single DMX value ending up in two different bytes.

Because it is impossible to know how far DMA has already progressed, 
we cannot assert that the new value of these bytes gets transferred in
a transactional sense and as a result, some MSBs might still have the old 
value while the LSBs were updated and transferred.

This could result e.g. in perceptible hiccups for something that moves.

 
## DMA / I2S
 
In contrast to UART, the I2S mode allows control of every single bit of the signal
as well as a very precise definition of the frequency. I2S is normally used
to send audio data to an external DAC (digital-analog converter).

The Arduino environment already contains support for I2S but the scenario
for an audio stream substantially differs from what we need for a DMX
signal.

While we would like to create a buffer with a valid DMX signal once,
instruct DMA to transfer it at a specific data rate to the I2S GPIO of the
ESP8266 continuously, the Arduino code takes explicit measures to prevent 
DMA from looping over the data.
It maintains a ring chain of buffers, making sure that the "oldest" is the 
one that gets fed, eventually even zero'ing a buffer that was ubject to a
buffer underrun.

This means that we would need to assert that we feed the DMX data over and
over again to keep this buffer filled. In the first implementation of I2S 
mode this was exactly the case and it felt wrong right away but I had no 
clue how the I2S thing works and no documentation to change this.

Starting with the I2S source files in the Arduino/ESP8266 project,
I ended up with what you will find in i2s_dmx.{h|cpp}.
The does exactly the opposite of the original code: 
When passed a pre-allocated buffer to i2s_dmx_begin(), it will initialize 
DMA such that it will continuously transfer this buffer at a rate that 
was set with i2s_set_rate() before.
I did not want to change port anything that already exists. For this reason
the original I2S includes are still necessary e.g. for setting the rate.

When Artnet data is received later, the only thing the code (loop() method) 
needs to do is to update the DMX values within this buffer at the 
corresponding bit positions. 
There is no need to feed/send this data or to inform the DMA
subsystem about changes. Everything that was updated in the buffer will be
sent next time the DMA rushes over the changed bytes.

There is a potential risk with doing so: There is no guarantee when a byte
that is subject to a change will be transferred next time. 

This should not be a big deal in general because it only means that a specific
update will go live like 25ms earlier or later - negligible.

However, a single channel value might get split up into some more
significant bits and some less significant ones. When not written in a 
single transactional that might cause unwanted and perceptible effects e.g.
for fixtures that incorporate moving parts like moving heads or scanners.

Therefore the default timing parameters have been chosen such that a single
DMX channel value will always be aligned at 8-bit boundaries, asserting that
there no carry will occur.

## Performance impact

Although the calculation in the class BitArray may look demanding and time
consuming, at the end of the day they consist of logical or mathematical
that can be considered a microcontroller's daily business.

In fact, when trying to set all channels in a 512 channel DMX sigal in a 
loop like 10.000 times or so, this results in an overall impact on the execution
time of loop() of 1 or 2 milli seconds.

As the loop method contains code anyway to prevent from too frequent excution 
of the code that sends UART data, this is no problem at all.
In contrast to the former implementation the I2S signal will no longer affect
the packet rate of data sent via UART in any way.

Indeed, the two frame rates are completely independent of each other because
I2S is mainly controlled by your choice of the aforementioned timing parameters
while UART rate is controller by Config.delay.











