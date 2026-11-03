This is an STM32CubeIDE demo program showing how to use the new Music Project Development board.
It assumes the use of a Nucleo-G431RB board, and a simple DAC such as the PCM5102.  It demonstrates
the use of both two-operator FM synthesis and KS (Karplus-Strong) synthesis.  It plays a bass note
using FM synthesis and a short arpeggio up a chord using KS synthesis whenever it receives a wireless
update message from the ESP32.
