# Tests for the FileStick (femto) build

These run the femto build of aund in the FileStick emulator and talk to it
over the emulator's BeebEm-style UDP Econet, as a NetFS client would.

```
./run.py                  # all tests
./run.py functional       # or just one
./run.py --keep stress    # keep the images and emulator log
```

Each test boots a fresh emulator from a temporary copy of the images: the
system binary padded to 64K, init, and the FileStick tree's
`emulator/fatfs.img` (whose `boot.rc` sets station 25) with `aund.elf` and an
empty `/aund` added. `run.py` types `aund.elf` at the shell prompt, then runs
the test client. See `./run.py --help` for using other trees or images.

Needs: the FileStick tree built (`build/`), the emulator built, aund built
with `AUND_BUILD=femto` (`build-femto/aund.elf`), Python 3 and mtools.

| Test | |
|---|---|
| `functional.py` | `*I AM`, SAVE/LOAD, load and exec addresses, `*INFO`, `*ACCESS`, locked files, `*RENAME`, OPENOUT/OPENIN with BPUT/BGET (including repeated requests, which need the system's `ECONET_GET_RXFLAG`), `*CDIR`, `*DELETE` of files and directories, EXAMINE, errors. Station 0.50. |
| `stress.py` | 30 files in a directory and listing them, a 30K file, and station 0.51 sending a request while 0.50 is in the middle of a SAVE. |

The clients answer Econet handshakes in real time, so the emulator has to run
at full speed: don't pass `-v`, which slows it down about four times. The
tests use UDP ports 20025, 20050 and 20051 on 127.0.0.1.

The flash filesystem is small (about 120K free, in 2K clusters), which is why
`stress.py` uses 30 files.
