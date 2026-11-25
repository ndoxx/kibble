#pragma once
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace kb
{
namespace cli
{

/// @brief Dimensions of a terminal, in characters
struct Size
{
    /*
        NOTE(ndx):
        The members are signed on purpose. Layout code subtracts from them (cols - 1, rows - 2).
        An unsigned wrap-around would hide the problem when the window is very small.
    */

    int cols{80}; ///< Number of columns
    int rows{24}; ///< Number of rows
};

/**
 * @brief [OS-dependent] Retrieve the number of columns and rows in the terminal
 *
 * @return Size the dimensions of the terminal, or 80x24 if the OS cannot tell (output is not a terminal, redirected...)
 */
Size get_terminal_size();

/// @brief [OS-dependent] Enable ANSI escape codes in a Windows terminal. Does nothing on other platforms
void enable_terminal_ANSI_support();

/**
 * @brief Tell if a person is at the other end of the terminal
 *
 * Returns true if stdin and stdout are terminals and the terminal is not "dumb" (it can do cursor addressing). Check
 * this before you start a FullscreenSession.
 */
bool is_interactive();

/**
 * @brief [OS-dependent] Retrieve the next key pressed, without waiting
 *
 * Call this in a loop, typically inside a FullscreenSession. The session delivers keys immediately, without Enter and
 * without echo.
 *
 * @return int the key as a character code, or -1 if no key is waiting. Special keys (arrows...) are delivered as the
 * bytes of their escape sequence on Linux, and ignored on Windows.
 */
int poll_key();

/// @brief Write text to stdout and flush
void write_stdout(std::string_view text);

/**
 * @brief RAII: take the terminal over for a full-screen program, and give it back on destruction
 *
 * While the session exists:
 * - the terminal shows the alternate screen (the scrollback of the user comes back on exit)
 * - the cursor is hidden
 * - keys are delivered immediately and are not echoed
 * - ANSI escape codes are enabled on Windows
 *
 * Ctrl-C and SIGTERM do not stop the program. They set a flag (see interrupted()). The main loop must check the flag
 * and leave, so that the destructor runs and restores the terminal. Only one session can exist at a time.
 */
class FullscreenSession
{
public:
    /// @brief Start the session: set up the terminal and the signal handlers
    FullscreenSession();

    /// @brief End the session: restore the terminal and the signal handlers
    ~FullscreenSession();

    FullscreenSession(const FullscreenSession&) = delete;
    FullscreenSession& operator=(const FullscreenSession&) = delete;

    /// @brief Check if the user asked the program to stop (Ctrl-C, SIGTERM) since the session started
    static bool interrupted();

private:
    /// @internal @brief OS-specific data that the session saves and restores
    struct State;

    std::unique_ptr<State> state_; ///< @internal OS-specific saved state
};

/// @brief The 16 standard ANSI colors. `Default` is the color the terminal uses when nothing is specified
enum class Color : std::uint8_t
{
    Default = 0,
    Black,
    Red,
    Green,
    Yellow,
    Blue,
    Magenta,
    Cyan,
    White,
    BrightBlack,
    BrightRed,
    BrightGreen,
    BrightYellow,
    BrightBlue,
    BrightMagenta,
    BrightCyan,
    BrightWhite
};

/**
 * @brief How a character is drawn: foreground color, background color, attributes
 *
 * A style is a plain 3-byte value, cheap to copy and to compare. It becomes an SGR escape code only when the screen is
 * sent to the terminal. The default style (all zeros) means "whatever the terminal does".
 */
struct Style
{
    /// @brief Text attributes, combined with `|`
    enum Attr : std::uint8_t
    {
        Bold = 1,
        Dim = 2,
        Underline = 4
    };

    Color fg{Color::Default}; ///< Foreground color
    Color bg{Color::Default}; ///< Background color
    std::uint8_t attrs{0};    ///< Combination of Attr flags

    constexpr Style() = default;
    constexpr Style(Color foreground, Color background = Color::Default, std::uint8_t attributes = 0)
        : fg(foreground), bg(background), attrs(attributes)
    {
    }

    /// @brief Get a copy of this style with the bold attribute set (or cleared, if `on` is false)
    constexpr Style with_bold(bool on = true) const
    {
        return Style(fg, bg, std::uint8_t(on ? (attrs | Bold) : (attrs & ~unsigned(Bold))));
    }

    /// @brief Check if this is the default style
    constexpr bool is_default() const
    {
        return fg == Color::Default && bg == Color::Default && attrs == 0;
    }
};

constexpr bool operator==(const Style& a, const Style& b)
{
    return a.fg == b.fg && a.bg == b.bg && a.attrs == b.attrs;
}

constexpr bool operator!=(const Style& a, const Style& b)
{
    return !(a == b);
}

/// @brief One character position of the screen: a character and its style. One char is one column: use ASCII only
struct Cell
{
    char ch{' '}; ///< Character
    Style style;  ///< How it is drawn
};

constexpr bool operator==(const Cell& a, const Cell& b)
{
    return a.ch == b.ch && a.style == b.style;
}

constexpr bool operator!=(const Cell& a, const Cell& b)
{
    return !(a == b);
}

class RowWriter;

/**
 * @brief A non-owning, clipped rectangle of cells (a window on a Canvas)
 *
 * A View is a small handle (a pointer and a few ints), like a std::span: copy it freely, pass it by value or by const
 * reference. Its coordinates start at (0, 0) in its own top-left corner, so a panel can be drawn without knowing where
 * it will be placed. Everything that falls outside of the view is silently clipped, which makes layout code safe in
 * very small terminals. Characters that are not printable ASCII are replaced by '?'.
 *
 * A View is only valid as long as the Canvas it comes from is not resized.
 */
class View
{
public:
    /// @brief An empty view: everything drawn into it is dropped
    View() = default;

    /// @brief Get the width of the view, in columns
    int width() const
    {
        return w_;
    }

    /// @brief Get the height of the view, in rows
    int height() const
    {
        return h_;
    }

    /// @brief Get the dimensions of the view
    Size size() const
    {
        return {w_, h_};
    }

    /// @brief Get a view on a rectangle of this view, in the coordinates of this view. The rectangle is clipped
    View sub(int x, int y, int w, int h) const;

    /// @brief Set one cell. Does nothing if it is outside of the view
    void set(int x, int y, char ch, Style style = {}) const;

    /// @brief Write text from (x, y), going right. What does not fit is cut, without "..": see truncate()
    void put(int x, int y, std::string_view text, Style style = {}) const;

    /// @brief Reset all the cells of the view to blank, default style
    void clear() const;

    /// @brief Get a cursor that writes at the start of row `y`, to chain runs: `v.row(2).add("a", s).add("b")`
    RowWriter row(int y) const;

private:
    friend class Canvas;
    View(Cell* origin, int stride, int w, int h) : origin_(origin), stride_(stride), w_(w), h_(h)
    {
    }

    Cell* origin_{nullptr}; ///< @internal Top-left cell of the view
    int stride_{0};         ///< @internal Distance, in cells, between a row and the next one (width of the canvas)
    int w_{0};              ///< @internal Width, in columns
    int h_{0};              ///< @internal Height, in rows
};

/// @brief Writes runs of text one after the other on a row of a View, like a cursor
class RowWriter
{
public:
    RowWriter(const View& view, int y) : view_(view), y_(y)
    {
    }

    /// @brief Write text at the cursor with an optional style, and move the cursor after it. Clipped by the view
    RowWriter& add(std::string_view text, Style style = {})
    {
        view_.put(x_, y_, text, style);
        x_ += int(text.size());
        return *this;
    }

    /// @brief Get the column of the cursor: the width written so far
    int column() const
    {
        return x_;
    }

private:
    View view_; ///< @internal Where to write
    int y_;     ///< @internal Row to write on
    int x_{0};  ///< @internal Cursor
};

/**
 * @brief A rectangular grid of cells, in one contiguous block of memory
 *
 * This is where a screen is composed. resize() and clear() reuse the memory, so a canvas that lives for the whole
 * program does not allocate once its size is stable.
 */
class Canvas
{
public:
    /// @brief Change the dimensions of the canvas. All cells are blank afterwards. Invalidates the views
    void resize(Size size);

    /// @brief Reset all cells to blank, default style
    void clear();

    /// @brief Get the dimensions of the canvas
    Size size() const
    {
        return size_;
    }

    /// @brief Get a view on the whole canvas
    View view();

    /**
     * @brief Get a row as a string, ready to print
     *
     * Trailing blanks are dropped. Styles become escape codes if `color` is true, and are ignored if it is false (use
     * false to print the canvas as plain text).
     */
    std::string row_text(int y, bool color) const;

private:
    friend class Screen;

    std::vector<Cell> cells_; ///< @internal size_.cols * size_.rows cells, row after row
    Size size_{0, 0};         ///< @internal Dimensions
};

/// @brief Cut text to `columns` columns. Add ".." at the end if text was cut (only if `columns` is more than 2)
std::string truncate(std::string_view text, int columns);

/**
 * @brief The terminal screen, drawn in place and without flicker
 *
 * Owns two canvases: the frame being composed, and the frame the terminal shows. present() compares them and sends only
 * the rows that changed, so most frames are tiny. It never scrolls the terminal.
 *
 * Usage, once per frame:
 * @code
 *  cli::View view = screen.begin_frame(cli::get_terminal_size());
 *  // draw into `view`
 *  screen.present(color);
 * @endcode
 */
class Screen
{
public:
    /**
     * @brief Start a new frame: get a blank view to draw into
     *
     * The view is one column narrower than the terminal: the last column is never written, to avoid auto-wrap
     * surprises. Its height is the number of rows of the terminal. What does not fit is clipped. If the dimensions of
     * the terminal changed, the next present() clears the whole screen first.
     *
     * @param terminal the current dimensions of the terminal
     */
    View begin_frame(Size terminal);

    /**
     * @brief Send the frame to the terminal
     *
     * @param color if false, styles are ignored
     */
    void present(bool color);

    /// @brief Forget what the terminal shows: the next present() redraws everything (if something else wrote on it)
    void invalidate()
    {
        front_valid_ = false;
    }

private:
    Canvas front_;            ///< @internal What the terminal shows
    Canvas back_;             ///< @internal The frame being composed
    bool front_valid_{false}; ///< @internal False until a frame has been sent, or after invalidate()
    std::string out_;         ///< @internal Output buffer, kept between frames to avoid allocations
};

} // namespace cli
} // namespace kb