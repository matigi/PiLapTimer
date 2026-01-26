Board: RP2350-Touch-AMOLED-1.64

Wiring: IR receiver OUT → GP2, VCC → 3V3, GND → GND

serial output example:
LAP TRIGGERED @ 18477 ms | framesSeen=1 | noiseEdges=442
t=18500 ms | level=HIGH | bursts=5 | frames=1 | noise=442
t=19000 ms | level=HIGH | bursts=9 | frames=8 | noise=465
t=19500 ms | level=HIGH | bursts=0 | frames=16 | noise=489
t=20000 ms | level=HIGH | bursts=5 | frames=24 | noise=489
t=20500 ms | level=HIGH | bursts=9 | frames=32 | noise=489
LAP TRIGGERED @ 20517 ms | framesSeen=33 | noiseEdges=489
t=21000 ms | level=HIGH | bursts=0 | frames=41 | noise=489
t=21500 ms | level=HIGH | bursts=5 | frames=49 | noise=489
t=22000 ms | level=HIGH | bursts=9 | frames=57 | noise=489
t=22500 ms | level=HIGH | bursts=0 | frames=66 | noise=489
LAP TRIGGERED @ 22557 ms | framesSeen=67 | noiseEdges=489
t=23000 ms | level=HIGH | bursts=5 | frames=74 | noise=489
t=23500 ms | level=HIGH | bursts=9 | frames=82 | noise=489
t=24000 ms | level=HIGH | bursts=0 | frames=91 | noise=489

Known-good constants:

FRAME_GAP_MIN_US=19000

MIN_VALID_BURSTS_FOR_FRAME=10

BURST_WINDOW_MS=120

COOLDOWN_MS = 2000
