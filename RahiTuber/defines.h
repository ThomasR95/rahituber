#pragma once

#include "SFML/System.hpp"
#include "SFML/Window/Keyboard.hpp"
#include "SFML/Window/Joystick.hpp"
#include "SFML/Graphics/Color.hpp"
#include "imgui/imgui.h"
#include <string>
#include <iostream>

#define PI 3.14159265359

#ifdef _WIN32
#include <Windows.h>

static inline void OsOpenInShell(const char* path) {
	// Note: executable path must use  backslashes! 
	ShellExecuteA(0, 0, path, 0, 0, SW_SHOW);
}
#else
#define MAX_PATH 4095
#include <cmath>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <X11/Xatom.h>
//#include <X11/extensions/Xrender.h>
#include <X11/Xutil.h>
#include <GL/glx.h>
#include <GL/glext.h>
#include <GL/glxext.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <filesystem>

namespace fs = std::filesystem;

static std::string getAppLocation()
{
    std::string loc = "";
    char fname[MAX_PATH];
    ssize_t count = readlink("/proc/self/exe", fname, MAX_PATH);
    if (count != -1)
    {
        fname[count] = '\0';  // Null-terminate the string
        fs::path exe = fname;
        loc = exe.parent_path().string() + "/";
    }
    else
    {
        std::cerr << "Failed to get module file name" << std::endl;
    }
    return loc;
}


static inline void OsOpenInShell(const char* path) {
    std::string cmd = std::string("xdg-open ") + path;
    system(cmd.c_str());
}

static int isExtensionSupported(const char *extList, const char *extension)
{

    const char *start;
    const char *where, *terminator;

    /* Extension names should not have spaces. */
    where = strchr(extension, ' ');
    if ( where || *extension == '\0' )
        return 0;

    /* It takes a bit of care to be fool-proof about parsing the
     OpenGL extensions string. Don't be fooled by sub-strings,
     etc. */
    for ( start = extList; ; ) {
        where = strstr( start, extension );

        if ( !where )
            break;

        terminator = where + strlen( extension );

        if ( where == start || *(where - 1) == ' ' )
            if ( *terminator == ' ' || *terminator == '\0' )
                return 1;

        start = terminator;
    }
    return 0;
}

static Bool WaitForMapNotify(Display *d, XEvent *e, char *arg)
{
    return d && e && arg && (e->type == MapNotify) && (e->xmap.window == *(Window*)arg);
}

static void describe_fbconfig(GLXFBConfig fbconfig, Display* Xdisplay)
{
    int doublebuffer;
    int red_bits, green_bits, blue_bits, alpha_bits, depth_bits;

    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_DOUBLEBUFFER, &doublebuffer);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_RED_SIZE, &red_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_GREEN_SIZE, &green_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_BLUE_SIZE, &blue_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_ALPHA_SIZE, &alpha_bits);
    glXGetFBConfigAttrib(Xdisplay, fbconfig, GLX_DEPTH_SIZE, &depth_bits);

    fprintf(stderr, "FBConfig selected:\n"
                    "Doublebuffer: %s\n"
                    "Red Bits: %d, Green Bits: %d, Blue Bits: %d, Alpha Bits: %d, Depth Bits: %d\n",
            doublebuffer == True ? "Yes" : "No",
            red_bits, green_bits, blue_bits, alpha_bits, depth_bits);
}

static inline void setWindowAlwaysOnTop(Display* display, Window window, bool onTop = true) {
    Atom wmState = XInternAtom(display, "_NET_WM_STATE", False);
    Atom wmStateAbove = XInternAtom(display, "_NET_WM_STATE_ABOVE", False);
    Atom wmStateBelow = XInternAtom(display, "_NET_WM_STATE_BELOW", False);

    XEvent xev;
    memset(&xev, 0, sizeof(xev));
    xev.type = ClientMessage;
    xev.xclient.window = window;
    xev.xclient.message_type = wmState;
    xev.xclient.format = 32;
    xev.xclient.data.l[0] = 1;  // _NET_WM_STATE_ADD
    if(onTop)
    {
        xev.xclient.data.l[1] = wmStateAbove;
    }
    else
    {
        xev.xclient.data.l[1] = wmStateBelow;
        xev.xclient.data.l[2] = 0;
        xev.xclient.data.l[3] = 0;
        xev.xclient.data.l[4] = 0;
    }

    XSendEvent(display, DefaultRootWindow(display), False,
               SubstructureRedirectMask | SubstructureNotifyMask, &xev);
    XFlush(display);
}

static inline void setWindowTransparency(Display* Xdisplay, Window window, bool transparent) {

    // static int VisData[] = {
    //     GLX_RENDER_TYPE, GLX_RGBA_BIT,
    //     GLX_DRAWABLE_TYPE, GLX_WINDOW_BIT,
    //     //GLX_DOUBLEBUFFER, (int)GLX_DONT_CARE,
    //     GLX_RED_SIZE, 8,
    //     GLX_GREEN_SIZE, 8,
    //     GLX_BLUE_SIZE, 8,
    //     GLX_ALPHA_SIZE, 8,
    //     GLX_DEPTH_SIZE, 16,
    //     //GLX_TRANSPARENT_TYPE, transparent ? GLX_TRANSPARENT_RGB : GLX_NONE,
    //     None
    // };

    // int Xscreen = DefaultScreen(Xdisplay);
    // Window Xroot = DefaultRootWindow(Xdisplay);

    // XVisualInfo* visual = nullptr;
    // XRenderPictFormat* pict_format = nullptr;

    // int numfbconfigs = 0;
    // GLXFBConfig* fbconfigs = glXChooseFBConfig(Xdisplay, Xscreen, VisData, &numfbconfigs);
    // GLXFBConfig fbconfig = 0;
    // for(int i = 0; i<numfbconfigs; i++) {
    //     visual = (XVisualInfo*) glXGetVisualFromFBConfig(Xdisplay, fbconfigs[i]);
    //     if(!visual)
    //         continue;

    //     pict_format = XRenderFindVisualFormat(Xdisplay, visual->visual);
    //     if(!pict_format)
    //         continue;

    //     fbconfig = fbconfigs[i];
    //     if(pict_format->direct.alphaMask > 0) {
    //         break;
    //     }
    //     XFree(visual);
    // }

    // if(!fbconfig) {
    //     fprintf(stderr, "No matching FB config found");
    //     return;
    // }

    // describe_fbconfig(fbconfig, Xdisplay);

    // /* Create a colormap - only needed on some X clients, eg. IRIX */
    // Colormap cmap = XCreateColormap(Xdisplay, DefaultRootWindow(Xdisplay), visual->visual, AllocNone);
    // XInstallColormap(Xdisplay, cmap);

    XSetWindowAttributes attr = {0,};

    //XVisualInfo vinfo;
    //XMatchVisualInfo(Xdisplay, DefaultScreen(Xdisplay), 32, TrueColor, &vinfo);

    int query[] = {GLX_RGBA, GLX_ALPHA_SIZE, 8, None };
    XVisualInfo* overlayVisual = glXChooseVisual(Xdisplay, DefaultScreen(Xdisplay), query);

    attr.colormap = XCreateColormap(Xdisplay, DefaultRootWindow(Xdisplay), overlayVisual->visual, AllocNone);
    attr.background_pixel = 0;
    attr.border_pixel = 0;
    attr.background_pixmap = 0;
    attr.border_pixmap = 0;
    attr.border_pixel = 0;
    attr.save_under = transparent;
    //attr.override_redirect = transparent;

    XChangeWindowAttributes(Xdisplay, window, /*CWOverrideRedirect | */CWBackPixmap | CWBorderPixmap | CWBackPixel | CWBorderPixel | /*CWColormap |*/ CWSaveUnder, &attr);
    XSetWindowBackground(Xdisplay, window, 0);
    XFlush(Xdisplay);
    XClearWindow(Xdisplay, window);
}

// Function to enable or disable a window
static inline void enableWindow(Display* display, Window window, bool enable) {
    if (enable) {
        XMapWindow(display, window);
    } else {
        XUnmapWindow(display, window);
    }

    XFlush(display);
}

static inline void setWindowProperties(Display* display, Window window) {
    // Set window properties as needed (example: remove border)
    Atom wmHints = XInternAtom(display, "_MOTIF_WM_HINTS", True);
    if (wmHints != None) {
        struct {
            unsigned long flags;
            unsigned long functions;
            unsigned long decorations;
            long inputMode;
            unsigned long status;
        } hints = {2, 0, 0, 0, 0}; // MWM_HINTS_DECORATIONS

        XChangeProperty(display, window, wmHints, wmHints, 32, PropModeReplace, (unsigned char *)&hints, sizeof(hints)/4);
    }
}
#endif

static bool runProcess(const std::string& cmd) {
#ifdef _WIN32
    std::string cmd2 = "cmd /c " + cmd;

    STARTUPINFOA si;
    PROCESS_INFORMATION procInfo;

    ZeroMemory(&si, sizeof(si));

    si.lpTitle = NULL;// (char*)"Get_Update_Version";
    si.dwFlags = STARTF_USESHOWWINDOW;
    si.wShowWindow = SW_HIDE;
    si.cb = sizeof(si);

    ZeroMemory(&procInfo, sizeof(procInfo));

    if (CreateProcessA(NULL, cmd2.data(), NULL, NULL, FALSE, 0, NULL, NULL, &si, &procInfo))
    {
        WaitForSingleObject(procInfo.hProcess, INFINITE);
        CloseHandle(procInfo.hProcess);
        CloseHandle(procInfo.hThread);
        return true;
    }

    return false;
#else
    pid_t pid = fork();
    if (pid == 0) {
        // Child process
        char* args[] = { (char*)"/bin/sh", (char*)"-c", (char*)cmd.c_str(), NULL };
        execvp(args[0], args);
        // If execvp returns, an error occurred
        std::cerr << "Error executing command" << std::endl;
        return false;
    } else if (pid > 0) {
        // Parent process
        int status;
        waitpid(pid, &status, 0);
        if (WIFEXITED(status)) {
            std::cout << "Process exited with status " << WEXITSTATUS(status) << std::endl;
            return true;
        } else {
            std::cerr << "Process did not exit successfully" << std::endl;
            return false;
        }
    } else {
        // Fork failed
        std::cerr << "Error forking process" << std::endl;
        return false;
    }
#endif
}

static inline bool ToolTip(const char* txt, sf::Clock* hoverTimer)
{
	if (ImGui::IsItemHovered() && hoverTimer->getElapsedTime().asSeconds() > 1.0 && ImGui::BeginTooltip())
	{
		ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Separator), txt);
		ImGui::EndTooltip();
		return true;
	}
	return false;
}

inline sf::Color toSFColor(const ImVec4& col)
{
	return sf::Color(sf::Uint8(col.x * 255), sf::Uint8(col.y * 255), sf::Uint8(col.z * 255), sf::Uint8(col.w * 255));
}

inline ImVec4 toImColor(const sf::Color& col)
{
	return ImVec4((float)col.r / 255, (float)col.g / 255, (float)col.b / 255, (float)col.a / 255);
}

inline sf::Vector2f toSFVector(const ImVec2& vec)
{
	return sf::Vector2f(vec.x, vec.y);
}

inline std::string ANSIToUTF8(const std::string& input)
{
#ifdef _WIN32
	int size = MultiByteToWideChar(CP_ACP, 0, input.c_str(),
		input.length(), nullptr, 0);
	std::wstring utf16_str(size, '\0');
	MultiByteToWideChar(CP_ACP, 0, input.c_str(),
		input.length(), &utf16_str[0], size);

	int utf8_size = WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(),
		utf16_str.length(), nullptr, 0,
		nullptr, nullptr);
	std::string utf8_str(utf8_size, '\0');
	WideCharToMultiByte(CP_UTF8, 0, utf16_str.c_str(),
		utf16_str.length(), &utf8_str[0], utf8_size,
		nullptr, nullptr);

	return utf8_str;
#else
    // linux already using utf8 (probably)
    return input;
#endif
}

inline std::string UTF8ToANSI(const std::string& input)
{
    #ifdef _WIN32
	int size = MultiByteToWideChar(CP_UTF8, 0, input.c_str(),
		input.length(), nullptr, 0);
	std::wstring utf16_str(size, '\0');
	MultiByteToWideChar(CP_UTF8, 0, input.c_str(),
		input.length(), &utf16_str[0], size);

	int ansi_size = WideCharToMultiByte(CP_ACP, 0, utf16_str.c_str(),
		utf16_str.length(), nullptr, 0,
		nullptr, nullptr);
	std::string ansi_str(ansi_size, '\0');
	WideCharToMultiByte(CP_ACP, 0, utf16_str.c_str(),
		utf16_str.length(), &ansi_str[0], ansi_size,
		nullptr, nullptr);

	return ansi_str;
#else
    // linux already using utf8 (probably)
    return input;
#endif
}

////////////////////////////////////////////////////////////
/// \relates Vector2
/// \brief Overload of binary operator *
///
/// \param left  Left operand (a vector)
/// \param right Right operand (a vector)
///
/// \return Hadamard product of the two vectors
///
////////////////////////////////////////////////////////////
template <typename T>
inline sf::Vector2<T> operator *(const sf::Vector2<T>& left, const sf::Vector2<T>& right)
{
	return sf::Vector2<T>(left.x * right.x, left.y * right.y);
}

////////////////////////////////////////////////////////////
/// \relates Vector2
/// \brief Overload of binary operator *
///
/// \param left  Left operand (a vector)
/// \param right Right operand (a vector)
///
/// \return Hadamard division of the two vectors
///
////////////////////////////////////////////////////////////
template <typename T>
inline sf::Vector2<T> operator /(const sf::Vector2<T>& left, const sf::Vector2<T>& right)
{
	return sf::Vector2<T>(left.x / right.x, left.y / right.y);
}

inline float Clamp(float in, float min, float max)
{
	if (in < min)
		return min;

	if (in > max)
		return max;

	return in;
}

template <typename T>
inline sf::Vector2<T> Clamp(sf::Vector2<T> in, T min, T max)
{
	if (in.x < min)
		in.x = min;

	if (in.x > max)
		in.x = max;

	if (in.y < min)
		in.y = min;

	if (in.y > max)
		in.y = max;

	return in;
}

inline float Abs(float in)
{
	if (in < 0)
		return in * -1.f;

	return in;
}

template<typename T>
inline float Max(T a, T b)
{
	if (a > b)
		return a;

	return b;
}

template<typename T>
inline float Min(T a, T b)
{
	if (a < b)
		return a;

	return b;
}

inline float Length(const sf::Vector2f& v)
{
    return sqrt(pow(v.x, 2.f) + pow(v.y, 2.f));
}

inline float Dot(const sf::Vector2f& a, const sf::Vector2f& b)
{
	return a.x * b.y + b.x * a.y;
}

inline double Deg2Rad(const float& a)
{
	return a / 180.0 * PI;
}

inline sf::Vector2f Rotate(const sf::Vector2f& point, float angle, sf::Vector2f pivot = { 0.f, 0.f })
{
	sf::Vector2f p = point;

	float s = sin(angle);
	float c = cos(angle);

	// translate point back to origin:
	p.x -= pivot.x;
	p.y -= pivot.y;

	// rotate point
	float xnew = p.x * c - p.y * s;
	float ynew = p.x * s + p.y * c;

	// translate point back:
	p.x = xnew + pivot.x;
	p.y = ynew + pivot.y;
	return p;
}

static std::map<wchar_t, sf::Keyboard::Key> g_specialkey_codes = {

	{L'\\', sf::Keyboard::Backslash},
	{L'/', sf::Keyboard::Slash},

	{L'[', sf::Keyboard::LBracket},
	{L']', sf::Keyboard::RBracket},

	{L'`', sf::Keyboard::Tilde},
	{L'\'', sf::Keyboard::Quote},
	{L';', sf::Keyboard::Semicolon},
};

static std::map<int, sf::Keyboard::Key> g_key_codes = {

	{0x08, sf::Keyboard::Backspace},

	{0x09, sf::Keyboard::Tab},
	{0x0D, sf::Keyboard::Enter},
	{0x1B, sf::Keyboard::Escape},
	{0x1D, sf::Keyboard::Return},

	{0X20, sf::Keyboard::Space},
	{0X21, sf::Keyboard::PageUp},
	{0X22, sf::Keyboard::PageDown},
	{0X23, sf::Keyboard::End},
	{0X24, sf::Keyboard::Home},

	{0X25, sf::Keyboard::Left},
	{0X26, sf::Keyboard::Up},
	{0X27, sf::Keyboard::Right},
	{0X28, sf::Keyboard::Down},

	{0x2D, sf::Keyboard::Insert},
	{0x2E, sf::Keyboard::Delete},

	{0x30, sf::Keyboard::Num0},
	{0x31, sf::Keyboard::Num1},
	{0x32, sf::Keyboard::Num2},
	{0x33, sf::Keyboard::Num3},
	{0x34, sf::Keyboard::Num4},
	{0x35, sf::Keyboard::Num5},
	{0x36, sf::Keyboard::Num6},
	{0x37, sf::Keyboard::Num7},
	{0x38, sf::Keyboard::Num8},
	{0x39, sf::Keyboard::Num9},

	{0x41, sf::Keyboard::A},
	{0x42, sf::Keyboard::B},
	{0x43, sf::Keyboard::C},
	{0x44, sf::Keyboard::D},
	{0x45, sf::Keyboard::E},
	{0x46, sf::Keyboard::F},
	{0x47, sf::Keyboard::G},
	{0x48, sf::Keyboard::H},
	{0x49, sf::Keyboard::I},
	{0x4A, sf::Keyboard::J},
	{0x4B, sf::Keyboard::K},
	{0x4C, sf::Keyboard::L},
	{0x4D, sf::Keyboard::M},
	{0x4E, sf::Keyboard::N},
	{0x4F, sf::Keyboard::O},
	{0x50, sf::Keyboard::P},
	{0x51, sf::Keyboard::Q},
	{0x52, sf::Keyboard::R},
	{0x53, sf::Keyboard::S},
	{0x54, sf::Keyboard::T},
	{0x55, sf::Keyboard::U},
	{0x56, sf::Keyboard::V},
	{0x57, sf::Keyboard::W},
	{0x58, sf::Keyboard::X},
	{0x59, sf::Keyboard::Y},
	{0x5A, sf::Keyboard::Z},

	{0x60, sf::Keyboard::Numpad0},
	{0x61, sf::Keyboard::Numpad1},
	{0x62, sf::Keyboard::Numpad2},
	{0x63, sf::Keyboard::Numpad3},
	{0x64, sf::Keyboard::Numpad4},
	{0x65, sf::Keyboard::Numpad5},
	{0x66, sf::Keyboard::Numpad6},
	{0x67, sf::Keyboard::Numpad7},
	{0x68, sf::Keyboard::Numpad8},
	{0x69, sf::Keyboard::Numpad9},

	{0x6A, sf::Keyboard::Multiply},
	{0x6B, sf::Keyboard::Add},
	{0x6D, sf::Keyboard::Subtract},
	{0x6E, sf::Keyboard::Period},
	{0x6F, sf::Keyboard::Divide},

	{0x70, sf::Keyboard::F1},
	{0x71, sf::Keyboard::F2},
	{0x72, sf::Keyboard::F3},
	{0x73, sf::Keyboard::F4},
	{0x74, sf::Keyboard::F5},
	{0x75, sf::Keyboard::F6},
	{0x76, sf::Keyboard::F7},
	{0x77, sf::Keyboard::F8},
	{0x78, sf::Keyboard::F9},
	{0x79, sf::Keyboard::F10},
	{0x7A, sf::Keyboard::F11},
	{0x7B, sf::Keyboard::F12},
	{0x7C, sf::Keyboard::F13},
	{0x7D, sf::Keyboard::F14},
	{0x7E, sf::Keyboard::F15},

	{0xA0, sf::Keyboard::LShift},
	{0xA1, sf::Keyboard::RShift},
	{0xA2, sf::Keyboard::LControl},
	{0xA3, sf::Keyboard::RControl},
	{0xA4, sf::Keyboard::LAlt},
	{0xA5, sf::Keyboard::RAlt},

	{0xBB, sf::Keyboard::Equal },
	{0xBC, sf::Keyboard::Comma },
	{0xBD, sf::Keyboard::Hyphen },
	{0xBE, sf::Keyboard::Period },
};

static std::map<sf::Joystick::Axis, std::string> g_axis_names = {
	{sf::Joystick::Axis::X, "X"},
	{sf::Joystick::Axis::Y, "Y"},
	{sf::Joystick::Axis::Z, "Z"},
	{sf::Joystick::Axis::R, "R"},
	{sf::Joystick::Axis::U, "U"},
	{sf::Joystick::Axis::V, "V"},
	{sf::Joystick::Axis::PovX, "PovX"},
	{sf::Joystick::Axis::PovY, "PovY"},
};

static std::map<sf::Keyboard::Key, std::string> g_key_names = {
	{sf::Keyboard::A, "A"},
	{sf::Keyboard::B, "B"},
	{sf::Keyboard::C, "C"},
	{sf::Keyboard::D, "D"},
	{sf::Keyboard::E, "E"},
	{sf::Keyboard::F, "F"},
	{sf::Keyboard::G, "G"},
	{sf::Keyboard::H, "H"},
	{sf::Keyboard::I, "I"},
	{sf::Keyboard::J, "J"},
	{sf::Keyboard::K, "K"},
	{sf::Keyboard::L, "L"},
	{sf::Keyboard::M, "M"},
	{sf::Keyboard::N, "N"},
	{sf::Keyboard::O, "O"},
	{sf::Keyboard::P, "P"},
	{sf::Keyboard::Q, "Q"},
	{sf::Keyboard::R, "R"},
	{sf::Keyboard::S, "S"},
	{sf::Keyboard::T, "T"},
	{sf::Keyboard::U, "U"},
	{sf::Keyboard::V, "V"},
	{sf::Keyboard::W, "W"},
	{sf::Keyboard::X, "X"},
	{sf::Keyboard::Y, "Y"},
	{sf::Keyboard::Z, "Z"},

	{sf::Keyboard::Num1, "1"},
	{sf::Keyboard::Num2, "2"},
	{sf::Keyboard::Num3, "3"},
	{sf::Keyboard::Num4, "4"},
	{sf::Keyboard::Num5, "5"},
	{sf::Keyboard::Num6, "6"},
	{sf::Keyboard::Num7, "7"},
	{sf::Keyboard::Num8, "8"},
	{sf::Keyboard::Num9, "9"},
	{sf::Keyboard::Num0, "0"},

	{sf::Keyboard::LControl, "Ctrl"},
	{sf::Keyboard::RControl, "Ctrl"},
	{sf::Keyboard::LAlt, "Alt"},
	{sf::Keyboard::RAlt, "Alt"},
	{sf::Keyboard::LShift, "Shift"},
	{sf::Keyboard::RShift, "Shift"},
	{sf::Keyboard::Delete, "Del"},
	{sf::Keyboard::Backspace, "Backspace"},
	{sf::Keyboard::Insert, "Ins"},
	{sf::Keyboard::Home, "Home"},
	{sf::Keyboard::End, "End"},
	{sf::Keyboard::PageUp, "PgUp"},
	{sf::Keyboard::PageDown, "PgDn"},
	{sf::Keyboard::Tab, "Tab"},
	{sf::Keyboard::Enter, "Enter"},
	{sf::Keyboard::Return, "Return"},
	{sf::Keyboard::Escape, "Escape"},
	{sf::Keyboard::Space, "Space"},

	{sf::Keyboard::Backslash, "\\"},
	{sf::Keyboard::Slash, "/"},
	{sf::Keyboard::Hyphen, "-"},
	{sf::Keyboard::Equal, "="},
	{sf::Keyboard::Period, "."},
	{sf::Keyboard::LBracket, "["},
	{sf::Keyboard::RBracket, "]"},
	{sf::Keyboard::Tilde, "` (backtick)"},
	{sf::Keyboard::Quote, "\'"},
	{sf::Keyboard::Comma, ","},

	{sf::Keyboard::Up, "Up"},
	{sf::Keyboard::Down, "Down"},
	{sf::Keyboard::Left, "Left"},
	{sf::Keyboard::Right, "Right"},

	{sf::Keyboard::Subtract, "Num-"},
	{sf::Keyboard::Multiply, "Num*"},
	{sf::Keyboard::Add, "Num+"},
	{sf::Keyboard::Divide, "Num/"},

	{sf::Keyboard::Numpad1, "Num1"},
	{sf::Keyboard::Numpad2, "Num2"},
	{sf::Keyboard::Numpad3, "Num3"},
	{sf::Keyboard::Numpad4, "Num4"},
	{sf::Keyboard::Numpad5, "Num5"},
	{sf::Keyboard::Numpad6, "Num6"},
	{sf::Keyboard::Numpad7, "Num7"},
	{sf::Keyboard::Numpad8, "Num8"},
	{sf::Keyboard::Numpad9, "Num9"},
	{sf::Keyboard::Numpad0, "Num0"},

	{sf::Keyboard::F1, "F1"},
	{sf::Keyboard::F2, "F2"},
	{sf::Keyboard::F3, "F3"},
	{sf::Keyboard::F4, "F4"},
	{sf::Keyboard::F5, "F5"},
	{sf::Keyboard::F6, "F6"},
	{sf::Keyboard::F7, "F7"},
	{sf::Keyboard::F8, "F8"},
	{sf::Keyboard::F9, "F9"},
	{sf::Keyboard::F10, "F10"},
	{sf::Keyboard::F11, "F11"},
	{sf::Keyboard::F12, "F12"},
};
