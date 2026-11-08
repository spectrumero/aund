#!/usr/bin/env python3
"""Stress tests for aund on the FileStick emulator: a big directory, a big
file, and a request from a second station (0.51) in the middle of a SAVE
from 0.50. Run it with run.py, which starts the emulator and aund."""
import select, socket, sys, threading, time, random

FS_STN = 25
FS_ADDR = ('127.0.0.1', 20025)
REPLY, DATA, ACK = 0x90, 0x91, 0x92

class Station:
    def __init__(self, stn):
        self.me = stn
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.bind(('127.0.0.1', 20000 + stn))
        self.urd = self.csd = self.lib = 0

    def rx(self, timeout):
        end = time.time() + timeout
        while True:
            left = end - time.time()
            if left <= 0: return None
            r, _, _ = select.select([self.sock], [], [], left)
            if not r: return None
            d, _ = self.sock.recvfrom(4096)
            if d[:2] == bytes([self.me, 0]):     # for us
                return d

    def hdr(self):
        return bytes([FS_STN, 0, self.me, 0])

    def ack_ok(self, a):
        return a is not None and len(a) == 4 and a == bytes([self.me, 0, FS_STN, 0])

    def send(self, port, data, ctrl=0x80, tries=60):
        for _ in range(tries):
            self.sock.sendto(self.hdr() + bytes([ctrl, port]), FS_ADDR)
            if not self.ack_ok(self.rx(0.3)):
                time.sleep(0.05); continue
            self.sock.sendto(self.hdr() + data, FS_ADDR)
            if self.ack_ok(self.rx(0.5)): return
            raise RuntimeError("%d: no data ack" % self.me)
        raise RuntimeError("%d: not listening" % self.me)

    def recv(self, port, timeout=30):
        end = time.time() + timeout
        while time.time() < end:
            s = self.rx(end - time.time())
            if s is None: break
            if len(s) == 6 and s[2:4] == bytes([FS_STN, 0]) and s[5] == port:
                self.sock.sendto(self.hdr(), FS_ADDR)
                d = self.rx(1.0)
                self.sock.sendto(self.hdr(), FS_ADDR)
                return d[4:]
        raise RuntimeError("%d: timeout on &%02X" % (self.me, port))

    def req(self, func, body, urd=None):
        self.send(0x99, bytes([REPLY, func, self.urd if urd is None else urd,
                               self.csd, self.lib]) + body)
        r = self.recv(REPLY)
        return r[1], r[2:]

    def cli(self, cmd):
        return self.req(0, cmd.encode() + b'\r')

    def iam(self):
        rc, d = self.cli("I AM SYST")
        assert rc == 0
        self.urd, self.csd, self.lib = d[0], d[1], d[2]

    def save(self, name, data, load=0x1900, exe=0x8023, midway=None):
        body = load.to_bytes(4, 'little') + exe.to_bytes(4, 'little') + \
            len(data).to_bytes(3, 'little') + name.encode() + b'\r'
        rc, d = self.req(1, body, urd=ACK)
        if rc: return rc
        port, blk = d[0], int.from_bytes(d[1:3], 'little')
        for n, i in enumerate(range(0, len(data), blk)):
            self.send(port, data[i:i+blk])
            if n == 0 and midway: midway()
            if i + blk < len(data):
                self.recv(ACK)
        r = self.recv(REPLY)
        return r[1]

    def load(self, name):
        self.send(0x99, bytes([REPLY, 2, DATA, self.csd, self.lib]) +
                  name.encode() + b'\r')
        r = self.recv(REPLY)
        if r[1]: return r[1], None
        size = int.from_bytes(r[10:13], 'little')
        got = b''
        while len(got) < size:
            got += self.recv(DATA)
        r2 = self.recv(REPLY)
        return r2[1], got

results = []
def check(name, ok, detail=''):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (" -- " + detail if detail else ""))

a, b = Station(50), Station(51)
a.iam(); b.iam()
check("two stations logged on", True)

t = time.time()
rcs = [a.save("F%02d" % i, bytes([i]) * 100) for i in range(30)]
check("SAVE 30 files", all(r == 0 for r in rcs), "%.1fs" % (time.time() - t))

names = []
start = 0
while True:
    rc, d = a.req(3, bytes([2, start, 10]) + b'\r')   # EXAMINE names
    if rc or d[0] == 0: break
    # names format: count, undef, then entries of length byte + name
    p, n = 2, d[0]
    for _ in range(n):
        ln = d[p]; names.append(bytes(d[p+1:p+1+ln]).decode().strip()); p += 1 + ln
    start += n
check("EXAMINE lists all 30", len(names) == 30 and names[0] == "F00" and names[-1] == "F29",
      "%d names, first %s last %s" % (len(names), names[:1], names[-1:]))

random.seed(2)
big = bytes(random.randrange(256) for _ in range(30000))
t = time.time()
check("SAVE 30000 bytes", a.save("BIG", big) == 0, "%.1fs" % (time.time() - t))
t = time.time()
rc, got = a.load("BIG")
check("LOAD 30000 bytes back", rc == 0 and got == big, "%.1fs" % (time.time() - t))

# Station 51 asks for *INFO while station 50 is in the middle of a SAVE
out = {}
def b_request():
    b.send(0x99, bytes([REPLY, 0, b.urd, b.csd, b.lib]) + b"INFO F07\r")
    out['accepted'] = time.time()
    r = b.recv(REPLY)
    out['b'] = (r[1], r[2:])
th = None
def midway():
    global th
    # 50 carries on with its SAVE (and keeps answering) while 51 sends
    th = threading.Thread(target=b_request); th.start()
data = bytes(range(256)) * 32
rc = a.save("MID", data, midway=midway)
save_done = time.time()
th.join(30)
check("51's request accepted while 50's SAVE was still going",
      out.get('accepted', 1e99) < save_done,
      "accepted %.2fs before the SAVE finished" % (save_done - out.get('accepted', 0)))
check("SAVE completes with another station's request in the middle", rc == 0)
rcb, db = out.get('b', (None, b''))
check("other station's request answered afterwards",
      rcb == 0 and b'F07' in bytes(db), bytes(db).split(b'\r')[0].decode('latin1'))
rc, got = a.load("MID")
check("MID intact", rc == 0 and got == data)

for i in range(30):
    a.cli("DELETE F%02d" % i)
a.cli("DELETE BIG"); a.cli("DELETE MID")
rc, d = a.req(3, bytes([2, 0, 10]) + b'\r')
check("cleaned up, still serving", rc == 0 and d[0] == 0, "%d left" % d[0])
print("%d/%d passed" % (sum(results), len(results)))
sys.exit(0 if all(results) else 1)
