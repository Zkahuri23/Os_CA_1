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
    cprintf("lapicid %d: panic: ", lapicid());
    cprintf(s);
    cprintf("\n");
    getcallerpcs(&s, pcs);
    for (i = 0; i < 10; i++)
        cprintf(" %p", pcs[i]);
    panicked = 1;
    for (;;)
        ;
}

#define BACKSPACE 0x100
#define CRTPORT 0x3d4
static ushort *crt = (ushort *)P2V(0xb8000);

static void clear_selection(void);

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

    pos = cga_get_cursor_pos();

    if (c == '\n')
        pos += 80 - pos % 80;
    else if (c == BACKSPACE)
    {
        if (pos > 0)
            --pos;
    }
    else
        crt[pos++] = (c & 0xff) | 0x0700;

    if (pos < 0 || pos > 25 * 80)
        panic("pos under/overflow");

    if ((pos / 80) >= 24)
    {
        memmove(crt, crt + 80, sizeof(crt[0]) * 23 * 80);
        pos -= 80;
        memset(crt + pos, 0, sizeof(crt[0]) * (24 * 80 - pos));
    }

    cga_set_cursor_pos(pos);
    if (c == BACKSPACE)
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
    uint r;
    uint w;
    uint e;
    uint c;
    int selecting;
    int sel_start;
    int sel_end;
} input;

#define CLIPBOARD_BUF 128
static struct
{
    char buf[CLIPBOARD_BUF];
    uint n;
} clipboard = {.n = 0};

static const char *commands[] = {
    "cat", "echo", "find_sum", "forktest", "grep", "init",
    "kill", "ln", "ls", "mkdir", "rm", "sh",
    "stressfs", "usertests", "wc", "zombie", "console"};
static int num_commands = sizeof(commands) / sizeof(commands[0]);

static struct
{
    int last_key_was_tab;
} tab_state;

#define C(x) ((x) - '@')

static void
reset_tab_state(void)
{
    tab_state.last_key_was_tab = 0;
}

static int
find_lcp_len(const char *matches[], int count)
{
    if (count <= 0)
        return 0;
    if (count == 1)
        return strlen(matches[0]);

    int lcp_len = 0;
    const char *first = matches[0];

    while (1)
    {
        char c = first[lcp_len];
        if (c == '\0')
            break;

        for (int i = 1; i < count; i++)
        {
            if (matches[i][lcp_len] != c)
            {
                return lcp_len;
            }
        }
        lcp_len++;
    }
    return lcp_len;
}

static void
print_matches_and_redraw(const char *matches[], int count)
{
    int original_locking = cons.locking;
    cons.locking = 0;

    cprintf("\n");

    for (int i = 0; i < count; i++)
    {
        cprintf("%s  ", matches[i]);
    }

    cprintf("\n");

    cprintf("$ ");

    if (input.e != input.w)
    {
        int curpos = cga_get_cursor_pos();
        int move_right = (input.e - input.c);
        cga_set_cursor_pos(curpos + move_right);
        input.c = input.e;

        while (input.e != input.w)
        {
            input.e--;
            input.c--;
            consputc(BACKSPACE);
        }
    }
    cprintf(" ");
    input.c = input.w;
    undo.n = 0;

    cons.locking = original_locking;
}

static void
handle_tab_completion(void)
{
    char prefix[INPUT_BUF];
    int len = input.e - input.w;

    int is_first_word = 1;
    for (int i = input.w; i < input.e; i++)
    {
        if (input.buf[i % INPUT_BUF] == ' ')
        {
            is_first_word = 0;
            break;
        }
    }
    if (!is_first_word || len < 0 || len >= INPUT_BUF)
    {
        reset_tab_state();
        return;
    }

    memmove(prefix, input.buf + (input.w % INPUT_BUF), len);
    prefix[len] = '\0';

    const char *matches[sizeof(commands) / sizeof(commands[0])];
    int match_count = 0;
    for (int i = 0; i < num_commands; i++)
    {
        if (strncmp(prefix, commands[i], len) == 0)
        {
            matches[match_count++] = commands[i];
        }
    }

    if (match_count == 0)
    {
        reset_tab_state();
        return;
    }

    if (match_count == 1)
    {
        const char *completion = matches[0];
        int comp_len = strlen(completion);

        for (int i = len; i < comp_len; i++)
        {
            if (input.e >= input.r + INPUT_BUF)
                break;
            input.buf[input.e++ % INPUT_BUF] = completion[i];
            consputc(completion[i]);
        }
        input.c = input.e;
        reset_tab_state();
        return;
    }

    if (tab_state.last_key_was_tab)
    {
        print_matches_and_redraw(matches, match_count);
        reset_tab_state();
    }
    else
    {
        int lcp_len = find_lcp_len(matches, match_count);
        if (lcp_len > len)
        {
            for (int i = len; i < lcp_len; i++)
            {
                if (input.e >= input.r + INPUT_BUF)
                    break;
                input.buf[input.e++ % INPUT_BUF] = matches[0][i];
                consputc(matches[0][i]);
            }
            input.c = input.e;
        }
        tab_state.last_key_was_tab = 1;
    }
}

static int
is_whitespace(char c)
{
    return c == ' ' || c == '\t' || c == '\n' || c == '\v';
}

static void
update_highlight(int start, int end, int on)
{
    if (start < 0 || end <= start)
        return;

    if (start < (int)input.w)
        start = input.w;
    if (end > (int)input.e)
        end = input.e;
    if (start >= end)
        return;

    int cursor_pos = cga_get_cursor_pos();
    int screen_pos_of_w = cursor_pos - (int)(input.c - input.w);

    ushort normal_attr = 0x0700;
    ushort highlight_attr = 0x7000;

    for (int i = start; i < end; i++)
    {
        int screen_pos = screen_pos_of_w + (i - (int)input.w);
        if (screen_pos >= 0 && screen_pos < 25 * 80)
        {
            ushort ch = crt[screen_pos] & 0x00ff;
            crt[screen_pos] = ch | (on ? highlight_attr : normal_attr);
        }
    }
}

static void
delete_selection(void)
{
    if (input.sel_start == -1 || input.sel_end == -1)
        return;

    int s = input.sel_start;
    int e = input.sel_end;
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
    if (s >= e)
    {
        clear_selection();
        return;
    }

    int len = e - s;
    int old_e = input.e;
    int old_c = input.c;

    for (int k = 0; k < len && undo.n < UNDO_BUF; k++)
    {
        undo.buf[undo.n].type = OP_DELETE;
        undo.buf[undo.n].c = input.buf[(s + k) % INPUT_BUF];
        undo.buf[undo.n].pos = s + k;
        undo.n++;
    }

    int hw_cursor = cga_get_cursor_pos();
    int screen_pos_of_w = hw_cursor - (old_c - (int)input.w);

    if (screen_pos_of_w < 0)
        screen_pos_of_w = 0;
    if (screen_pos_of_w >= 25 * 80)
        screen_pos_of_w = 25 * 80 - 1;

    for (int i = e; i < old_e; i++)
        input.buf[(i - len) % INPUT_BUF] = input.buf[i % INPUT_BUF];

    input.e -= len;
    input.c = s;

    cga_set_cursor_pos(screen_pos_of_w);
    for (int i = input.w; i < input.e; i++)
        consputc(input.buf[i % INPUT_BUF]);

    int leftover = old_e - input.e;
    for (int i = 0; i < leftover; i++)
        consputc(' ');

    cga_set_cursor_pos(screen_pos_of_w + (input.c - input.w));

    clear_selection();
}

static void
clear_selection(void)
{
    if (input.sel_start != -1)
    {
        update_highlight(input.sel_start, input.sel_end, 0);
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
        case '\t':
            deselect_if_any();
            handle_tab_completion();
            break;
        case C('S'):
            if (input.selecting == 0)
            {
                clear_selection();
                input.selecting = 1;
                input.sel_start = input.c;
                input.sel_end = -1;
            }
            else
            {
                input.selecting = 0;
                input.sel_end = input.c;

                if (input.sel_start > input.sel_end)
                {
                    int tmp = input.sel_start;
                    input.sel_start = input.sel_end;
                    input.sel_end = tmp;
                }
                if (input.sel_start == input.sel_end)
                {
                    input.sel_start = -1;
                    input.sel_end = -1;
                }
                else
                {
                    update_highlight(input.sel_start, input.sel_end, 1);
                }
            }
            break;
        case C('C'):
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
                clear_selection();
                clipboard.n = 0;
            }
            break;

        case C('V'):
            if (clipboard.n > 0)
            {
                if (input.sel_start != -1 && input.sel_end != -1)
                {
                    delete_selection();
                }
                for (uint i = 0; i < clipboard.n; i++)
                {
                    int ch = clipboard.buf[i];

                    if (input.e < input.r + INPUT_BUF)
                    {
                        if (undo.n < UNDO_BUF)
                        {
                            undo.buf[undo.n].type = OP_INSERT;
                            undo.buf[undo.n].pos = input.c;
                            undo.buf[undo.n].c = ch;
                            undo.n++;
                        }

                        for (int j = input.e; j > input.c; j--)
                            input.buf[j % INPUT_BUF] = input.buf[(j - 1) % INPUT_BUF];
                        input.buf[input.c % INPUT_BUF] = ch;
                        input.e++;
                        input.c++;

                        for (int j = input.c - 1; j < input.e; j++)
                            consputc(input.buf[j % INPUT_BUF]);
                        cga_set_cursor_pos(cga_get_cursor_pos() - (input.e - input.c));
                    }
                }
            }

            deselect_if_any();
            clear_selection();
            break;

        case C('A'):
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
        case C('D'):
            deselect_if_any();
            if (input.e == input.w)
            {
                input.buf[input.e++ % INPUT_BUF] = C('D');
                input.w = input.e;
                input.c = input.w;
                wakeup(&input.r);
            }
            else if (input.c < input.e)
            {
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
        case C('P'):
            deselect_if_any();
            doprocdump = 1;
            break;
        case C('U'):
            deselect_if_any();
            if (input.e != input.w)
            {
                int curpos = cga_get_cursor_pos();
                int move_right = (input.e - input.c);
                cga_set_cursor_pos(curpos + move_right);
                input.c = input.e;

                while (input.e != input.w)
                {
                    input.e--;
                    input.c--;
                    consputc(BACKSPACE);
                }

                input.c = input.w;
                undo.n = 0;
            }
            input.c = input.w;
            undo.n = 0;
            break;
        case C('H'):
        case '\x7f':
            if (input.sel_start != -1 && input.sel_end != -1)
            {
                delete_selection();
                break;
            }

            if (input.c > input.w)
            {
                if (undo.n < UNDO_BUF)
                {
                    undo.buf[undo.n].type = OP_DELETE;
                    undo.buf[undo.n].c = input.buf[(input.c - 1) % INPUT_BUF];
                    undo.buf[undo.n].pos = input.c - 1;
                    undo.n++;
                }

                int cursor_pos = cga_get_cursor_pos();
                int line_start_pos = cursor_pos - (int)(input.c - input.w);

                for (int i = input.c; i < (int)input.e; i++)
                    input.buf[(i - 1) % INPUT_BUF] = input.buf[i % INPUT_BUF];

                input.c--;
                input.e--;

                cga_set_cursor_pos(line_start_pos);
                for (int i = input.w; i < (int)input.e; i++)
                    consputc(input.buf[i % INPUT_BUF]);

                consputc(' ');

                cga_set_cursor_pos(line_start_pos + (input.c - input.w));
            }
            break;
        case C('Z'):
            deselect_if_any();
            if (undo.n > 0)
            {
                struct op last = undo.buf[--undo.n];
                int pos = last.pos;

                if (last.type == OP_INSERT)
                {
                    if (pos < input.w || pos >= input.e)
                        break;

                    for (int i = pos + 1; i < input.e; i++)
                        input.buf[(i - 1) % INPUT_BUF] = input.buf[i % INPUT_BUF];
                    input.e--;

                    int cursor_before = cga_get_cursor_pos();
                    int delta = input.c - pos;
                    cga_set_cursor_pos(cursor_before - delta);
                    for (int i = pos; i < input.e; i++)
                        consputc(input.buf[i % INPUT_BUF]);
                    consputc(' ');
                    cga_set_cursor_pos(cga_get_cursor_pos() - (input.e - pos + 1));

                    input.c = pos;
                }
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
                if (c == '\r')
                    c = '\n';

                if (input.sel_start != -1 && input.sel_end != -1)
                {
                    delete_selection();
                }

                if (c == '\n' || input.e == input.r + INPUT_BUF)
                {
                    if (c == '\n')
                        cgaputc('\n');
                    input.buf[input.e++ % INPUT_BUF] = '\n';
                    input.w = input.e;
                    input.c = input.w;
                    undo.n = 0;
                    wakeup(&input.r);
                }
                else
                {
                    if (input.e < input.r + INPUT_BUF)
                    {
                        if (undo.n < UNDO_BUF)
                        {
                            undo.buf[undo.n].type = OP_INSERT;
                            undo.buf[undo.n].pos = input.c;
                            undo.buf[undo.n].c = c;
                            undo.n++;
                        }

                        for (int i = input.e; i > (int)input.c; i--)
                            input.buf[i % INPUT_BUF] = input.buf[(i - 1) % INPUT_BUF];

                        input.buf[input.c % INPUT_BUF] = c;
                        input.e++;
                        input.c++;

                        for (int i = input.c - 1; i < (int)input.e; i++)
                            consputc(input.buf[i % INPUT_BUF]);

                        cga_set_cursor_pos(cga_get_cursor_pos() - (input.e - input.c));
                    }
                }
                clear_selection();
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
        {
            if (n < target)
            {
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
    undo.n = 0;
    reset_tab_state();

    ioapicenable(IRQ_KBD, 0);
}