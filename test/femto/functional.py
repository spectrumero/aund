#!/usr/bin/env python3
"""Functional tests for aund on the FileStick emulator: a minimal NetFS
client over BeebEm-style UDP Econet. Station 0.50 talks to the file
server, 0.25. Run it with run.py, which starts the emulator and aund."""
import select, socket, sys, time, random

FS_STN, ME = 25, 50
FS_ADDR = ('127.0.0.1', 20025)
REPLY_PORT, DATA_PORT, ACK_PORT = 0x90, 0x91, 0x92

sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sock.bind(('127.0.0.1', 20050))

def rx(timeout):
    r, _, _ = select.select([sock], [], [], timeout)
    if not r:
        return None
    d, _ = sock.recvfrom(4096)
    return d

def send(port, data, ctrl=0x80, tries=40):
    """Four way handshake to the file server."""
    for _ in range(tries):
        sock.sendto(bytes([FS_STN, 0, ME, 0, ctrl, port]), FS_ADDR)
        a = rx(0.3)
        if a is None or a[:4] != bytes([ME, 0, FS_STN, 0]) or len(a) != 4:
            time.sleep(0.05)
            continue
        sock.sendto(bytes([FS_STN, 0, ME, 0]) + data, FS_ADDR)
        a = rx(0.3)
        if a is not None and a[:4] == bytes([ME, 0, FS_STN, 0]):
            return
        raise RuntimeError("no data ack")
    raise RuntimeError("not listening on port &%02X" % port)

def recv(port, timeout=15.0):
    """Receive a frame from the file server on port."""
    end = time.time() + timeout
    while time.time() < end:
        s = rx(end - time.time())
        if s is None:
            break
        if len(s) == 6 and s[:4] == bytes([ME, 0, FS_STN, 0]) and s[5] == port:
            ctrl = s[4]
            sock.sendto(bytes([FS_STN, 0, ME, 0]), FS_ADDR)
            d = rx(1.0)
            if d is None or d[:4] != bytes([ME, 0, FS_STN, 0]):
                raise RuntimeError("no data frame after scout")
            sock.sendto(bytes([FS_STN, 0, ME, 0]), FS_ADDR)
            return ctrl, d[4:]
    raise RuntimeError("timed out waiting on port &%02X" % port)

class Client:
    def __init__(self):
        self.urd = self.csd = self.lib = 0
        self.seq = {}

    def req(self, func, body, urd=None, ctrl=0x80):
        u = self.urd if urd is None else urd
        send(0x99, bytes([REPLY_PORT, func, u, self.csd, self.lib]) + body, ctrl)
        _, r = recv(REPLY_PORT)
        return r[0], r[1], r[2:]

    def cli(self, cmd):
        cc, rc, data = self.req(0, cmd.encode() + b'\r')
        return cc, rc, data

    def iam(self, user):
        cc, rc, d = self.cli("I AM " + user)
        assert rc == 0, err(rc, d)
        self.urd, self.csd, self.lib = d[0], d[1], d[2]
        return d

    def save(self, name, data, load, exe):
        body = load.to_bytes(4, 'little') + exe.to_bytes(4, 'little') + \
            len(data).to_bytes(3, 'little') + name.encode() + b'\r'
        cc, rc, d = self.req(1, body, urd=ACK_PORT)
        if rc:
            return rc, d
        port, blk = d[0], int.from_bytes(d[1:3], 'little')
        for i in range(0, len(data), blk):
            send(port, data[i:i+blk])
            if i + blk < len(data):
                recv(ACK_PORT)
        _, r = recv(REPLY_PORT)
        return r[1], r[2:]

    def load(self, name):
        send(0x99, bytes([REPLY_PORT, 2, DATA_PORT, self.csd, self.lib]) +
             name.encode() + b'\r')
        _, r = recv(REPLY_PORT)
        if r[1]:
            return r[1], r[2:], None
        info = r[2:]
        size = int.from_bytes(info[8:11], 'little')
        got = b''
        while len(got) < size:
            _, d = recv(DATA_PORT)
            got += d
        _, r2 = recv(REPLY_PORT)
        return r2[1], info, got

    def open(self, name, must_exist, read_only):
        cc, rc, d = self.req(6, bytes([must_exist, read_only]) + name.encode() + b'\r')
        return rc, (d[0] if rc == 0 else d)

    def close(self, h):
        return self.req(7, b'', urd=h)[1]

    def bput(self, h, b, seq):
        send(0x99, bytes([REPLY_PORT, 9, h, b]), 0x80 | seq)
        _, r = recv(REPLY_PORT)
        return r[1]

    def bget(self, h, seq):
        send(0x99, bytes([REPLY_PORT, 8, h]), 0x80 | seq)
        _, r = recv(REPLY_PORT)
        return r[1], r[2:]

    def examine(self, path, arg=1, start=0, n=20):
        return self.req(3, bytes([arg, start, n]) + path.encode() + b'\r')

def err(rc, d):
    return "error &%02X %r" % (rc, bytes(d).split(b'\r')[0])

def txt(d):
    return bytes(d).replace(b'\r', b'\n').replace(b'\x80', b'').decode('latin1').strip()

# --------------------------------------------------------------------------
results = []
def check(name, ok, detail=''):
    results.append(ok)
    print(("PASS " if ok else "FAIL ") + name + (" -- " + detail if detail else ""))

def run_tests():
    c = Client()
    d = c.iam("SYST")
    check("I AM", True, "handles urd=%d csd=%d lib=%d opt=%d" % tuple(d[:4]))

    random.seed(1)
    data = bytes(random.randrange(256) for _ in range(3000))
    rc, d = c.save("TEST", data, 0x1900, 0x8023)
    check("SAVE 3000 bytes (3 blocks)", rc == 0, err(rc, d) if rc else '')

    rc, info, got = c.load("TEST")
    check("LOAD returns same data", rc == 0 and got == data,
          "%d bytes" % (len(got or b'')))
    load = int.from_bytes(info[0:4], 'little'); exe = int.from_bytes(info[4:8], 'little')
    check("LOAD load/exec addresses kept", (load, exe) == (0x1900, 0x8023),
          "%08X %08X access=%02X" % (load, exe, info[11]))

    cc, rc, d = c.cli("INFO TEST")
    check("*INFO TEST", rc == 0, txt(d))
    check("default access WR/r", b'WR/r' in bytes(d))

    cc, rc, d = c.cli("ACCESS TEST LR/")
    check("*ACCESS TEST LR/", rc == 0, err(rc, d) if rc else '')
    cc, rc, d = c.cli("INFO TEST")
    check("*INFO shows LR/ and same addresses",
          rc == 0 and b'LR/' in bytes(d) and b'00001900' in bytes(d), txt(d))

    cc, rc, d = c.cli("DELETE TEST")
    check("*DELETE locked file refused", rc != 0, err(rc, d))

    cc, rc, d = c.cli("ACCESS TEST WR/R")
    check("*ACCESS TEST WR/R", rc == 0, err(rc, d) if rc else '')

    cc, rc, d = c.cli("RENAME TEST NEWNAME")
    check("*RENAME TEST NEWNAME", rc == 0, err(rc, d) if rc else '')
    cc, rc, d = c.cli("INFO NEWNAME")
    check("*INFO NEWNAME kept metadata",
          rc == 0 and b'00001900' in bytes(d) and b'WR/r' in bytes(d), txt(d))
    rc, info, got = c.load("NEWNAME")
    check("LOAD NEWNAME same data", rc == 0 and got == data)

    # OPENOUT / BPUT / BGET with sequence numbers
    rc, h = c.open("BFILE", 0, 0)
    check("OPENOUT BFILE", rc == 0, "handle %r" % h)
    seq = 0
    for b in b"HELLO":
        rc = c.bput(h, b, seq); seq ^= 1
        if rc: break
    check("BPUT x5", rc == 0)
    # a repeated BPUT (same sequence bit as the last) must not write again
    rc = c.bput(h, ord('O'), seq ^ 1)
    check("repeated BPUT ignored", rc == 0)
    check("CLOSE", c.close(h) == 0)

    rc, h = c.open("BFILE", 1, 1)
    check("OPENIN BFILE", rc == 0, "handle %r" % h)
    seq, out = 0, b''
    for i in range(6):
        rc, d = c.bget(h, seq)
        if rc: break
        out += bytes([d[0]]); flag = d[1]
        if i == 1:
            # repeat the previous request: same byte again
            rc, d2 = c.bget(h, seq)
            check("repeated BGET returns same byte", d2[0] == d[0],
                  "%r %r" % (chr(d[0]), chr(d2[0])))
        seq ^= 1
    check("BGET reads HELLO then EOF", out[:5] == b"HELLO", repr(out))
    c.close(h)

    # Directories
    cc, rc, d = c.cli("CDIR SUB")
    check("*CDIR SUB", rc == 0, err(rc, d) if rc else '')
    rc, d = c.save("SUB.F", b"x" * 10, 0xFFFF0E00, 0xFFFF0E00)
    check("SAVE SUB.F", rc == 0, err(rc, d) if rc else '')
    cc, rc, d = c.cli("DELETE SUB")
    check("*DELETE non-empty dir refused", rc != 0, err(rc, d))
    cc, rc, d = c.examine("")
    check("EXAMINE $", rc == 0, txt(d[2:]) if rc == 0 else err(rc, d))
    listing = bytes(d)
    check("listing shows files, hides .aundmeta",
          b'NEWNAME' in listing and b'SUB' in listing and b'aundmeta' not in listing)
    cc, rc, d = c.cli("DELETE SUB.F")
    check("*DELETE SUB.F", rc == 0, err(rc, d) if rc else '')
    cc, rc, d = c.cli("DELETE SUB")
    check("*DELETE SUB (empty but for metadata)", rc == 0, err(rc, d) if rc else '')
    cc, rc, d = c.cli("INFO SUB")
    check("SUB gone", rc != 0, err(rc, d))

    rc, info, got = c.load("NOSUCH")
    check("LOAD missing file errors", rc != 0, err(rc, info))

    cc, rc, d = c.cli("FREE")
    print("     (*FREE ->", err(rc, d) if rc else txt(d), ")")

    print("%d/%d passed" % (sum(results), len(results)))
    return all(results)

if __name__ == '__main__':
    sys.exit(0 if run_tests() else 1)
