#include "kibble/cli/terminal.h"
#include "kibble/platform/platform.h"

#include <algorithm>
#include <charconv>
#include <csignal>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <utility>

#if defined(K_PLATFORM_LINUX)
#include <poll.h>
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#elif defined(K_PLATFORM_WINDOWS)
#if defined(K_COMPILER_CLANG)
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wunknown-pragmas"
#endif
#include <conio.h>
#include <io.h>
#include <windows.h>
#if defined(K_COMPILER_CLANG)
#pragma clang diagnostic pop
#endif
#endif

namespace kb
{
namespace cli
{

namespace
{
/// @internal @brief Set to 1 when the user asks the program to stop
volatile std::sig_atomic_t s_interrupted = 0;

/// @internal @brief Signal handler: set the interrupt flag
void on_signal(int)
{
    s_interrupted = 1;
}

/// @internal @brief Type of a signal handler
using SignalHandler = void (*)(int);

/// @internal @brief Escape codes to enter full screen: alternate screen, hide cursor, clear screen
constexpr const char* k_enter_fullscreen = "\x1b[?1049h\x1b[?25l\x1b[2J\x1b[H";

/// @internal @brief Escape codes to leave full screen: reset style, show cursor, main screen
constexpr const char* k_leave_fullscreen = "\x1b[0m\x1b[?25h\x1b[?1049l";
} // namespace

Size get_terminal_size()
{
    Size size; // Default is 80x24 if the OS cannot tell

#if defined(K_PLATFORM_LINUX)

    winsize ws{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col > 0 && ws.ws_row > 0)
    {
        size.cols = int(ws.ws_col);
        size.rows = int(ws.ws_row);
    }

#elif defined(K_PLATFORM_WINDOWS)

    CONSOLE_SCREEN_BUFFER_INFO csbi;
    if (GetConsoleScreenBufferInfo(GetStdHandle(STD_OUTPUT_HANDLE), &csbi))
    {
        size.cols = csbi.srWindow.Right - csbi.srWindow.Left + 1;
        size.rows = csbi.srWindow.Bottom - csbi.srWindow.Top + 1;
    }

#else
#error get_terminal_size() not implemented for this OS
#endif

    return size;
}

void enable_terminal_ANSI_support()
{
#if defined(K_PLATFORM_WINDOWS)
    // Enable ANSI escape codes
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    DWORD dwMode = 0;
    GetConsoleMode(hOut, &dwMode);
    dwMode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, dwMode);
#endif
}

bool is_interactive()
{
#if defined(K_PLATFORM_LINUX)
    const char* term = std::getenv("TERM");
    return isatty(STDIN_FILENO) && isatty(STDOUT_FILENO) && !(term && std::string_view(term) == "dumb");
#elif defined(K_PLATFORM_WINDOWS)
    return _isatty(_fileno(stdin)) && _isatty(_fileno(stdout));
#else
#error is_interactive() not implemented for this OS
#endif
}

int poll_key()
{
#if defined(K_PLATFORM_LINUX)
    pollfd pfd{STDIN_FILENO, POLLIN, 0};
    unsigned char c = 0;
    if (poll(&pfd, 1, 0) > 0 && (pfd.revents & POLLIN) && read(STDIN_FILENO, &c, 1) == 1)
    {
        return int(c);
    }
    return -1;
#elif defined(K_PLATFORM_WINDOWS)
    if (_kbhit())
    {
        const int c = _getch();
        if (c == 0 || c == 224) // Function or arrow key: a second code follows
        {
            _getch();
            return -1;
        }
        return c;
    }
    return -1;
#else
#error poll_key() not implemented for this OS
#endif
}

void write_stdout(std::string_view text)
{
    std::fwrite(text.data(), 1, text.size(), stdout);
    std::fflush(stdout);
}

// ---------------------------------------------------------------------------------------------------------------------

struct FullscreenSession::State
{
    SignalHandler previous_sigint{SIG_DFL};  ///< Handler to restore for SIGINT
    SignalHandler previous_sigterm{SIG_DFL}; ///< Handler to restore for SIGTERM
#if defined(K_PLATFORM_LINUX)
    termios saved_mode{}; ///< Terminal mode to restore
#elif defined(K_PLATFORM_WINDOWS)
    HANDLE out{};        ///< Handle of the console output
    DWORD saved_mode{0}; ///< Console output mode to restore
#endif
};

FullscreenSession::FullscreenSession() : state_(std::make_unique<State>())
{
    s_interrupted = 0;

#if defined(K_PLATFORM_LINUX)
    tcgetattr(STDIN_FILENO, &state_->saved_mode);
    termios raw = state_->saved_mode;
    raw.c_lflag &= ~tcflag_t(ICANON | ECHO); // Disable line buffering and echo. Keep ISIG so Ctrl-C raises SIGINT
    raw.c_cc[VMIN] = 0;
    raw.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &raw);
#elif defined(K_PLATFORM_WINDOWS)
    state_->out = GetStdHandle(STD_OUTPUT_HANDLE);
    GetConsoleMode(state_->out, &state_->saved_mode);
    SetConsoleMode(state_->out, state_->saved_mode | ENABLE_VIRTUAL_TERMINAL_PROCESSING);
#endif

    state_->previous_sigint = std::signal(SIGINT, on_signal);
    state_->previous_sigterm = std::signal(SIGTERM, on_signal);
    write_stdout(k_enter_fullscreen);
}

FullscreenSession::~FullscreenSession()
{
    write_stdout(k_leave_fullscreen);

#if defined(K_PLATFORM_LINUX)
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &state_->saved_mode);
#elif defined(K_PLATFORM_WINDOWS)
    SetConsoleMode(state_->out, state_->saved_mode);
#endif

    std::signal(SIGINT, state_->previous_sigint == SIG_ERR ? SIG_DFL : state_->previous_sigint);
    std::signal(SIGTERM, state_->previous_sigterm == SIG_ERR ? SIG_DFL : state_->previous_sigterm);
}

bool FullscreenSession::interrupted()
{
    return s_interrupted != 0;
}

// ---------------------------------------------------------------------------------------------------------------------

namespace
{
/// @internal @brief Append a non-negative integer, in decimal
void append_number(std::string& out, int value)
{
    char buf[16];
    const auto result = std::to_chars(buf, buf + sizeof(buf), value);
    out.append(buf, size_t(result.ptr - buf));
}

/// @internal @brief SGR code of a color, 0 for the default color. `base` is 30 for foregrounds, 40 for backgrounds
int sgr_code(Color color, int base)
{
    const int c = int(color);
    if (c == 0)
    {
        return 0;
    }
    return c <= 8 ? base + (c - 1) : base + 60 + (c - 9); // 30-37 / 90-97 (and 40-47 / 100-107)
}

/// @internal @brief Append the escape code that switches to a style (it starts with a reset)
void append_sgr(std::string& out, Style style)
{
    out += "\x1b[0";
    if (style.attrs & Style::Bold)
    {
        out += ";1";
    }
    if (style.attrs & Style::Dim)
    {
        out += ";2";
    }
    if (style.attrs & Style::Underline)
    {
        out += ";4";
    }
    if (const int fg = sgr_code(style.fg, 30))
    {
        out += ';';
        append_number(out, fg);
    }
    if (const int bg = sgr_code(style.bg, 40))
    {
        out += ';';
        append_number(out, bg);
    }
    out += 'm';
}

/// @internal @brief Printable ASCII only: one char is one column, and no cell can inject an escape code
char printable(char c)
{
    return (c >= 32 && c < 127) ? c : '?';
}

/// @internal @brief Append a row of cells as text. Trailing blanks are dropped (the caller erases the rest of the line)
void encode_row(std::string& out, const Cell* cells, int width, bool color)
{
    const auto blank = [color](const Cell& c) { return c.ch == ' ' && (!color || c.style.is_default()); };

    int count = width;
    while (count > 0 && blank(cells[count - 1]))
    {
        --count;
    }

    Style current; // What the terminal is using right now: the default style, as each row starts after a reset
    for (int i = 0; i < count; ++i)
    {
        if (color && cells[i].style != current)
        {
            append_sgr(out, cells[i].style);
            current = cells[i].style;
        }
        out += cells[i].ch;
    }
    if (!current.is_default())
    {
        out += "\x1b[0m";
    }
}
} // namespace

// ---------------------------------------------------------------------------------------------------------------------

View View::sub(int x, int y, int w, int h) const
{
    const int x0 = std::clamp(x, 0, w_);
    const int y0 = std::clamp(y, 0, h_);
    const int x1 = std::clamp(x + std::max(w, 0), x0, w_);
    const int y1 = std::clamp(y + std::max(h, 0), y0, h_);
    return View(origin_ + std::ptrdiff_t(y0) * stride_ + x0, stride_, x1 - x0, y1 - y0);
}

void View::set(int x, int y, char ch, Style style) const
{
    if (x >= 0 && x < w_ && y >= 0 && y < h_)
    {
        origin_[std::ptrdiff_t(y) * stride_ + x] = Cell{printable(ch), style};
    }
}

void View::put(int x, int y, std::string_view text, Style style) const
{
    if (y < 0 || y >= h_ || x >= w_)
    {
        return;
    }
    Cell* row = origin_ + std::ptrdiff_t(y) * stride_;
    const size_t skip = x < 0 ? size_t(-x) : 0; // Characters that fall at the left of the view
    const size_t room = size_t(w_ - std::max(x, 0));
    for (size_t i = skip; i < text.size() && i - skip < room; ++i)
    {
        row[size_t(std::max(x, 0)) + (i - skip)] = Cell{printable(text[i]), style};
    }
}

void View::clear() const
{
    for (int y = 0; y < h_; ++y)
    {
        Cell* row = origin_ + std::ptrdiff_t(y) * stride_;
        std::fill(row, row + w_, Cell{});
    }
}

RowWriter View::row(int y) const
{
    return RowWriter(*this, y);
}

// ---------------------------------------------------------------------------------------------------------------------

void Canvas::resize(Size size)
{
    size_ = {std::max(size.cols, 0), std::max(size.rows, 0)};
    cells_.assign(size_t(size_.cols) * size_t(size_.rows), Cell{}); // No allocation if the capacity is enough
}

void Canvas::clear()
{
    std::fill(cells_.begin(), cells_.end(), Cell{});
}

View Canvas::view()
{
    return View(cells_.data(), size_.cols, size_.cols, size_.rows);
}

std::string Canvas::row_text(int y, bool color) const
{
    std::string text;
    if (y >= 0 && y < size_.rows)
    {
        encode_row(text, cells_.data() + size_t(y) * size_t(size_.cols), size_.cols, color);
    }
    return text;
}

std::string truncate(std::string_view text, int columns)
{
    if (columns <= 0)
    {
        return {};
    }
    if (int(text.size()) <= columns)
    {
        return std::string(text);
    }
    return columns <= 2 ? std::string(text.substr(0, size_t(columns)))
                        : std::string(text.substr(0, size_t(columns - 2))) + "..";
}

// ---------------------------------------------------------------------------------------------------------------------

View Screen::begin_frame(Size terminal)
{
    const Size wanted{std::max(terminal.cols - 1, 0), std::max(terminal.rows, 0)}; // Never write in the last column
    if (back_.size().cols != wanted.cols || back_.size().rows != wanted.rows)
    {
        back_.resize(wanted);
    }
    else
    {
        back_.clear();
    }
    return back_.view();
}

void Screen::present(bool color)
{
    const Size size = back_.size();
    const bool full = !front_valid_ || front_.size().cols != size.cols || front_.size().rows != size.rows;

    out_.clear();
    out_ += "\x1b[?2026h"; // Begin synchronized update (terminals that do not know it ignore it)
    const size_t prefix = out_.size();
    if (full)
    {
        out_ += "\x1b[2J";
    }

    const size_t stride = size_t(size.cols);
    for (int y = 0; y < size.rows; ++y)
    {
        const Cell* next = back_.cells_.data() + size_t(y) * stride;
        if (!full)
        {
            const Cell* shown = front_.cells_.data() + size_t(y) * stride;
            if (std::equal(next, next + stride, shown))
            {
                continue; // The terminal already shows this row
            }
        }
        out_ += "\x1b[";
        append_number(out_, y + 1);
        out_ += ";1H"; // Absolute move: no newlines, so nothing can scroll
        encode_row(out_, next, size.cols, color);
        out_ += "\x1b[K"; // Erase the rest of the line
    }

    if (full || out_.size() > prefix)
    {
        out_ += "\x1b[?2026l"; // End the synchronized update
        write_stdout(out_);
    }

    std::swap(front_, back_); // What we just sent is now what the terminal shows
    front_valid_ = true;
}

} // namespace cli
} // namespace kb