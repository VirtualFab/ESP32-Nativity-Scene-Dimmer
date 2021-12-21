# ESP32-Nativity-Scene-Dimmer
ESP32 LED lights dimmer. Sunrise/sunset/stars effect for a Nativity Scene/Christmas crib (creche)

This is the software part of a hardware project for a board capable of driving up to 16 LED spot lights to be placed under a Nativity Scene.
#### History
A friend of mine, which is a very well known goldsmith and makes beautiful artwork, asked me if I could make the automation for a Nativity Scene he was making for a customer.
The scene is composed of various characters, all being silver handmade statuette representing customer's family members, and the target was to light up each of them with a precise sequence. 

Characters were many (more than ten) and all the statuettes had to be placed on a wonderful marble plane, so the light beneath them must trepass marble opacity, hence the light had to be powerful enough: I needed powerful LED light spots to be dimmed in a precise, programmable sequence. 

I searched a lot but didn't find anything ready to be used, except the [QuinLED Deca](https://quinled.info/2018/09/08/quinled-deca/) which could satisfy my needs although the timing was so short that I hardly could wait the shipment from USA.

So I decided to go for a brand new project, as I could see that the hardware capable of driving a 12V LED spotlight via a MOSFet is very easy nowadays and you have only to make the software for your need.

#### PWM dimming
All this project relies on the powerful capability of the ESP32 PWM: 16 indipendent channels that can drive powerful LED spotlights each with its own animation. WOW! 

You "only" have to instruct the underlying PWM LEDC module with the desired PWM duty-cycle, time by time, as for your animation needs.

So I needed a finite-state machine, capable of running a (complex) animation made of light states and their durations. I needed 4 states for the light:
- ON
- OFF
- FADEUP
- FADEDOWN

and a way of easily lay out the animation program.
#### Sequence representation
To represent the sequence programming I opted for a classic multi-dimensional array directly into the code, since flashing the ESP32 is such an easy thing to do.

The array contains a number of steps composed by pairs of light-state and its duration, all repeated by the number of LEDs.
When I realized it could be handy to have different, selectable animations, the array got a further dimension so the result for the program matrix is a 4-dimension array, which can be hardly imagined but is well represented in C language:

![Screenshot from 2021-12-13 10-11-44](https://user-images.githubusercontent.com/49951329/145784204-f5acff0e-3fb0-4ae8-9967-95c48c24a5c6.png)

The example above represents 3 different program sequences, 4 channels (LEDs) each. The first one is the more complex and for the first LED it provides
- {OFF, 1}: 1 seconds of OFF state
- {FADEUP,15}: a fade up (smooth turning on) to be slowly done in 15 seconds 
- {ON, 1}: 1 second of ON, steady state
- {FADEDOWN,15}: a fade down (smooth turning off) 15 seconds long
- {OFF, 5}: a final OFF state of 5 seconds
after which the whole sequence is repeated endlessly.

The remaining 3 rows in the first block are the sequences for the other 3 LEDs, very similar to the first one but with different timings.

Then the second program block follows, which is a simple ON/OFF cycle of 1 second each, all channels together, for testing purposes.

The third example program is the same test but with fade-in, fade-out effect, each of them lasts 5 seconds.

Switching from program 0 to progam 1 is done by long-pressing the enable button.

#### Timing
The whole main routine is based on time-quantization: each hundredth of a second a control/counting routine is activated which controls the whole process.

Tests were made on a 1/10th of a second basis, which I considered thin enough, but this way the fading steps were noticeable and the final results were disappointing. So I switched to 1/100th of a second resolution, which gives full control and smooth fades.

#### Temperature/FAN control
Being so much the power involved and to keep things safe I incorporated a temperature control via a Dallas/Maxim DS18B20 sensors which drives a PWM controlled fan.

If the temperature beneath the scene reaches FAN_TEMP_MAX then the fan is started and proportinally driven until the temperature falls under FAN_TEMP_MIN

#### Hardware
Being a single item project, board hardware POC has been initially realized on a breadboard and then the final circuit has been realized on a prototype PCB board. 

TODO: Schematics

#### RemoteXY support
Added RemoteXY panel to turn on/off and choose animation and brightness

