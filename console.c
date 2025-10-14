// Console input and output.
// Input is from the keyboard or serial port.
// Output is written to the screen and serial port.

#include "types.h"
#include "defs.h"
#include "param.h"
#include "traps.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "fs.h"
#include "file.h"
#include "memlayout.h"
#include "mmu.h"
#include "proc.h"
#include "x86.h"
#include "kbd.h"

static void consputc(int);

static int panicked = 0;

static struct
{
    struct spinlock lock;
    int locking;
} cons;

#define UNDO_BUF 128
enum
{
    OP_INSERT,
    OP_DELETE
};
struct op
{
    char type;
    char c;
    uint pos;
};
struct
{
    struct op buf[UNDO_BUF];
    uint n;
} undo;



static void
printint(int xx, int base, int sign)
{
    static char digits[] = "0123456789abcdef";
    char buf[16];
    int i;
    uint x;

    if (sign && (sign = xx < 0))
        x = -xx;
    else
        x = xx;

    i = 0;
    do
    {
        buf[i++] = digits[x % base];
    } while ((x /= base) != 0);

    if (sign)
        buf[i++] = '-';

    while (--i >= 0)
        consputc(buf[i]);
}
// PAGEBREAK: 50

// Print to the console. only understands %d, %x, %p, %s.
void cprintf(char *fmt, ...)
{
    int i, c, locking;
    uint *argp;
    char *s;

    locking = cons.locking;
    if (locking)
        acquire(&cons.lock);

    if (fmt == 0)
        panic("null fmt");

    argp = (uint *)(void *)(&fmt + 1);
    for (i = 0; (c = fmt[i] & 0xff) != 0; i++)
    {
        if (c != '%')
        {
            consputc(c);
            continue;
        }
        c = fmt[++i] & 0xff;
        if (c == 0)
            break;
        switch (c)
        {
        case 'd':
            printint(*argp++, 10, 1);
            break;
        case 'x':
        case 'p':
            printint(*argp++, 16, 0);
            break;
        case 's':
            if ((s = (char *)*argp++) == 0)
                s = "(null)";
            for (; *s; s++)
                consputc(*s);
            break;
        case '%':
            consputc('%');
            break;
        default:
            // Print unknown % sequence to draw attention.
            consputc('%');
            consputc(c);
            break;
        }
    }

    if (locking)
        release(&cons.lock);
}

void panic(char *s)
{
    int i;
    uint pcs[10];

    cli();
    cons.locking = 0;
    // use lapiccpunum so that we can call panic from mycpu()
    cprintf("lapicid %d: panic: ", lapicid());
    cprintf(s);
    cprintf("\n");
    getcallerpcs(&s, pcs);
    for (i = 0; i < 10; i++)
        cprintf(" %p", pcs[i]);
    panicked = 1; // freeze other CPU
    for (;;)
        ;
}

// PAGEBREAK: 50
#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort *)P2V(0xb8000); // CGA memory

// NEW: Forward declaration
static void clear_selection(void);

// NEW: Helper function to get hardware cursor position.
static int
cga_get_cursor_pos(void)
{
    int pos;
    outb(CRTPORT, 14);
    pos = inb(CRTPORT + 1) << 8;
    outb(CRTPORT, 15);
    pos |= inb(CRTPORT + 1);
    return pos;
}

// NEW: Helper function to set hardware cursor position.
static void
cga_set_cursor_pos(int pos)
{
    outb(CRTPORT, 14);
    outb(CRTPORT + 1, pos >> 8);
    outb(CRTPORT, 15);
    outb(CRTPORT + 1, pos);
}

static void
cgaputc(int c)
{
    int pos;

    // Cursor position: col + 80*row.
    pos = cga_get_cursor_pos();

    if (c == '\n')
        pos += 80 - pos % 80;
    else if (c == BACKSPACE)
    {
        if (pos > 0)
            --pos;
    }
    else
        crt[pos++] = (c & 0xff) | 0x0700; // black on white

    if (pos < 0 || pos > 25 * 80)
        panic("pos under/overflow");

    if ((pos / 80) >= 24)
    { // Scroll up.
        memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
        pos -= 80;
        memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
    }

    cga_set_cursor_pos(pos);
    if (c == BACKSPACE) // Erase character at new cursor pos only for backspace.
        crt[pos] = ' ' | 0x0700;
}

void consputc(int c)
{
    if (panicked)
    {
        cli();
        for (;;)
            ;
    }

    if (c == BACKSPACE)
    {
        uartputc('\b');
        uartputc(' ');
        uartputc('\b');
    }
    else
        uartputc(c);
    cgaputc(c);
}

#define INPUT_BUF 128
struct
{
    char buf[INPUT_BUF];
    uint r;        // Read index
    uint w;        // Write index
    uint e;        // End of line index
    uint c;        // Cursor index
    int selecting; // Is a selection process active (after first Ctrl+S)?
    int sel_start; // Selection start index in buffer.
    int sel_end;   // Selection end index in buffer.
} input;

#define CLIPBOARD_BUF 128
static struct
{
    char buf[CLIPBOARD_BUF];
    uint n;
} clipboard = {.n = 0};

#define C(x) ((x) - '@') // Control-x

static int
is_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v';
}

// NEW: Helper to highlight or un-highlight text on screen
// NEW: Helper to highlight or un-highlight text on screen
static void
update_highlight(int start, int end, int on)
{
    // Validate indices and ensure start < end
    if (start < 0 || end <= start)
        return;

    // Clamp to visible buffer region
    if (start < (int)input.w)
        start = input.w;
    if (end > (int)input.e)
        end = input.e;
    if (start >= end)
        return;

    // Compute screen position of input.w (start of displayed buffer)
    // cursor_pos corresponds to input.c; so screen_pos_of_w = cursor - (input.c - input.w)
    int cursor_pos = cga_get_cursor_pos();
    int screen_pos_of_w = cursor_pos - (int)(input.c - input.w);

    // Attributes:
    // normal attribute (text drawn by consputc uses 0x0700 in this file)
    ushort normal_attr = 0x0700;
    // highlight: invert background/foreground - choose 0x7000 (0x70 in high byte)
    ushort highlight_attr = 0x7000;

    for (int i = start; i < end; i++)
    {
        int screen_pos = screen_pos_of_w + (i - (int)input.w);
        if (screen_pos >= 0 && screen_pos < 25 * 80)
        {
            ushort ch = crt[screen_pos] & 0x00ff; // keep character
            crt[screen_pos] = ch | (on ? highlight_attr : normal_attr);
        }
    }
}

// Delete the current selection and redraw the input line correctly.
// Also record deletions into undo (best-effort, up to UNDO_BUF).
static void
delete_selection(void)
{
    if (input.sel_start == -1 || input.sel_end == -1)
        return;

    // Make sure start < end
    int s = input.sel_start;
    int e = input.sel_end;
    if (s > e)
    {
        int t = s;
        s = e;
        e = t;
    }

    // Clamp to current buffer region
    if (s < (int)input.w)
        s = input.w;
    if (e > (int)input.e)
        e = input.e;
    if (s >= e)
    {
        clear_selection();
        return;
    }

    int len = e - s;
    int old_e = input.e;
    int old_c = input.c; // hardware cursor corresponds to this logical cursor

    // Record deleted chars into undo (so Ctrl+Z can restore). This is optional.
    for (int k = 0; k < len && undo.n < UNDO_BUF; k++)
    {
        undo.buf[undo.n].type = OP_DELETE;
        undo.buf[undo.n].c = input.buf[(s + k) % INPUT_BUF];
        undo.buf[undo.n].pos = s + k;
        undo.n++;
    }

    // Compute screen start position for input.w based on the *old* cursor mapping.
    // screen_pos_of_w = hw_cursor - (old_c - input.w)
    int hw_cursor = cga_get_cursor_pos();
    int screen_pos_of_w = hw_cursor - (old_c - (int)input.w);

    // Safety clamp for screen_pos_of_w
    if (screen_pos_of_w < 0)
        screen_pos_of_w = 0;
    if (screen_pos_of_w >= 25 * 80)
        screen_pos_of_w = 25 * 80 - 1;

    // Shift buffer left to delete selected text
    for (int i = e; i < old_e; i++)
        input.buf[(i - len) % INPUT_BUF] = input.buf[i % INPUT_BUF];

    input.e -= len;
    input.c = s;

    // Redraw the visible line from input.w .. input.e
    cga_set_cursor_pos(screen_pos_of_w);
    for (int i = input.w; i < input.e; i++)
        consputc(input.buf[i % INPUT_BUF]);

    // Clear leftover characters that remain from old_e
    int leftover = old_e - input.e;
    for (int i = 0; i < leftover; i++)
        consputc(' ');

    // Restore logical cursor to input.c
    cga_set_cursor_pos(screen_pos_of_w + (input.c - input.w));

    clear_selection();
}

// NEW: Clears any active selection
static void
clear_selection(void)
{
    if (input.sel_start != -1)
    {
        update_highlight(input.sel_start, input.sel_end, 0); // Turn off highlight
        input.sel_start = -1;
        input.sel_end = -1;
    }
    input.selecting = 0;
}

static void deselect_if_any(void)
{
    if (input.sel_start != -1 && input.sel_end != -1)
    {
        clear_selection();
    }
}

void consoleintr(int (*getc)(void))
{
    int c, doprocdump = 0;

    acquire(&cons.lock);
    while ((c = getc()) >= 0)
    {
        switch (c)
        {
        case C('S'): // NEW: Select text
            if (input.selecting == 0)
            {
                // First Ctrl+S: Start selection
                clear_selection(); // Clear any previous selection
                input.selecting = 1;
                input.sel_start = input.c;
                input.sel_end = -1;
            }
            else
            {
                // Second Ctrl+S: End selection
                input.selecting = 0;
                input.sel_end = input.c;

                // Ensure start is always before end
                if (input.sel_start > input.sel_end)
                {
                    int tmp = input.sel_start;
                    input.sel_start = input.sel_end;
                    input.sel_end = tmp;
                }
                // If start and end are the same, it's not a valid selection
                if (input.sel_start == input.sel_end)
                {
                    input.sel_start = -1;
                    input.sel_end = -1;
                }
                else
                {
                    update_highlight(input.sel_start, input.sel_end, 1); // Turn on highlight
                }
            }
            break;
        case C('C'): // copy
            if (input.sel_start != -1 && input.sel_end != -1)
            {
                int s = input.sel_start, e = input.sel_end;
                if (s > e)
                {
                    int t = s;
                    s = e;
                    e = t;
                }
                if (s < (int)input.w)
                    s = input.w;
                if (e > (int)input.e)
                    e = input.e;
                int len = e - s;
                if (len > CLIPBOARD_BUF)
                    len = CLIPBOARD_BUF;
                for (int i = 0; i < len; i++)
                    clipboard.buf[i] = input.buf[(s + i) % INPUT_BUF];
                clipboard.n = len;
            }
            else
            {
                // no selection — just clear any selection artifact
                clear_selection();
                clipboard.n = 0; // optionally clear clipboard if you prefer
            }
            break;

        case C('V'): // Paste clipboard content

            if (clipboard.n > 0)
            {
                if (input.sel_start != -1 && input.sel_end != -1)
                {
                    delete_selection();
                }
                for (uint i = 0; i < clipboard.n; i++)
                {
                    int ch = clipboard.buf[i];

                    // Use same insertion logic as default typing
                    if (input.e < input.r + INPUT_BUF)
                    {
                        // Log insertion for undo
                        if (undo.n < UNDO_BUF)
                        {
                            undo.buf[undo.n].type = OP_INSERT;
                            undo.buf[undo.n].pos = input.c; // position where char will be inserted
                            undo.buf[undo.n].c = ch;        // store the inserted character
                            undo.n++;
                        }

                        // Insert character
                        for (int j = input.e; j > input.c; j--)
                            input.buf[j % INPUT_BUF] = input.buf[(j - 1) % INPUT_BUF];
                        input.buf[input.c % INPUT_BUF] = ch;
                        input.e++;
                        input.c++;

                        // Redraw everything from current cursor
                        for (int j = input.c - 1; j < input.e; j++)
                            consputc(input.buf[j % INPUT_BUF]);
                        cga_set_cursor_pos(cga_get_cursor_pos() - (input.e - input.c));
                    }
                }
            }

            // After pasting, clear any selection
            deselect_if_any();
            clear_selection();
            break;

        case C('A'): // NEW: Backward-word
            deselect_if_any();
            if (input.c > input.w)
            {
                int old_c = input.c;
                int temp_c = input.c;
                temp_c--;
                while (temp_c > input.w && is_whitespace(input.buf[temp_c % INPUT_BUF]))
                    temp_c--;
                while (temp_c > input.w && !is_whitespace(input.buf[(temp_c - 1) % INPUT_BUF]))
                    temp_c--;
                input.c = temp_c;
                cga_set_cursor_pos(cga_get_cursor_pos() - (old_c - input.c));
            }
            break;
        case C('D'): // NEW: Forward-word or EOF
            deselect_if_any();
            if (input.e == input.w)
            { // Empty line -> EOF
                input.buf[input.e++ % INPUT_BUF] = C('D');
                input.w = input.e;
                input.c = input.w;
                wakeup(&input.r);
            }
            else if (input.c < input.e)
            { // Line has text -> move forward
                int old_c = input.c;
                int temp_c = input.c;
                while (temp_c < input.e && !is_whitespace(input.buf[temp_c % INPUT_BUF]))
                    temp_c++;
                while (temp_c < input.e && is_whitespace(input.buf[temp_c % INPUT_BUF]))
                    temp_c++;
                if (temp_c < input.e)
                {
                    cga_set_cursor_pos(cga_get_cursor_pos() + (temp_c - old_c));
                    input.c = temp_c;
                }
            }
            break;
        case C('P'): // Process listing.
            deselect_if_any();
            // procdump() locks cons.lock indirectly; invoke later
            doprocdump = 1;
            break;
        case C('U'): // Kill line (Ctrl+U)
            deselect_if_any();
            if (input.e != input.w)
            {
                // Move cursor to end of line for consistent backspacing
                int curpos = cga_get_cursor_pos();
                int move_right = (input.e - input.c);
                cga_set_cursor_pos(curpos + move_right);
                input.c = input.e;

                // Erase all characters on screen
                while (input.e != input.w)
                {
                    input.e--;
                    input.c--;
                    consputc(BACKSPACE);
                }

                // Reset cursor and input buffer indexes
                input.c = input.w;
                undo.n = 0; // clear undo history
            }
            break;
            input.c = input.w;
            undo.n = 0; // NEW: Clear undo history
            break;
        case C('H'):
        case '\x7f': // Backspace
            deselect_if_any();
            if (input.c > input.w)
            {
                undo.n--;
                // NEW: Log the deletion for undo.
                // if (undo.n < UNDO_BUF)
                // {
                //     undo.buf[undo.n].type = OP_DELETE;
                //     undo.buf[undo.n].c = input.buf[(input.c - 1) % INPUT_BUF];
                //     undo.buf[undo.n].pos = input.c - 1;
                //     undo.n++;
                // }
                // Shift characters left.
                for (int i = input.c; i < input.e; i++)
                    input.buf[(i - 1) % INPUT_BUF] = input.buf[i % INPUT_BUF];
                input.e--;
                input.c--;
                // Redraw on screen.
                cga_set_cursor_pos(cga_get_cursor_pos() - 1);
                for (int i = input.c; i < input.e; i++)
                    consputc(input.buf[i % INPUT_BUF]);
                consputc(' ');
                cga_set_cursor_pos(cga_get_cursor_pos() - (input.e - input.c + 1));
            }
            break;
        case C('Z'): // Undo last operation
            deselect_if_any();
            if (undo.n > 0)
            {
                struct op last = undo.buf[--undo.n];
                int pos = last.pos;

                if (last.type == OP_INSERT)
                {
                    // Undo an insertion → remove the inserted char at its original position.
                    if (pos < input.w || pos >= input.e)
                        break;

                    // Shift buffer left
                    for (int i = pos + 1; i < input.e; i++)
                        input.buf[(i - 1) % INPUT_BUF] = input.buf[i % INPUT_BUF];
                    input.e--;

                    // Redraw everything from 'pos'
                    int cursor_before = cga_get_cursor_pos();
                    int delta = input.c - pos;
                    cga_set_cursor_pos(cursor_before - delta);
                    for (int i = pos; i < input.e; i++)
                        consputc(input.buf[i % INPUT_BUF]);
                    consputc(' '); // erase leftover char
                    cga_set_cursor_pos(cga_get_cursor_pos() - (input.e - pos + 1));

                    // Restore logical cursor
                    input.c = pos;
                }
                // else if (last.type == OP_DELETE)
                // {
                //     // Undo a deletion → reinsert the deleted char at its original position.
                //     if (pos < input.w || pos > input.e)
                //         break;

                //     // Shift buffer right
                //     for (int i = input.e; i > pos; i--)
                //         input.buf[i % INPUT_BUF] = input.buf[(i - 1) % INPUT_BUF];
                //     input.buf[pos % INPUT_BUF] = last.c;
                //     input.e++;

                //     // Redraw from 'pos'
                //     int cursor_before = cga_get_cursor_pos();
                //     int delta = input.c - pos;
                //     cga_set_cursor_pos(cursor_before - delta);
                //     for (int i = pos; i < input.e; i++)
                //         consputc(input.buf[i % INPUT_BUF]);
                //     cga_set_cursor_pos(cga_get_cursor_pos() - (input.e - pos - 1));

                //     // Restore logical cursor
                //     input.c = pos + 1;
                // }
            }
            break;

        case KEY_LF:
            deselect_if_any();
            if (input.c > input.w)
            {
                input.c--;
                cga_set_cursor_pos(cga_get_cursor_pos() - 1);
            }
            break;
        case KEY_RT:
            deselect_if_any();
            if (input.c < input.e)
            {
                input.c++;
                cga_set_cursor_pos(cga_get_cursor_pos() + 1);
            }
            break;
        default:
            if (c != 0)
            {
                deselect_if_any();
                if (c == '\r')
                    c = '\n';

                if (c == '\n' || input.e == input.r + INPUT_BUF)
                {
                    if (c == '\n')
                        cgaputc('\n');
                    input.buf[input.e++ % INPUT_BUF] = '\n';
                    input.w = input.e;
                    input.c = input.w;
                    undo.n = 0; // NEW: Clear undo history
                    wakeup(&input.r);
                }
                else
                {
                    if (input.e < input.r + INPUT_BUF)
                    {
                        // NEW: Log insertion for undo (store the character too)
                        if (undo.n < UNDO_BUF)
                        {
                            undo.buf[undo.n].type = OP_INSERT;
                            undo.buf[undo.n].pos = input.c; // position where char will be inserted
                            undo.buf[undo.n].c = c;         // store the inserted character
                            undo.n++;
                        }
                        // Insert character
                        for (int i = input.e; i > input.c; i--)
                            input.buf[i % INPUT_BUF] = input.buf[(i - 1) % INPUT_BUF];
                        input.buf[input.c % INPUT_BUF] = c;
                        input.e++;
                        input.c++;
                        // Redraw
                        for (int i = input.c - 1; i < input.e; i++)
                            consputc(input.buf[i % INPUT_BUF]);
                        cga_set_cursor_pos(cga_get_cursor_pos() - (input.e - input.c));
                    }
                }
            }
            break;
        }
    }
    release(&cons.lock);
    if (doprocdump)
    {
        procdump();
    }
}

int consoleread(struct inode *ip, char *dst, int n)
{
    uint target;
    int c;

    iunlock(ip);
    target = n;
    acquire(&cons.lock);
    while (n > 0)
    {
        while (input.r == input.w)
        {
            if (myproc()->killed)
            {
                release(&cons.lock);
                ilock(ip);
                return -1;
            }
            sleep(&input.r, &cons.lock);
        }
        c = input.buf[input.r++ % INPUT_BUF];
        if (c == C('D'))
        { // EOF
            if (n < target)
            {
                // Save ^D for next time, to make sure
                // caller gets a 0-byte result.
                input.r--;
            }
            break;
        }
        *dst++ = c;
        --n;
        if (c == '\n')
            break;
    }
    release(&cons.lock);
    ilock(ip);

    return target - n;
}

int consolewrite(struct inode *ip, char *buf, int n)
{
    int i;

    iunlock(ip);
    acquire(&cons.lock);
    for (i = 0; i < n; i++)
        consputc(buf[i] & 0xff);
    release(&cons.lock);
    ilock(ip);

    return n;
}

void consoleinit(void)
{
    initlock(&cons.lock, "console");

    devsw[CONSOLE].write = consolewrite;
    devsw[CONSOLE].read = consoleread;
    cons.locking = 1;

    input.r = input.w = input.e = input.c = 0;
    undo.n = 0; // NEW: Initialize undo buffer

    ioapicenable(IRQ_KBD, 0);
}
