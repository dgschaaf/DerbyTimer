#!/usr/bin/env python3
"""
DerbyTimer UART Monitor
=======================
Connects to one DerbyTimer controller over serial and provides four modes:

  [1] Passive Monitor   -- log all received messages, send nothing
  [2] SC Simulator      -- scripted Start Controller sequence
  [3] FC Simulator      -- ACK everything; send winner on keypress
  [4] Protocol Injector -- interactive menu, send any message on demand

Usage:
  python tools/uart_monitor.py COM3
  python tools/uart_monitor.py COM3 --log session.txt
  python tools/uart_monitor.py /dev/ttyUSB0 --baud 57600

Dependencies:
  pip install pyserial
"""

import sys
import time
import struct
import argparse
import threading
import queue
import serial  # pip install pyserial

# ---------------------------------------------------------------------------
# Protocol constants -- must match serialComm.h
# ---------------------------------------------------------------------------

MSG = {
    "NULL":         0x00,
    "ACK":          0x01,
    "NACK":         0x02,
    "RACE_MODE":    0x03,
    "RACE_STATE":   0x04,
    "RACE_START":   0x05,
    "ERROR":        0x06,
    "LEFT_REACT":   0x07,
    "RIGHT_REACT":  0x08,
    "FOUL":         0x09,
    "WINNER":       0x0A,
    "DISP_ADVANCE": 0x0B,
}
MSG_COUNT = 0x0C

MSG_NAME = {v: k for k, v in MSG.items()}

PAYLOAD_LEN = {
    MSG["NULL"]:         0,
    MSG["ACK"]:          1,
    MSG["NACK"]:         1,
    MSG["RACE_MODE"]:    1,
    MSG["RACE_STATE"]:   1,
    MSG["RACE_START"]:   1,
    MSG["ERROR"]:        1,
    MSG["LEFT_REACT"]:   4,
    MSG["RIGHT_REACT"]:  4,
    MSG["FOUL"]:         1,
    MSG["WINNER"]:       1,
    MSG["DISP_ADVANCE"]: 0,
}

# Race state values
STATE = {"IDLE": 0, "STAGING": 1, "COUNTDOWN": 2, "RACING": 3, "COMPLETE": 4, "TEST": 5}
STATE_NAME = {v: k for k, v in STATE.items()}

# Race mode values
MODE = {"GATEDROP": 0, "REACTION": 1, "PRO": 2, "DIALIIN": 3}
MODE_NAME = {v: k for k, v in MODE.items()}

# Foul bitmasks
FOUL_LEFT  = 0b0001
FOUL_RIGHT = 0b0010
FOUL_BOTH  = 0b0011

# Winner bitmasks
WIN_LEFT  = 0b0001
WIN_RIGHT = 0b0010
WIN_TIE   = 0b0100
WIN_NONE  = 0b1000

# Error codes
ERR_NAME = {
    0: "err_NULL",
    1: "err_STATE_TX_TIMEOUT",
    2: "err_MODE_TX_TIMEOUT",
    3: "err_START_TX_TIMEOUT",
    4: "err_SERIAL_OVERFLOW",
    5: "err_INVALID_MSG",
    6: "err_STATE_MISMATCH",
}

# ---------------------------------------------------------------------------
# Payload formatting helpers
# ---------------------------------------------------------------------------

def fmt_payload(msg_id, payload):
    """Return a human-readable string for a message's payload bytes."""
    if msg_id == MSG["RACE_STATE"]:
        return STATE_NAME.get(payload[0], f"0x{payload[0]:02X}")
    if msg_id == MSG["RACE_MODE"]:
        return MODE_NAME.get(payload[0], f"0x{payload[0]:02X}")
    if msg_id == MSG["WINNER"]:
        bits = payload[0]
        parts = []
        if bits & WIN_LEFT:  parts.append("leftWin")
        if bits & WIN_RIGHT: parts.append("rightWin")
        if bits & WIN_TIE:   parts.append("tie")
        if bits & WIN_NONE:  parts.append("noResult")
        return "|".join(parts) if parts else f"0x{bits:02X}"
    if msg_id == MSG["FOUL"]:
        bits = payload[0]
        parts = []
        if bits & FOUL_LEFT:  parts.append("left")
        if bits & FOUL_RIGHT: parts.append("right")
        return "|".join(parts) if parts else f"0x{bits:02X}"
    if msg_id in (MSG["LEFT_REACT"], MSG["RIGHT_REACT"]):
        val = struct.unpack("<i", bytes(payload[:4]))[0]
        return f"{val} us"
    if msg_id == MSG["ACK"] or msg_id == MSG["NACK"]:
        name = MSG_NAME.get(payload[0], f"0x{payload[0]:02X}")
        return f"for {name}"
    if msg_id == MSG["ERROR"]:
        return ERR_NAME.get(payload[0], f"0x{payload[0]:02X}")
    if msg_id == MSG["RACE_START"]:
        return f"mask=0x{payload[0]:02X}"
    return " ".join(f"{b:02X}" for b in payload)

# ---------------------------------------------------------------------------
# Monitor core
# ---------------------------------------------------------------------------

class Monitor:
    def __init__(self, port, baud, log_path):
        self.port = port
        self.baud = baud
        self.log_path  = log_path
        self.log_file  = None
        self.start_time = time.monotonic()
        self._parse_buf = bytearray()
        self._parse_id  = None
        self._parse_expected = 0
        self.ack_timeout = 0.150   # 3 * 50ms protocol timeout + margin

        try:
            self.ser = serial.Serial(port, baud, timeout=0.05)
        except serial.SerialException as e:
            print(f"[ERROR] Cannot open {port}: {e}")
            sys.exit(1)

        if log_path:
            self.log_file = open(log_path, "w", encoding="utf-8")

    def elapsed(self):
        t = time.monotonic() - self.start_time
        minutes = int(t // 60)
        seconds = t % 60
        return f"{minutes:02d}:{seconds:06.3f}"

    def log(self, line):
        ts = f"[{self.elapsed()}]"
        full = f"{ts} {line}"
        print(full)
        if self.log_file:
            self.log_file.write(full + "\n")
            self.log_file.flush()

    def send(self, msg_id, payload=b"", direction="TX", note=""):
        frame = bytes([msg_id]) + bytes(payload)
        self.ser.write(frame)
        name = MSG_NAME.get(msg_id, f"0x{msg_id:02X}")
        pay_str = fmt_payload(msg_id, list(payload)) if payload else ""
        suffix = f"  [{note}]" if note else ""
        self.log(f"-> {direction:<3} {name:<18} {pay_str}{suffix}")

    def send_ack(self, ack_id):
        self.send(MSG["ACK"], bytes([ack_id]), note="auto-ACK")

    def send_nack(self, nack_id):
        self.send(MSG["NACK"], bytes([nack_id]))

    def _drain_rx(self):
        """Read all available bytes and yield complete (msg_id, payload) tuples."""
        raw = self.ser.read(64)
        if not raw:
            return
        self._parse_buf.extend(raw)

        while self._parse_buf:
            if self._parse_id is None:
                b = self._parse_buf[0]
                if b >= MSG_COUNT:
                    self._parse_buf = self._parse_buf[1:]
                    self.log(f"[WARN] Unknown message ID 0x{b:02X} -- discarded")
                    continue
                self._parse_id = b
                self._parse_expected = PAYLOAD_LEN.get(b, 0)
                self._parse_buf = self._parse_buf[1:]

            needed = self._parse_expected
            if len(self._parse_buf) < needed:
                break  # wait for more bytes

            payload = bytes(self._parse_buf[:needed])
            self._parse_buf = self._parse_buf[needed:]
            msg_id = self._parse_id
            self._parse_id = None
            yield msg_id, payload

    def recv_messages(self):
        """Drain RX, log each message, return list of (msg_id, payload) tuples."""
        received = []
        for msg_id, payload in self._drain_rx():
            name = MSG_NAME.get(msg_id, f"0x{msg_id:02X}")
            pay_str = fmt_payload(msg_id, list(payload)) if payload else ""
            self.log(f"<- RX  {name:<18} {pay_str}")
            received.append((msg_id, payload))
        return received

    def wait_ack(self, for_id=None, timeout=None):
        """
        Block until an ACK is received (optionally for a specific msg_id),
        auto-handling any non-ACK messages that arrive while waiting.
        Returns True on ACK, False on timeout.
        """
        if timeout is None:
            timeout = self.ack_timeout
        deadline = time.monotonic() + timeout
        while time.monotonic() < deadline:
            msgs = self.recv_messages()
            for mid, pay in msgs:
                if mid == MSG["ACK"]:
                    if for_id is None or (pay and pay[0] == for_id):
                        return True
            time.sleep(0.005)
        self.log(f"[TIMEOUT] No ACK for {MSG_NAME.get(for_id, str(for_id))}")
        return False

    def close(self):
        self.ser.close()
        if self.log_file:
            self.log_file.close()

# ---------------------------------------------------------------------------
# Mode 1: Passive Monitor
# ---------------------------------------------------------------------------

def mode_passive(mon):
    mon.log("[MODE] Passive Monitor -- press Ctrl+C to exit")
    try:
        while True:
            msgs = mon.recv_messages()
            for mid, pay in msgs:
                # Auto-ACK everything so the controller doesn't stall
                if mid not in (MSG["ACK"], MSG["NACK"]):
                    mon.send_ack(mid)
            time.sleep(0.01)
    except KeyboardInterrupt:
        mon.log("[STOP] Monitor stopped")

# ---------------------------------------------------------------------------
# Mode 2: SC Simulator
# ---------------------------------------------------------------------------

def mode_sc_sim(mon):
    mon.log("[MODE] SC Simulator -- scripted Start Controller sequence")
    mon.log("       Press Enter to advance each step, Ctrl+C to abort")

    steps = [
        (MSG["RACE_STATE"], bytes([STATE["STAGING"]]),   "STAGING"),
        (MSG["RACE_STATE"], bytes([STATE["COUNTDOWN"]]), "COUNTDOWN"),
        (MSG["RACE_START"], bytes([0b0111]),              "GO"),
        (MSG["RACE_STATE"], bytes([STATE["COMPLETE"]]),  "COMPLETE"),
        (MSG["RACE_STATE"], bytes([STATE["IDLE"]]),      "IDLE"),
    ]

    try:
        for msg_id, payload, label in steps:
            input(f"\n  [Enter to send {label}] ")
            mon.send(msg_id, payload)
            if not mon.wait_ack(for_id=msg_id):
                mon.log(f"[WARN] No ACK for {label} -- continuing anyway")

        mon.log("[SC SIM] Waiting for MSG_WINNER from FC...")
        deadline = time.monotonic() + 30.0
        while time.monotonic() < deadline:
            msgs = mon.recv_messages()
            for mid, pay in msgs:
                if mid == MSG["WINNER"]:
                    mon.send_ack(mid)
                    mon.log("[SC SIM] Winner received and ACKed -- sequence complete")
                    return
            time.sleep(0.01)
        mon.log("[WARN] No MSG_WINNER received within 30s")

    except KeyboardInterrupt:
        mon.log("[STOP] SC Simulator aborted")

# ---------------------------------------------------------------------------
# Mode 3: FC Simulator
# ---------------------------------------------------------------------------

def mode_fc_sim(mon):
    mon.log("[MODE] FC Simulator -- ACKs all messages; send winner via keypresses:")
    mon.log("       w1=leftWin  w2=rightWin  wt=tie  wn=noResult")
    mon.log("       r1 <ms>=left react  r2 <ms>=right react")
    mon.log("       Press Ctrl+C to exit")

    # Use a thread for non-blocking stdin on Windows
    stdin_q = queue.Queue()
    def stdin_reader():
        while True:
            try:
                line = input()
                stdin_q.put(line.strip())
            except EOFError:
                break
    t = threading.Thread(target=stdin_reader, daemon=True)
    t.start()

    try:
        while True:
            msgs = mon.recv_messages()
            for mid, pay in msgs:
                if mid not in (MSG["ACK"], MSG["NACK"]):
                    mon.send_ack(mid)

            try:
                cmd = stdin_q.get_nowait()
            except queue.Empty:
                time.sleep(0.01)
                continue

            if cmd == "w1":
                mon.send(MSG["WINNER"], bytes([WIN_LEFT]))
            elif cmd == "w2":
                mon.send(MSG["WINNER"], bytes([WIN_RIGHT]))
            elif cmd == "wt":
                mon.send(MSG["WINNER"], bytes([WIN_TIE]))
            elif cmd == "wn":
                mon.send(MSG["WINNER"], bytes([WIN_NONE]))
            elif cmd.startswith("r1 ") or cmd.startswith("r2 "):
                lane = cmd[1]
                try:
                    ms = int(cmd[3:])
                    us = ms * 1000
                    payload = struct.pack("<i", us)
                    msg_id = MSG["LEFT_REACT"] if lane == "1" else MSG["RIGHT_REACT"]
                    mon.send(msg_id, payload)
                except ValueError:
                    print("  Usage: r1 <ms>  or  r2 <ms>")
            else:
                print(f"  Unknown: '{cmd}'  (w1 w2 wt wn r1 <ms> r2 <ms>)")

    except KeyboardInterrupt:
        mon.log("[STOP] FC Simulator stopped")

# ---------------------------------------------------------------------------
# Mode 4: Protocol Injector
# ---------------------------------------------------------------------------

def mode_injector(mon):
    mon.log("[MODE] Protocol Injector -- send any message on demand")

    menu = """
  [1] MSG_RACE_MODE    [2] MSG_RACE_STATE  [3] MSG_RACE_START
  [4] MSG_FOUL         [5] MSG_WINNER      [6] MSG_LEFT_REACT
  [7] MSG_RIGHT_REACT  [8] MSG_ERROR       [9] MSG_DISP_ADVANCE
  [b] Bad message ID (trigger NACK)        [q] Quit
"""

    stdin_q = queue.Queue()
    def stdin_reader():
        while True:
            try:
                line = input()
                stdin_q.put(line.strip())
            except EOFError:
                break
    t = threading.Thread(target=stdin_reader, daemon=True)
    t.start()

    def prompt_int(label, lo, hi):
        while True:
            try:
                v = int(input(f"    {label} ({lo}-{hi}): "))
                if lo <= v <= hi:
                    return v
            except (ValueError, EOFError):
                pass
            print(f"    Enter a number between {lo} and {hi}")

    print(menu)

    try:
        while True:
            msgs = mon.recv_messages()
            for mid, pay in msgs:
                if mid not in (MSG["ACK"], MSG["NACK"]):
                    mon.send_ack(mid)

            try:
                cmd = stdin_q.get_nowait()
            except queue.Empty:
                time.sleep(0.01)
                continue

            if cmd == "1":
                v = prompt_int("Mode (0=GATEDROP 1=REACTION 2=PRO 3=DIALIIN)", 0, 3)
                mon.send(MSG["RACE_MODE"], bytes([v]))
                mon.wait_ack(MSG["RACE_MODE"])
            elif cmd == "2":
                v = prompt_int("State (0=IDLE 1=STAGING 2=COUNTDOWN 3=RACING 4=COMPLETE 5=TEST)", 0, 5)
                mon.send(MSG["RACE_STATE"], bytes([v]))
                mon.wait_ack(MSG["RACE_STATE"])
            elif cmd == "3":
                mon.send(MSG["RACE_START"], bytes([0b0111]))
                mon.wait_ack(MSG["RACE_START"])
            elif cmd == "4":
                v = prompt_int("Foul (1=left 2=right 3=both)", 1, 3)
                mon.send(MSG["FOUL"], bytes([v]))
                mon.wait_ack(MSG["FOUL"])
            elif cmd == "5":
                v = prompt_int("Winner (1=leftWin 2=rightWin 4=tie 8=noResult)", 1, 15)
                mon.send(MSG["WINNER"], bytes([v]))
                mon.wait_ack(MSG["WINNER"])
            elif cmd == "6":
                ms = prompt_int("Left reaction time (ms)", 0, 99999)
                payload = struct.pack("<i", ms * 1000)
                mon.send(MSG["LEFT_REACT"], payload)
                mon.wait_ack(MSG["LEFT_REACT"])
            elif cmd == "7":
                ms = prompt_int("Right reaction time (ms)", 0, 99999)
                payload = struct.pack("<i", ms * 1000)
                mon.send(MSG["RIGHT_REACT"], payload)
                mon.wait_ack(MSG["RIGHT_REACT"])
            elif cmd == "8":
                v = prompt_int("Error code (1-6)", 1, 6)
                mon.send(MSG["ERROR"], bytes([v]))
                mon.wait_ack(MSG["ERROR"])
            elif cmd == "9":
                mon.send(MSG["DISP_ADVANCE"])
                mon.wait_ack(MSG["DISP_ADVANCE"])
            elif cmd == "b":
                mon.log("[INJECT] Sending invalid message ID 0xFF")
                mon.ser.write(bytes([0xFF]))
            elif cmd in ("q", "Q"):
                break
            elif cmd == "":
                print(menu)
            else:
                print(f"  Unknown: '{cmd}'")

    except KeyboardInterrupt:
        pass

    mon.log("[STOP] Injector stopped")

# ---------------------------------------------------------------------------
# Entry point
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="DerbyTimer UART Monitor",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=__doc__,
    )
    parser.add_argument("port",          help="Serial port (e.g. COM3 or /dev/ttyUSB0)")
    parser.add_argument("--baud", "-b",  type=int, default=115200, help="Baud rate (default 115200)")
    parser.add_argument("--log",  "-l",  default=None,             help="Save session to file")
    args = parser.parse_args()

    mon = Monitor(args.port, args.baud, args.log)
    mon.log(f"[OPEN] {args.port} at {args.baud} baud")
    if args.log:
        mon.log(f"[LOG]  Writing to {args.log}")

    print("\nSelect mode:")
    print("  [1] Passive Monitor   -- log all received messages, send nothing")
    print("  [2] SC Simulator      -- act as Start Controller")
    print("  [3] FC Simulator      -- ACK everything, send winner on keypress")
    print("  [4] Protocol Injector -- interactive menu, send any message\n")

    choice = input("Mode [1-4]: ").strip()

    if choice == "1":
        mode_passive(mon)
    elif choice == "2":
        mode_sc_sim(mon)
    elif choice == "3":
        mode_fc_sim(mon)
    elif choice == "4":
        mode_injector(mon)
    else:
        print("Invalid choice -- defaulting to Passive Monitor")
        mode_passive(mon)

    mon.close()

if __name__ == "__main__":
    main()
