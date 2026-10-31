# potentiometer-joystick-servo-testing

Test for the submarine's X-rudder: one potentiometer thumb joystick drives four servos, one per fin, on an nRF52840 DK. Not tested with other board.

- **Servo:** DFRobot SER0062, 45 kg·cm brushless waterproof ([link](https://www.dfrobot.com/product-2572.html))
- **Mechanics reference:** [X-rudder mechanics, RC Shipyard](https://www.youtube.com/watch?v=_EHDBxhOQPw)
- **Toolchain:** nRF Connect SDK v3.4.1, based on the `servo_testing` project

## How the mixing works

In the video, two pushrods each drive one diagonal pair of fins through a Z-link, so each pair moves like a single rigid shaft through the hull. This project has one servo per fin and no Z-links, so the firmware commands both fins of a pair to match. 

```
       Seen from the stern, looking forward

   upper port (A)         upper stbd (B)
            \               /
             \             /
              (   hull    )
             /             \
            /               \
   lower port (B)         lower stbd (A)

   pair A = yaw - pitch      pitch: + is dive
   pair B = yaw + pitch      yaw:   + is right
```

Where every trailing edge should go:

| Stick | Command | Pair A | Pair B | Every trailing edge moves |
|---|---|---|---|---|
| Forward | Dive | − | + | Down |
| Back | Surface | + | − | Up |
| Right | Right | + | + | To starboard |
| Left | Left | − | − | To port |

A positive pair A command moves the trailing edge up and to starboard. A positive pair B command moves it down and to starboard. With the stick on a diagonal, only one pair moves.

## Wiring

| Part | Pin | nRF pin |
|---|---|---|
| Joystick | +5V | **VDD** (not 5V) |
| Joystick | GND | GND | 
| Joystick | VRx | P0.03 |
| Joystick | VRy | P0.04 |
| Joystick | SW | P1.03 |
| Servo, upper port | Signal | P1.04 |
| Servo, upper stbd | Signal | P1.05 |
| Servo, lower stbd | Signal | P1.06 |
| Servo, lower port | Signal | P1.07 |
| All servos | V+ | External 6–8.4 V supply |
| All servos | GND | External supply GND **and** nRF GND |

 - **AGAIN, MAKE SURE JOYSTICK POWER IS CONNECTED TO VDD NOT 5V**
 - Refer to previous milestone for wiring of each servo

## Demoing

### Seeing what the board is doing

Used AI to make a visual screen in the terminal so you can see whats going on. Plug the nRF board into your computer and open its serial port. On a Mac:

```sh
ls /dev/tty.usbmodem*
screen -U /dev/tty.usbmodem<port> 115200
```

The first command lists the ports. You'll see two, and on our board the one ending in 1 is the right one. Make the window is big enough, or the panel wraps and turns into a mess. `screen` doesn't come with Windows, so use the nRF Serial Terminal in VS Code there.

To get out of `screen`, press `Ctrl-A`, then `K`, then `Y`. If you just close the window, `screen` keeps running in the background and hangs on to the port, and you won't be able to open it again. Run `screen -ls` to find the old session and `screen -X -S <id> quit` to close it. Only have one terminal on the port at a time, and that includes the one in VS Code.

If the window stays blank, press reset on the board. The panel only starts once the joystick passes its check at boot. If it fails, the error prints once and then nothing else happens, so it's easy to miss if you connected late. An unplugged joystick is the usual cause.

### The joystick

Forward dives, back surfaces, and left and right turn. The stick also clicks down like a button, and that's what arms the fins.

The board always starts disarmed. In this state you can move the stick and watch the numbers change on the panel, but the fins stay straight, so it's a safe way to check the joystick. Click the stick down to arm it, and LED1 comes on (will also show in the terminal screen visual). Now the fins follow the stick. Click again to disarm, and they go back to straight.

If a direction comes out backwards, see the settings below.

### The buttons

Buttons 1–4 on the DK move one fin at a time, which is handy for checking that each servo is plugged in where you think it is. Each press moves that fin for one second, then it goes back to straight. They only work while disarmed.

| Button | Fin |
|---|---|
| 1 | Upper port |
| 2 | Upper stbd |
| 3 | Lower stbd |
| 4 | Lower port |

Don't touch the stick while it boots, because that's when it works out where the centre is.

## Settings

| Setting | File | Default | Meaning |
|---|---|---|---|
| `reversed`, `trim_us` | `src/fins.c` | Lower fins reversed, no trim | Per-fin direction and neutral offset |
| `FIN_TRAVEL_US` | `src/fins.c` | 333 µs, about ±30° | Servo movement at full command |
| `FIN_SLEW_PER_LOOP` | `src/fins.c` | 60, about 90°/s | Speed limit on fin movement |
| `JOY_DEADBAND` | `src/joystick.c` | 80, or 8% | Stick dead zone around centre |
| `JOY_INVERT_PITCH`, `JOY_INVERT_YAW` | `src/joystick.c` | Off | Stick direction |

Wasn't sure what angle is needed within the water, so just started at ±30°. Can be tweaked easily.
