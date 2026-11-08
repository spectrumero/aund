#!/usr/bin/env python3
"""Test the femto (FileStick) build of aund in the FileStick emulator.

For each test, this makes a fresh flash image in a temporary directory
(the system binary padded to 64K, init, and a copy of the emulator's
fatfs.img with aund.elf and an empty /aund directory added), boots the
emulator on a private Econet, types aund.elf at the shell prompt, and
runs the test client against it.

    ./run.py                    run all the tests
    ./run.py functional         run one (functional or stress)
    ./run.py --keep stress      keep the work directory afterwards

The tests need the FileStick tree built (boot, system, init), the
emulator built, aund built with AUND_BUILD=femto, and mtools.
"""
import argparse, os, shutil, subprocess, sys, tempfile, threading, time

HERE = os.path.dirname(os.path.abspath(__file__))
AUND = os.path.normpath(os.path.join(HERE, '..', '..'))
TESTS = ['functional', 'stress']

# The private network: the FileStick is 0.25, the test clients 0.50 and
# 0.51 (their UDP ports are fixed in the test scripts).
ECONET_CFG = """\
0 25 127.0.0.1 20025
0 50 127.0.0.1 20050
0 51 127.0.0.1 20051
"""

def prepare(work, fs, aund_elf, fatfs):
    system = os.path.join(work, 'system.bin')
    shutil.copy(os.path.join(fs, 'build/system/filestick-system.bin'), system)
    # The boot loader copies 64K; padding with zeros keeps .bss clear
    with open(system, 'r+b') as f:
        f.truncate(64 * 1024)
    img = os.path.join(work, 'fatfs.img')
    shutil.copy(fatfs, img)
    subprocess.run(['mcopy', '-o', '-i', img, aund_elf, '::/aund.elf'], check=True)
    subprocess.run(['mmd', '-i', img, '::/aund'], check=True)
    with open(os.path.join(work, 'econet.cfg'), 'w') as f:
        f.write(ECONET_CFG)

def run_test(test, work, fs, emu_args):
    uart = os.path.join(work, 'uart')
    for f in ('flash.img', 'uart'):
        try: os.unlink(os.path.join(work, f))
        except FileNotFoundError: pass

    emu = subprocess.Popen([os.path.join(fs, 'emulator/filestick-emu'),
        '-b', os.path.join(fs, 'build/boot/filestick-boot.bin'),
        '-f', os.path.join(work, 'flash.img'),
        '-l', '0x20000:' + os.path.join(work, 'system.bin'),
        '-l', '0x30000:' + os.path.join(fs, 'build/init/init.elf'),
        '-l', '0x40000:' + os.path.join(work, 'fatfs.img'),
        '-e', os.path.join(work, 'econet.cfg'), '-u', uart] + emu_args,
        stdout=subprocess.PIPE, stderr=subprocess.STDOUT)

    console = bytearray()
    lock = threading.Lock()
    for _ in range(100):
        if os.path.exists(uart): break
        time.sleep(0.05)
    fd = os.open(uart, os.O_RDWR | os.O_NOCTTY)

    def reader():
        while True:
            try:
                d = os.read(fd, 1024)
            except OSError:
                return
            if not d: return
            with lock: console.extend(d)
    threading.Thread(target=reader, daemon=True).start()

    def wait_for(s, timeout):
        end = time.time() + timeout
        while time.time() < end:
            with lock:
                if s in console: return True
            time.sleep(0.05)
        return False

    ok = False
    try:
        if not wait_for(b'*> ', 30):
            print("no shell prompt")
        else:
            os.write(fd, b'aund.elf\r')
            if not wait_for(b'file server on station', 20):
                print("aund didn't start")
            else:
                r = subprocess.run([sys.executable,
                                    os.path.join(HERE, test + '.py')])
                ok = r.returncode == 0
    finally:
        time.sleep(0.5)
        with lock:
            text = console.decode('latin1')
        print("----- FileStick console -----")
        print(text[text.find('*> '):] if '*> ' in text else text)
        emu.terminate()
        out = emu.communicate(timeout=5)[0].decode('latin1')
        with open(os.path.join(work, 'emu.log'), 'w') as f:
            f.write(out)
        os.close(fd)
    return ok

def main():
    sys.stdout.reconfigure(line_buffering=True)   # in order with the tests
    ap = argparse.ArgumentParser(description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('tests', nargs='*',
        help='tests to run: %s (default: all)' % ', '.join(TESTS))
    ap.add_argument('--filestick', default=os.path.dirname(AUND),
        help='FileStick source tree (default: the parent of aund)')
    ap.add_argument('--aund', default=os.path.join(AUND, 'build-femto/aund.elf'),
        help='aund binary (default: build-femto/aund.elf)')
    ap.add_argument('--fatfs', help='flash FAT image to start from '
        '(default: emulator/fatfs.img in the FileStick tree)')
    ap.add_argument('--keep', action='store_true',
        help="keep the work directory (images, emu.log)")
    ap.add_argument('--emu-arg', action='append', default=[],
        help='extra emulator argument (not -v: it runs too slowly '
             'for the real time test clients)')
    args = ap.parse_args()
    for t in args.tests:
        if t not in TESTS:
            ap.error("unknown test %r" % t)

    fs = os.path.abspath(args.filestick)
    fatfs = args.fatfs or os.path.join(fs, 'emulator/fatfs.img')
    for f in (args.aund, fatfs, os.path.join(fs, 'emulator/filestick-emu'),
              os.path.join(fs, 'build/system/filestick-system.bin')):
        if not os.path.exists(f):
            sys.exit("missing %s: see the comment at the top of run.py" % f)

    failed = []
    for test in args.tests or TESTS:
        work = tempfile.mkdtemp(prefix='aund-femto-%s-' % test)
        print("===== %s (in %s) =====" % (test, work))
        prepare(work, fs, args.aund, fatfs)
        if not run_test(test, work, fs, args.emu_arg):
            failed.append(test)
        if args.keep:
            print("kept", work)
        else:
            shutil.rmtree(work)

    print("===== %s =====" % ("FAILED: " + ", ".join(failed) if failed
                               else "all passed"))
    sys.exit(1 if failed else 0)

if __name__ == '__main__':
    main()
