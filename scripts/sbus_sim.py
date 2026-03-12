#!/usr/bin/env python3
"""
SBUS simulator - outputs valid SBUS frames to a pty.
Prints the pty slave path so you can connect your parser to it.

Usage:
    python3 sbus_sim.py [--fast] [--failsafe] [--port /dev/ttyUSB0]

    --fast         7ms frame interval (SBUS fast mode), default is 14ms
    --failsafe     Set failsafe flag in frames
    --port PATH    Write to real serial port at 100kbps 8E2 (requires pyserial)

SBUS framing: 100kbps, 8E2, inverted signal.
Over pty: inversion is skipped - handle in hardware or flip in your parser.
Over real UART (--port): pyserial sets 100000 baud, 8E2.

To bridge pty to real UART via socat:
    socat <slave_path> /dev/ttyUSB0,b100000,parenb,cs8,cstopb,raw
"""

import os
import pty
import time
import math
import argparse

SBUS_HEADER   = 0x0F
SBUS_FOOTER   = 0x00
SBUS_CH_MIN   = 172
SBUS_CH_MAX   = 1811
SBUS_CH_MID   = 992
SBUS_FLAG_FAILSAFE   = (1 << 7)
SBUS_FLAG_FRAMELOST  = (1 << 6)


def pack_sbus_frame(channels, flags=0x00):
    """Pack 16 x 11-bit channel values into a 25-byte SBUS frame."""
    assert len(channels) == 16

    ch = [max(SBUS_CH_MIN, min(SBUS_CH_MAX, c)) for c in channels]

    bits = 0
    for i, v in enumerate(ch):
        bits |= (v & 0x7FF) << (i * 11)

    data = [(bits >> (i * 8)) & 0xFF for i in range(22)]
    return bytes([SBUS_HEADER] + data + [flags, SBUS_FOOTER])


def channels_sine(t):
    """Animate channels with sine waves at different frequencies."""
    channels = []
    for i in range(16):
        phase = i * (2 * math.pi / 16)
        freq = 0.5 + i * 0.1
        val = SBUS_CH_MID + int(400 * math.sin(2 * math.pi * freq * t + phase))
        channels.append(val)
    return channels


def run_pty(interval, flags):
    master_fd, slave_fd = pty.openpty()
    slave_path = os.ttyname(slave_fd)

    print(f"Slave pty : {slave_path}")
    print(f"To bridge to real UART:")
    print(f"  socat {slave_path} /dev/ttyUSB0,b100000,parenb,cs8,cstopb,raw")
    print()

    t = 0.0
    frame_count = 0
    try:
        while True:
            start = time.monotonic()
            channels = channels_sine(t)
            os.write(master_fd, pack_sbus_frame(channels, flags))
            frame_count += 1
            if frame_count % 100 == 0:
                print(f"frames={frame_count}  ch1={channels[0]}  ch2={channels[1]}")
            t += interval
            sleep = interval - (time.monotonic() - start)
            if sleep > 0:
                time.sleep(sleep)
    except KeyboardInterrupt:
        print(f"\nDone. {frame_count} frames sent.")
    finally:
        os.close(master_fd)
        os.close(slave_fd)


def run_serial(port, interval, flags):
    import serial
    ser = serial.Serial(
        port=port,
        baudrate=100000,
        bytesize=serial.EIGHTBITS,
        parity=serial.PARITY_EVEN,
        stopbits=serial.STOPBITS_TWO,
        timeout=1
    )
    print(f"Sending SBUS to {port} at 100kbps 8E2")
    print("Note: signal inversion must be handled externally (transistor inverter)")
    print()

    t = 0.0
    frame_count = 0
    try:
        while True:
            start = time.monotonic()
            channels = channels_sine(t)
            ser.write(pack_sbus_frame(channels, flags))
            frame_count += 1
            if frame_count % 100 == 0:
                print(f"frames={frame_count}  ch1={channels[0]}  ch2={channels[1]}")
            t += interval
            sleep = interval - (time.monotonic() - start)
            if sleep > 0:
                time.sleep(sleep)
    except KeyboardInterrupt:
        print(f"\nDone. {frame_count} frames sent.")
    finally:
        ser.close()


def main():
    parser = argparse.ArgumentParser(description="SBUS frame simulator")
    parser.add_argument("--fast", action="store_true", help="7ms frame rate")
    parser.add_argument("--failsafe", action="store_true", help="Set failsafe flag")
    parser.add_argument("--port", metavar="PATH", help="Real serial port (e.g. /dev/ttyUSB0)")
    args = parser.parse_args()

    interval = 0.007 if args.fast else 0.014
    flags = SBUS_FLAG_FAILSAFE if args.failsafe else 0x00

    print(f"SBUS simulator  |  {'fast 7ms' if args.fast else 'normal 14ms'}  |  "
          f"{'FAILSAFE' if args.failsafe else 'normal'}")

    if args.port:
        run_serial(args.port, interval, flags)
    else:
        run_pty(interval, flags)


if __name__ == "__main__":
    main()
