#pragma once

#include "SFML/System.hpp"
#include "SFML/Window/Keyboard.hpp"
#include "SFML/Window/Joystick.hpp"
#include "SFML/Graphics/Color.hpp"
#include "imgui/imgui.h"
#include <string>
#include <iostream>

#include <map>
#include <deque>

#include <filesystem>
namespace fs = std::filesystem;

#define PI 3.14159265359

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h>
#include <Dwmapi.h>

static inline void OsOpenInShell(const char* path) {
	// Note: executable path must use  backslashes! 
	ShellExecuteA(0, 0, path, 0, 0, SW_SHOW);
}

inline void setWindowTransparency(HWND hwnd, bool transparent)
{
	if (transparent)
	{
		MARGINS margins = { -1 };

		//SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) | WS_EX_LAYERED);
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_STYLE) | WS_POPUP | WS_VISIBLE);

		HRESULT hr = S_OK;
		hr = DwmExtendFrameIntoClientArea(hwnd, &margins);
		if (!SUCCEEDED(hr))
		{
			__debugbreak();
		}

		LRESULT nRet = S_OK;
		HRGN hRgnBlur = 0;
		DWM_BLURBEHIND bb = { 0 };
		// Create and populate the BlurBehind structure.
		// Set Blur Behind and Blur Region.
		bb.fEnable = transparent;
		bb.dwFlags = DWM_BB_ENABLE | DWM_BB_BLURREGION;
		bb.fTransitionOnMaximized = 0;
		// Fool DWM with a fake region
		if (transparent) { hRgnBlur = CreateRectRgn(-1, -1, 0, 0); }
		bb.hRgnBlur = hRgnBlur;
		// Set Blur Behind mode.
		nRet = DwmEnableBlurBehindWindow(hwnd, &bb);

	}
	else
	{
		SetWindowLongPtr(hwnd, GWL_EXSTYLE, GetWindowLongPtr(hwnd, GWL_EXSTYLE) & ~WS_EX_TRANSPARENT);
		EnableWindow(hwnd, true);
	}
	
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
#include <libgen.h>
#include <filesystem>

namespace fs = std::filesystem;

static inline std::string getAppLocation()
{
    char result[MAX_PATH] = {};
    ssize_t count = readlink("/proc/self/exe", result, MAX_PATH);
    const char *path;
    if (count != -1) {
        path = dirname(result);
        return std::string(path) + "/";
    }

    std::cerr << "Failed to get module file name" << std::endl;
    return "";
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


static fs::path TryAbsolutePath(const fs::path& orig)
{
	std::error_code ec;
	auto out = fs::absolute(orig, ec);
	if (ec.value() != 0)
		return orig;
	else
		return out;
};

static bool runProcess(const std::string& cmd, bool wait = false) {
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
		if(wait)
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

template <typename T, typename T2>
inline sf::Vector2<T> operator *(const sf::Vector2<T>& left, const T2& right)
{
	return sf::Vector2<T>(left.x * (T)right, left.y * (T)right);
}

inline bool operator ==(const ImVec4& left, const ImVec4& right)
{
	return left.x == right.x && left.y == right.y && left.z == right.z && left.w == right.w;
}

inline bool operator !=(const ImVec4& left, const ImVec4& right)
{
	return !(left == right);
}


inline float Clamp(float in, float min = 0.0, float max = 1.0)
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

template <typename T>
inline sf::Vector2<T> Clamp(sf::Vector2<T> in, sf::Vector2<T> min, sf::Vector2<T> max)
{
	if (in.x < min.x)
		in.x = min.x;

	if (in.x > max.x)
		in.x = max.x;

	if (in.y < min.y)
		in.y = min.y;

	if (in.y > max.y)
		in.y = max.y;

	return in;
}

inline float Abs(float in)
{
	if (in < 0)
		return in * -1.f;

	return in;
}

template<typename T>
inline sf::Vector2<T> Abs(sf::Vector2<T> in)
{
	return { in.x > 0 ? in.x : -in.x, in.y > 0 ? in.y : -in.y };
}

template<typename T>
inline float Max(T a, T b)
{
	if (a > b)
		return a;

	return b;
}

template<typename T>
inline sf::Vector2<T> Max(const sf::Vector2<T>& a, const sf::Vector2<T>& b)
{
	return { Max(a.x, b.x), Max(a.y, b.y)};
}

template<typename T>
inline float Min(T a, T b)
{
	if (a < b)
		return a;

	return b;
}

static inline bool ToolTip(const char* title, const char* txt, sf::Clock* hoverTimer, bool forSlider = false)
{

	if (ImGui::IsItemHovered() && hoverTimer->getElapsedTime().asSeconds() > 1.0 && ImGui::BeginTooltip(true))
	{
		if (title != nullptr)
		{
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Text), title);
		}
		ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_Separator), txt);
		if (forSlider)
		{
			ImGui::TextColored(ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive), "(CTRL + Click to type a value)");
		}
		ImGui::EndTooltip();
		return true;
	}
	return false;
}

static inline bool ToolTip(const char* txt, sf::Clock* hoverTimer, bool forSlider = false)
{
	return ToolTip(nullptr, txt, hoverTimer, forSlider);
}

struct TableFlags {
	bool childOpen = false;
	float indentSize = 0;
	float childTop = 0;
	float childBottom = -1;
};

static std::map < std::string, TableFlags> g_indentFlags;

static std::deque<std::string> g_lastIndent;

static inline void BetterIndent(float indSize, const std::string& id)
{
	float maxWidth = ImGui::GetContentRegionAvail().x;
	float widthAvailable = maxWidth;
	widthAvailable -= indSize;

	TableFlags child;
	if (g_indentFlags.count(id))
		child = g_indentFlags[id];

	child.indentSize = indSize;
	child.childOpen = false;

	ImGui::Indent(indSize);

	float childHeight = 1;

	if (child.childBottom > child.childTop)
		childHeight = child.childBottom - child.childTop;

	child.childOpen = true;
	ImGui::BeginChild(("##content" + id).c_str(), { widthAvailable, childHeight }, 0, ImGuiWindowFlags_NoScrollbar);
	child.childTop = 0;


	g_lastIndent.push_back(id);
	
	g_indentFlags[id] = child;
}

static inline void BetterUnindent(const std::string& id)
{
	if (g_indentFlags.count(id))
	{
		g_lastIndent.pop_back();

		auto& child = g_indentFlags[id];

		float cursorPos = ImGui::GetCursorPosY();
		if (cursorPos > 0)
			child.childBottom = cursorPos;

		ImGui::EndChild();

		ImGui::Unindent(child.indentSize);

		child.childOpen = false;
	}
}

static inline void TextCentered(const std::string& text) 
{
	auto windowWidth = ImGui::GetWindowSize().x;
	auto textWidth = ImGui::CalcTextSize(text.c_str()).x;

	ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
	ImGui::Text(text.c_str());
}

inline sf::Color toSFColor(const ImVec4& col)
{
	return sf::Color(sf::Uint8(Clamp(col.x * 255u, 0, 255u)), 
		sf::Uint8(Clamp(col.y * 255u, 0, 255u)), 
		sf::Uint8(Clamp(col.z * 255u, 0, 255u)), 
		sf::Uint8(Clamp(col.w * 255u, 0, 255u)));
}

inline sf::Color toSFColor(const float* col)
{
	if (col != nullptr)
	{
		if(sizeof(col) >= (sizeof(float) * 4))
		{
			return sf::Color(sf::Uint8(Clamp(col[0] * 255u, 0, 255u)),
				sf::Uint8(Clamp(col[1] * 255u, 0, 255u)),
				sf::Uint8(Clamp(col[2] * 255u, 0, 255u)),
				sf::Uint8(Clamp(col[3] * 255u, 0, 255u)));
		}
		else if (sizeof(col) >= (sizeof(float) * 3))
		{
			return sf::Color(sf::Uint8(Clamp(col[0] * 255u, 0, 255u)),
				sf::Uint8(Clamp(col[1] * 255u, 0, 255u)),
				sf::Uint8(Clamp(col[2] * 255u, 0, 255u)),
				sf::Uint8(255u));
		}
	}
	return sf::Color::White;
}

inline ImVec4 toImVec4(const float* col)
{
	if (col != nullptr)
	{
		return ImVec4(col[0], col[1], col[2], col[3]);
	}
}

inline ImVec4 toImVec4(const sf::Color& col)
{
	return ImVec4((float)col.r / 255, (float)col.g / 255, (float)col.b / 255, (float)col.a / 255);
}

inline ImColor toImColor(const float* col)
{
	if (col != nullptr)
	{
		return ImColor(col[0], col[1], col[2], col[3]);
	}
}

inline ImColor toImColor(const sf::Color& col)
{
	return ImColor((float)col.r / 255, (float)col.g / 255, (float)col.b / 255, (float)col.a / 255);
}

inline sf::Vector2f toSFVector(const ImVec2& vec)
{
	return sf::Vector2f(vec.x, vec.y);
}

////////////////////////////////////////////////////////////
/// \relates ImVec4
/// \brief Overload of binary operator *
////////////////////////////////////////////////////////////
inline ImVec4 operator *(const ImVec4& left, const double& right)
{
	return ImVec4(left.x * right, left.y * right, left.z * right, left.w * right);
}

inline ImVec4 operator *(const ImVec4& left, const ImVec4& right)
{
	return ImVec4(left.x * right.x, left.y * right.y, left.z * right.z, left.w * right.w);
}

////////////////////////////////////////////////////////////
/// \relates ImVec4
/// \brief Overload of binary operator +
////////////////////////////////////////////////////////////
inline ImVec4 operator +(const ImVec4& left, const ImVec4& right)
{
	return ImVec4(left.x + right.x, left.y + right.y, left.z + right.z, left.w + right.w);
}

////////////////////////////////////////////////////////////
/// \relates ImVec4
/// \brief Overload of binary operator -
////////////////////////////////////////////////////////////
inline ImVec4 operator -(const ImVec4& left, const ImVec4& right)
{
	return ImVec4(left.x - right.x, left.y - right.y, left.z - right.z, left.w - right.w);
}

////////////////////////////////////////////////////////////
/// \relates ImVec2
/// \brief Overload of binary operator +
////////////////////////////////////////////////////////////
inline ImVec2 operator +(const ImVec2& left, const ImVec2& right)
{
	return ImVec2(left.x + right.x, left.y + right.y);
}

////////////////////////////////////////////////////////////
/// \relates ImVec2
/// \brief Overload of binary operator -
////////////////////////////////////////////////////////////
inline ImVec2 operator -(const ImVec2& left, const ImVec2& right)
{
	return ImVec2(left.x - right.x, left.y - right.y);
}

inline bool LesserButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0), bool border = true)
{
	auto& style = ImGui::GetStyle();
	if(border)
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
	ImVec4 lessCol = style.Colors[ImGuiCol_Button] * ImVec4(0.5, 0.5, 0.5, 1.0);
	ImGui::PushStyleColor(ImGuiCol_Border, style.Colors[ImGuiCol_Button]);
	ImGui::PushStyleColor(ImGuiCol_Button, lessCol);
	bool res = ImGui::Button(label, size_arg);
	ImGui::PopStyleColor(2);
	if (border)
		ImGui::PopStyleVar(1);
	return res;

}

inline bool GreaterButton(const char* label, const ImVec2& size_arg = ImVec2(0, 0), bool border = true)
{
	auto& style = ImGui::GetStyle();
	if (border)
		ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
	ImVec4 lessCol = style.Colors[ImGuiCol_Button] * ImVec4(1.4, 1.4, 1.4, 1.0);
	ImGui::PushStyleColor(ImGuiCol_Border, style.Colors[ImGuiCol_Button]);
	ImGui::PushStyleColor(ImGuiCol_Button, lessCol);
	bool res = ImGui::Button(label, size_arg);
	ImGui::PopStyleColor(2);
	if (border)
		ImGui::PopStyleVar(1);
	return res;

}

inline bool LesserCollapsingHeader(const char* label, ImGuiTreeNodeFlags flags = 0)
{
	auto& style = ImGui::GetStyle();
	ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1);
	ImVec4 lessCol = style.Colors[ImGuiCol_Button] * ImVec4(0.8, 0.8, 0.8, 1.0);
	ImGui::PushStyleColor(ImGuiCol_Border, style.Colors[ImGuiCol_Button]);
	ImGui::PushStyleColor(ImGuiCol_Header, lessCol);
	bool res = ImGui::CollapsingHeader(label, flags);
	ImGui::PopStyleColor(2);
	ImGui::PopStyleVar(1);
	return res;

}

inline bool FloatSliderDrag(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0, int type = 0)
{
	float diff = (v_max - v_min);
	float speed = (diff * 0.004);
	if(diff > 100)
		speed = 5;
	else if (diff <= 100 && diff > 10)
		speed = 1;
	else if (diff <= 10 && diff > 1)
		speed = 0.1;
	else
		speed = 0.005;

	if (type == 0)
		return ImGui::SliderFloat(label, v, v_min, v_max, format, flags);
	else if (type == 1)
		return ImGui::DragFloat(label, v, speed, v_min, v_max, format, flags);
	else if (type == 2)
	{
		bool res = ImGui::InputFloat(label, v, speed, speed * 4, format, 0);
		if (ImGuiSliderFlags_ClampOnInput)
			*v = Clamp(*v, v_min, v_max);
		return res;
	}

	return false;
}

inline bool Float2SliderDrag(const char* label, float* v, float v_min, float v_max, const char* format = "%.3f", ImGuiSliderFlags flags = 0, int type = 0)
{
	float diff = (v_max - v_min);
	float speed = (diff * 0.004);
	if (diff > 100)
		speed = 5;
	else if (diff <= 100 && diff > 10)
		speed = 1;
	else if (diff <= 10 && diff > 1)
		speed = 0.1;
	else
		speed = 0.005;

	if (type == 0)
		return ImGui::SliderFloat2(label, v, v_min, v_max, format, flags);
	else if (type == 1)
		return ImGui::DragFloat2(label, v, speed, v_min, v_max, format, flags);
	else if (type == 2)
	{
		bool res = ImGui::InputFloat2(label, v, format, 0);
		if (ImGuiSliderFlags_ClampOnInput)
		{
			v[0] = Clamp(*v, v_min, v_max);
			v[1] = Clamp(*v, v_min, v_max);
		}
		return res;
	}

	return false;
}

struct SwapButtonDef
{
	std::string btnName = "";
	std::string tooltip = "";
	int onFlag = 0;
};

inline bool SwapButtons(const char* label, const std::vector<SwapButtonDef>& options, int& flag, sf::Clock* hoverTimer, bool useLabel = true)
{
	bool optionChanged = false;
	int outerTableCols = 1;
	if (useLabel)
		outerTableCols = 3;

	float topLine = ImGui::GetCursorScreenPos().y;

	if (ImGui::BeginTable("##tableOuter", outerTableCols, ImGuiTableFlags_SizingStretchProp | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_PreciseWidths))
	{
		if (useLabel)
		{
			ImGui::TableSetupColumn("##buttons", 0, 1.9);
			ImGui::TableSetupColumn("##spacer", ImGuiTableColumnFlags_WidthFixed, ImGui::GetStyle().ItemInnerSpacing.x - 1);
			ImGui::TableSetupColumn("##label", 0, 1);
		}

		ImGui::TableNextColumn();

		if (ImGui::BeginTable("##tableBtns", options.size(), ImGuiTableFlags_SizingStretchSame | ImGuiTableFlags_NoPadInnerX | ImGuiTableFlags_PreciseWidths))
		{
			for (auto& opt : options)
			{
				ImGui::TableNextColumn();
				ImGui::SetCursorScreenPos({ ImGui::GetCursorScreenPos().x, topLine });
				if (flag == opt.onFlag ? ImGui::Button(opt.btnName.c_str(), {-1, 0}) : LesserButton(opt.btnName.c_str(), {-1, 0}))
				{
					flag = opt.onFlag;
					optionChanged = true;
				}
				ToolTip(opt.tooltip.c_str(), hoverTimer);
			}

			ImGui::EndTable();
		}

		if (useLabel)
		{
			ImGui::TableNextColumn();

			ImGui::TableNextColumn();

			ImGui::AlignTextToFramePadding();
			ImGui::Text(label);
		}

		ImGui::EndTable();

		ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetStyle().ItemInnerSpacing.y);
	}

	return optionChanged;
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
	return (a / 180.0) * PI;
}
inline double Rad2Deg(const float& a)
{
	return (a / PI) * 180.0;
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

inline float EllipseRadius(float angle, const sf::Vector2f& axes)
{
	// calculate radius of ellipse from x and y radius components
	float radius = (axes.x * axes.y) /
		pow((pow(axes.x, 2) * pow(sin(angle), 2) + pow(axes.y, 2) * pow(cos(angle), 2)), 0.5);

	return radius;
}

static std::map<wchar_t, sf::Keyboard::Scan::Scancode> g_specialkey_codes = {

	{L'\\', sf::Keyboard::Scan::Backslash},
	{L'/', sf::Keyboard::Scan::Slash},

	{L'[', sf::Keyboard::Scan::LBracket},
	{L']', sf::Keyboard::Scan::RBracket},

	{L'`', sf::Keyboard::Scan::Grave},
	{L'\'', sf::Keyboard::Scan::Apostrophe},
	{L';', sf::Keyboard::Scan::Semicolon},
};

static std::map<int, sf::Keyboard::Scan::Scancode> g_key_codes = {

	{0x08, sf::Keyboard::Scan::Backspace},

	{0x09, sf::Keyboard::Scan::Tab},
	{0x0D, sf::Keyboard::Scan::Enter},
	{0x1B, sf::Keyboard::Scan::Escape},
	{0x1D, sf::Keyboard::Scan::Enter},

	{0X20, sf::Keyboard::Scan::Space},
	{0X21, sf::Keyboard::Scan::PageUp},
	{0X22, sf::Keyboard::Scan::PageDown},
	{0X23, sf::Keyboard::Scan::End},
	{0X24, sf::Keyboard::Scan::Home},

	{0X25, sf::Keyboard::Scan::Left},
	{0X26, sf::Keyboard::Scan::Up},
	{0X27, sf::Keyboard::Scan::Right},
	{0X28, sf::Keyboard::Scan::Down},

	{0x2D, sf::Keyboard::Scan::Insert},
	{0x2E, sf::Keyboard::Scan::Delete},

	{0x30, sf::Keyboard::Scan::Num0},
	{0x31, sf::Keyboard::Scan::Num1},
	{0x32, sf::Keyboard::Scan::Num2},
	{0x33, sf::Keyboard::Scan::Num3},
	{0x34, sf::Keyboard::Scan::Num4},
	{0x35, sf::Keyboard::Scan::Num5},
	{0x36, sf::Keyboard::Scan::Num6},
	{0x37, sf::Keyboard::Scan::Num7},
	{0x38, sf::Keyboard::Scan::Num8},
	{0x39, sf::Keyboard::Scan::Num9},

	{0x41, sf::Keyboard::Scan::A},
	{0x42, sf::Keyboard::Scan::B},
	{0x43, sf::Keyboard::Scan::C},
	{0x44, sf::Keyboard::Scan::D},
	{0x45, sf::Keyboard::Scan::E},
	{0x46, sf::Keyboard::Scan::F},
	{0x47, sf::Keyboard::Scan::G},
	{0x48, sf::Keyboard::Scan::H},
	{0x49, sf::Keyboard::Scan::I},
	{0x4A, sf::Keyboard::Scan::J},
	{0x4B, sf::Keyboard::Scan::K},
	{0x4C, sf::Keyboard::Scan::L},
	{0x4D, sf::Keyboard::Scan::M},
	{0x4E, sf::Keyboard::Scan::N},
	{0x4F, sf::Keyboard::Scan::O},
	{0x50, sf::Keyboard::Scan::P},
	{0x51, sf::Keyboard::Scan::Q},
	{0x52, sf::Keyboard::Scan::R},
	{0x53, sf::Keyboard::Scan::S},
	{0x54, sf::Keyboard::Scan::T},
	{0x55, sf::Keyboard::Scan::U},
	{0x56, sf::Keyboard::Scan::V},
	{0x57, sf::Keyboard::Scan::W},
	{0x58, sf::Keyboard::Scan::X},
	{0x59, sf::Keyboard::Scan::Y},
	{0x5A, sf::Keyboard::Scan::Z},

	{0x60, sf::Keyboard::Scan::Numpad0},
	{0x61, sf::Keyboard::Scan::Numpad1},
	{0x62, sf::Keyboard::Scan::Numpad2},
	{0x63, sf::Keyboard::Scan::Numpad3},
	{0x64, sf::Keyboard::Scan::Numpad4},
	{0x65, sf::Keyboard::Scan::Numpad5},
	{0x66, sf::Keyboard::Scan::Numpad6},
	{0x67, sf::Keyboard::Scan::Numpad7},
	{0x68, sf::Keyboard::Scan::Numpad8},
	{0x69, sf::Keyboard::Scan::Numpad9},

	{0x6A, sf::Keyboard::Scan::NumpadMultiply},
	{0x6B, sf::Keyboard::Scan::NumpadPlus},
	{0x6D, sf::Keyboard::Scan::NumpadMinus},
	{0x6E, sf::Keyboard::Scan::NumpadDecimal},
	{0x6F, sf::Keyboard::Scan::NumpadDivide},

	{0x70, sf::Keyboard::Scan::F1},
	{0x71, sf::Keyboard::Scan::F2},
	{0x72, sf::Keyboard::Scan::F3},
	{0x73, sf::Keyboard::Scan::F4},
	{0x74, sf::Keyboard::Scan::F5},
	{0x75, sf::Keyboard::Scan::F6},
	{0x76, sf::Keyboard::Scan::F7},
	{0x77, sf::Keyboard::Scan::F8},
	{0x78, sf::Keyboard::Scan::F9},
	{0x79, sf::Keyboard::Scan::F10},
	{0x7A, sf::Keyboard::Scan::F11},
	{0x7B, sf::Keyboard::Scan::F12},
	{0x7C, sf::Keyboard::Scan::F13},
	{0x7D, sf::Keyboard::Scan::F14},
	{0x7E, sf::Keyboard::Scan::F15},
	{0x7F, sf::Keyboard::Scan::F16},
	{0x80, sf::Keyboard::Scan::F17},
	{0x81, sf::Keyboard::Scan::F18},
	{0x82, sf::Keyboard::Scan::F19},
	{0x83, sf::Keyboard::Scan::F20},
	{0x84, sf::Keyboard::Scan::F21},
	{0x85, sf::Keyboard::Scan::F22},
	{0x86, sf::Keyboard::Scan::F23},
	{0x87, sf::Keyboard::Scan::F24},

	{0xA0, sf::Keyboard::Scan::LShift},
	{0xA1, sf::Keyboard::Scan::RShift},
	{0xA2, sf::Keyboard::Scan::LControl},
	{0xA3, sf::Keyboard::Scan::RControl},
	{0xA4, sf::Keyboard::Scan::LAlt},
	{0xA5, sf::Keyboard::Scan::RAlt},

	{0xBB, sf::Keyboard::Scan::Equal },
	{0xBC, sf::Keyboard::Scan::Comma },
	{0xBD, sf::Keyboard::Scan::Hyphen },
	{0xBE, sf::Keyboard::Scan::Period },
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

// Just mapping the awkward ones because the sfml function doesn't get it right
static std::map<sf::Keyboard::Scan::Scancode, std::string> g_scancode_names = {

	{sf::Keyboard::Scan::Application,"Application"},
	{sf::Keyboard::Scan::Execute,"Execute"},
	{sf::Keyboard::Scan::ModeChange,"Mode Change"},
	{sf::Keyboard::Scan::Help,"Help"},
	{sf::Keyboard::Scan::Menu,"Menu"},
	{sf::Keyboard::Scan::Select,"Select"},
	{sf::Keyboard::Scan::Redo,"Redo"},
	{sf::Keyboard::Scan::Undo,"Undo"},
	{sf::Keyboard::Scan::Cut,"Cut"},
	{sf::Keyboard::Scan::Copy,"Copy"},
	{sf::Keyboard::Scan::Paste,"Paste"},
	{sf::Keyboard::Scan::VolumeMute,"Vol Mute"},
	{sf::Keyboard::Scan::VolumeUp,"Vol Up"},
	{sf::Keyboard::Scan::VolumeDown,"Vol Down"},
	{sf::Keyboard::Scan::MediaPlayPause,"Media Play/Pause"},
	{sf::Keyboard::Scan::MediaStop,"Media Stop"},
	{sf::Keyboard::Scan::MediaNextTrack,"Media Next"},
	{sf::Keyboard::Scan::MediaPreviousTrack,"Media Prev"},
	{sf::Keyboard::Scan::Back,"Back"},
	{sf::Keyboard::Scan::Forward,"Forward"},
	{sf::Keyboard::Scan::Refresh,"Refresh"},
	{sf::Keyboard::Scan::Stop,"Stop"},
	{sf::Keyboard::Scan::Search,"Search"},
	{sf::Keyboard::Scan::Favorites,"Favourites"},
	{sf::Keyboard::Scan::HomePage,"HomePage"},
	{sf::Keyboard::Scan::LaunchApplication1,"Launch 1"},
	{sf::Keyboard::Scan::LaunchApplication2,"Launch 2"},
	{sf::Keyboard::Scan::LaunchMail,"Mail"},
	{sf::Keyboard::Scan::LaunchMediaSelect,"Media Select"},
	{sf::Keyboard::Scan::F13, "F13"},
	{sf::Keyboard::Scan::F14, "F14"},
	{sf::Keyboard::Scan::F15, "F15"},
	{sf::Keyboard::Scan::F16, "F16"},
	{sf::Keyboard::Scan::F17, "F17"},
	{sf::Keyboard::Scan::F18, "F18"},
	{sf::Keyboard::Scan::F19, "F19"},
	{sf::Keyboard::Scan::F20, "F20"},
	{sf::Keyboard::Scan::F21, "F21"},
	{sf::Keyboard::Scan::F22, "F22"},
	{sf::Keyboard::Scan::F23, "F23"},
	{sf::Keyboard::Scan::F24, "F24"},
};
