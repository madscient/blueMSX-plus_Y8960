/* Probe for the Y8960 MSX-TIMER block (romMapperY8960Timer.c): what happens
** when the resolution is lowered while a counter runs, without clearing it.
**
** The block's source is compiled into the probe with the board timers of
** opl2ex-host-stub.c, so it runs headless and the board time moves only when
** the probe says. The expected behaviour comes from the hardware's RTL
** (msx_timer_core.v): the end of a count is judged on nine bits, the eighth
** set whenever any bit above the eight-bit count field is, so a counter that
** stands above 255 after the change has passed every terminal value and ends
** at the next step of the new resolution.
**
** Build and run, from a Visual Studio command prompt:
**
**   set SRC=..\..\..\..\blueMSX\Src
**   cl /nologo /W3 /O2 /TC /D_CRT_SECURE_NO_WARNINGS /I%SRC%\Memory /I%SRC%\Board ^
**      /I%SRC%\Common /I%SRC%\Utils /I%SRC%\Media /I%SRC%\Emulator /I%SRC%\SoundChips ^
**      /I%SRC%\Debugger /I%SRC%\VideoChips ^
**      msx-timer-probe.c opl2ex-host-stub.c
**   msx-timer-probe.exe
**
** Exit status is non-zero if any check fails.
*/
#include "romMapperY8960Timer.c"

extern UInt32 y8960ProbePendingIrq;
void probeRun(UInt32 span);

void ioPortRegister(int port, IoPortRead r, IoPortWrite w, void* ref) { (void)port; (void)r; (void)w; (void)ref; }
void ioPortUnregister(int port, void* ref) { (void)port; (void)ref; }
int deviceManagerRegister(int type, DeviceCallbacks* callbacks, void* ref) { (void)type; (void)callbacks; (void)ref; return 1; }
void deviceManagerUnregister(int handle) { (void)handle; }
int y8960IoEnabled(Y8960IoBlock block) { (void)block; return 1; }
SaveState* saveStateOpenForRead(const char* fileName) { (void)fileName; return NULL; }
SaveState* saveStateOpenForWrite(const char* fileName) { (void)fileName; return NULL; }
void saveStateClose(SaveState* state) { (void)state; }

static int fails = 0;

static void check(const char* what, int ok)
{
    printf("%-62s %s\n", what, ok ? "ok" : "FAIL");
    if (!ok) fails++;
}

#define BOARD_HZ  (6 * 3579545)
/* One step at resolution 0: 2^10 counts of the 42.95MHz clock, in board
** cycles (half as many: the board runs at half that clock). */
#define STEP0     512

static void setReg(int counter, int reg, UInt8 value)
{
    write(theTimer, 0xB0, (UInt8)((counter << 2) | reg));
    write(theTimer, 0xB1, value);
}

static int flag0(void) { return read(theTimer, 0xB2) & 0x01; }

/* Counter 0 counting to 255 at resolution 7, for one second: it stands at
** about 42.95M counts, which reads 2 at resolution 7 and far above 255 at
** resolution 0. Then the resolution goes to 0, the counter left as it is. */
static void startAndLower(int repeat)
{
    romMapperY8960TimerCreate();
    y8960ProbePendingIrq = 0;
    setReg(0, 1, 0xFF);
    setReg(0, 0, (UInt8)(0x80 | 0x70 | repeat));    /* interrupt on, resolution 7 */
    setReg(0, 2, 0x03);                             /* clear and count */
    probeRun(BOARD_HZ);
    setReg(0, 0, (UInt8)(0x80 | 0x00 | repeat));    /* resolution 0, no clear */
}

int main(void)
{
    UInt32 waited;
    int before;

    /* ---- repeating counter ---- */
    startAndLower(1);
    before = flag0();
    probeRun(2 * STEP0);
    printf("  repeat: flag before the change %d, two steps after it %d, B3h reads %02X\n",
           before, flag0(), read(theTimer, 0xB3));
    check("no end before the change: 2 of 255 at resolution 7", before == 0);
    check("lowered past its terminal value, it ends within two steps", flag0() == 1);
    check("and raises the interrupt", (y8960ProbePendingIrq & BOARD_INT_Y8960_TIMER) != 0);
    check("then counts again from 0", read(theTimer, 0xB3) <= 2);

    /* The next end is a whole count away: 256 steps of resolution 0. */
    write(theTimer, 0xB2, 0x01);
    for (waited = 0; waited < BOARD_HZ / 10 && !flag0(); waited += 64) probeRun(64);
    printf("  repeat: next end after %u board cycles (256 steps are %u)\n", waited, 256 * STEP0);
    check("the next end comes after 256 steps",
          waited >= 256 * STEP0 - 2 * STEP0 && waited <= 256 * STEP0 + 2 * STEP0);
    romMapperY8960TimerDestroy();

    /* ---- one-shot counter ---- */
    startAndLower(0);
    probeRun(2 * STEP0);
    printf("  one-shot: flag %d, counting %d\n", flag0(), theTimer->core[0].countEnable);
    check("a one-shot counter ends within two steps as well", flag0() == 1);
    check("and stops counting", theTimer->core[0].countEnable == 0);
    romMapperY8960TimerDestroy();

    printf(fails ? "FAILURES PRESENT\n" : "ALL OK\n");
    return fails ? 1 : 0;
}
