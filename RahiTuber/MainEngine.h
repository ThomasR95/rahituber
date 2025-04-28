#pragma once

#include <string>
#include <iostream>
#include <random>
#include <deque>

#define IMGUI_DISABLE_WIN32_FUNCTIONS
#define IMGUI_ENABLE_FREETYPE
#include "imgui.h"
#include "imgui/misc/freetype/imgui_freetype.h"
#include "imgui-SFML.h"

#include "imgui_internal.h"

#include "defines.h"

#include <fstream>

#ifdef _WIN32
#include "spout2/SpoutSender.h"

#include "wtypes.h"
#include <Windows.h>
#include "ShellScalingApi.h"
#include <fileapi.h>
#include <Dwmapi.h>

#include "CrashHandler.h"

#pragma comment (lib, "Dwmapi.lib")
#endif

#include "Config.h"
#include "xmlConfig.h"

#include "LayerManager.h"

#include "Gamepad.h"

// must be last
#include "websocket.h"

AudioConfig* g_audioConfig = nullptr;

float mean(float a, float b) { return a + (b - a) * 0.5f; }
float between(float a, float b) { return a * 0.5f + b * 0.5f; }

void four1(float* data, unsigned long nn)
{
	unsigned long n, mmax, m, j, istep, i;
	double wtemp, wr, wpr, wpi, wi, theta;
	double tempr, tempi;

	// reverse-binary reindexing
	n = nn << 1;
	j = 1;
	for (i = 1; i < n; i += 2) {
		if (j > i) {
			std::swap(data[j - 1], data[i - 1]);
			std::swap(data[j], data[i]);
		}
		m = nn;
		while (m >= 2 && j > m) {
			j -= m;
			m >>= 1;
		}
		j += m;
	};

	// here begins the Danielson-Lanczos section
	mmax = 2;
	while (n > mmax) {
		istep = mmax << 1;
		theta = -(2 * PI / mmax);
		wtemp = sin(0.5 * theta);
		wpr = -2.0 * wtemp * wtemp;
		wpi = sin(theta);
		wr = 1.0;
		wi = 0.0;
		for (m = 1; m < mmax; m += 2) {
			for (i = m; i <= n; i += istep) {
				j = i + mmax;
				tempr = wr * data[j - 1] - wi * data[j];
				tempi = wr * data[j] + wi * data[j - 1];

				data[j - 1] = data[i - 1] - tempr;
				data[j] = data[i] - tempi;
				data[i - 1] += tempr;
				data[i] += tempi;
			}
			wtemp = wr;
			wr += wr * wpr - wi * wpi;
			wi += wi * wpr + wtemp * wpi;
		}
		mmax = istep;
	}
}

static int recordCallback(const void* inputBuffer, void* outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void* userData)
{
	int checkSize = framesPerBuffer;

	int numChannels = g_audioConfig->_params.channelCount;

	//Erase old frames, leave enough in the vector for 2 checkSizes
	{ //lock for frequency data
		std::lock_guard<std::mutex> guard(g_audioConfig->_freqDataMutex);
		if (g_audioConfig->_frames.size() > checkSize * 2)
			g_audioConfig->_frames.erase(g_audioConfig->_frames.begin(), g_audioConfig->_frames.begin() + (g_audioConfig->_frames.size() - checkSize * 2));
	}

	SAMPLE* rptr = (SAMPLE*)inputBuffer;

	int s = 0;

	while (s < checkSize)
	{
		SAMPLE splLeft = (*rptr++);
		SAMPLE splRight = splLeft;
		if (numChannels == 2)
			splRight = (*rptr++);

		SAMPLE spl = (splLeft + splRight) / 2.f;

		spl = spl * spl;

		{ //lock for frequency data
			std::lock_guard<std::mutex> guard(g_audioConfig->_freqDataMutex);

			g_audioConfig->_frames.push_back(fabs(spl));
		}

		s++;
	}

	g_audioConfig->_processedNew = true;

	return paContinue;
}

class MainEngine
{
public:
	AppConfig* appConfig = nullptr;
	AudioConfig* audioConfig = nullptr;
	UIConfig* uiConfig = nullptr;
	LayerManager* layerMan = nullptr;

#ifdef _WIN32
	Spout* spout = nullptr;
#endif

	void LoadCustomFont()
	{
		ImGuiIO& io = ImGui::GetIO();

		fs::path fontpath(appConfig->_appLocation);
		fontpath.append(uiConfig->_fontName);
		std::error_code ec;
		if (!fs::exists(fontpath, ec))
			return;

		io.Fonts->Clear();

		io.Fonts->AddFontDefault();

		uiConfig->_fontReloadNeeded = false;

		ImFontConfig cfg;
		cfg.OversampleH = 1;
		cfg.SizePixels = uiConfig->_fontSize;
		//cfg.MergeMode = true;
		io.Fonts->FontBuilderIO = ImGuiFreeType::GetBuilderForFreeType();
		io.Fonts->FontBuilderFlags = ImGuiFreeTypeBuilderFlags_Bold;

		ImFont* result = io.Fonts->AddFontFromFileTTF(fontpath.string().c_str(), uiConfig->_fontSize, &cfg);
		if (result == nullptr)
			return;

		io.FontDefault = result;

		if (!io.Fonts->IsBuilt())
		{
			if (!io.Fonts->Build())
			{
				io.FontDefault = io.Fonts->AddFontDefault();
				return;
			}

			// Retrieve texture in RGBA format
			unsigned char* tex_pixels = NULL;
			int tex_width, tex_height, bpp;
			io.Fonts->GetTexDataAsRGBA32(&tex_pixels, &tex_width, &tex_height, &bpp);

			uiConfig->fontimg.create(tex_width, tex_height, tex_pixels);
			uiConfig->fontTex.loadFromImage(uiConfig->fontimg);
			uiConfig->fontTex.setSmooth(true);

			uiConfig->fontBuilt = true;
		}

		io.Fonts->SetTexID((ImTextureID)uiConfig->fontTex.getNativeHandle());

		uiConfig->_font.loadFromFile(fontpath.string());

		io.FontGlobalScale = 0.5;
	}

	void GetWindowSizes()
	{
#ifdef _WIN32
		RECT desktop;
		// Get a handle to the desktop window
		const HWND hDesktop = GetDesktopWindow();
		// Get the size of screen to the variable desktop
		GetWindowRect(hDesktop, &desktop);

		appConfig->_fullScrW = desktop.right;
		appConfig->_fullScrH = desktop.bottom;
#else
		Display* display = XOpenDisplay(NULL);
		if (display == NULL) {
			std::cerr << "Cannot open display" << std::endl;
			exit(1);
		}

		Screen* screen = DefaultScreenOfDisplay(display);

		appConfig->_fullScrW = screen->width;
		appConfig->_fullScrH = screen->height;

		XCloseDisplay(display);
#endif

		appConfig->_minScrW = SCRW;
		appConfig->_minScrH = SCRH;

		appConfig->_scrW = appConfig->_minScrW;
		appConfig->_scrH = appConfig->_minScrH;

		appConfig->_ratio = appConfig->_scrW / appConfig->_scrH;
	}

	bool EnsurePositionOnMonitor(int& x, int& y)
	{
#ifdef _WIN32
		POINT windPos;
		windPos.x = x;
		windPos.y = y;
		HMONITOR monitor = MonitorFromPoint(windPos, MONITOR_DEFAULTTONULL);
		if (monitor == NULL)
		{
			HMONITOR monitor = MonitorFromPoint(windPos, MONITOR_DEFAULTTONEAREST);
			MONITORINFO mInfo;
			mInfo.cbSize = sizeof(MONITORINFO);
			if (GetMonitorInfoA(monitor, &mInfo));
			{
				x = mInfo.rcWork.left;
				y = mInfo.rcWork.top;
				return true;
			}
		}
#endif
		return false;
	}

	float CheckMonitorScaleFactor(const sf::Vector2i& windowPos, const sf::Vector2u windowSize)
	{
		float scaleFactor = 1.0;
#ifdef _WIN32
		RECT windowRect;
		windowRect.left = windowPos.x;
		windowRect.top = windowPos.y;
		windowRect.right = windowRect.left + windowSize.x;
		windowRect.bottom = windowRect.top + windowSize.y;
		HMONITOR hMonitor = MonitorFromRect(&windowRect, MONITOR_DEFAULTTONEAREST);
		DEVICE_SCALE_FACTOR monitorScale;

		if (GetScaleFactorForMonitor(hMonitor, &monitorScale) == S_OK
			&& monitorScale != DEVICE_SCALE_FACTOR_INVALID)
		{
			scaleFactor = (float)monitorScale * 0.01;
		}
#endif

		return scaleFactor;
	}

	void initWindow(bool firstStart = false, bool ignoreScaleForResize = false)
	{
		if (appConfig->_isFullScreen)
		{
			if (appConfig->_window.isOpen())
			{
				appConfig->_minScrW = appConfig->_window.getSize().x;
				appConfig->_minScrH = appConfig->_window.getSize().y;
				auto pos = appConfig->_window.getPosition();
				appConfig->_scrX = pos.x;
				appConfig->_scrY = pos.y;
			}
			appConfig->_scrW = appConfig->_fullScrW;
			appConfig->_scrH = appConfig->_fullScrH;
			if (!appConfig->_wasFullScreen || firstStart)
			{
				if (firstStart)
				{
					appConfig->_nameLock.lock();
					{
						auto beforeCreate = std::chrono::system_clock::now();
						appConfig->_window.create(sf::VideoMode(appConfig->_fullScrW, appConfig->_fullScrH, 32U), appConfig->windowName, 0);
						logToFile(appConfig, "Window Creation took " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() - beforeCreate)).count()) + " ms");
						appConfig->_pendingNameChange = false;
					}
					appConfig->_nameLock.unlock();
				}
				appConfig->_scrW = appConfig->_fullScrW + 1;
				appConfig->_scrH = appConfig->_fullScrH + 1;
				appConfig->_window.setSize({ (sf::Uint16)appConfig->_scrW, (sf::Uint16)appConfig->_scrH });
				appConfig->_window.setPosition({ 0,0 });
				sf::View v = appConfig->_window.getView();
				v.setSize({ appConfig->_scrW, appConfig->_scrH });
				v.setCenter({ appConfig->_scrW / 2, appConfig->_scrH / 2 });
				appConfig->_window.setView(v);
			}
		}
		else
		{
			bool movedOntoMonitor = EnsurePositionOnMonitor(appConfig->_scrX, appConfig->_scrY);
			if (firstStart || movedOntoMonitor)
			{
				appConfig->mainWindowScaling = CheckMonitorScaleFactor({ appConfig->_scrX, appConfig->_scrY }, { (sf::Uint16)appConfig->_minScrW, (sf::Uint16)appConfig->_minScrH });
			}

			float resizeScale = appConfig->mainWindowScaling;
			if (ignoreScaleForResize)
				resizeScale = 1.0f;

			appConfig->_scrW = appConfig->_minScrW * resizeScale;
			appConfig->_scrH = appConfig->_minScrH * resizeScale;
			if (appConfig->_wasFullScreen || firstStart)
			{
				std::string name = "RahiTuber";
				appConfig->_nameLock.lock();
				{
					name = appConfig->windowName;
				}
				appConfig->_nameLock.unlock();

				auto beforeCreate = std::chrono::system_clock::now();
				appConfig->_window.create(sf::VideoMode(appConfig->_scrW, appConfig->_scrH, 32U), name, 0);
				logToFile(appConfig, "Window Creation took " + std::to_string(std::chrono::duration_cast<std::chrono::milliseconds>((std::chrono::system_clock::now() - beforeCreate)).count()) + " ms");
				appConfig->_pendingNameChange = false;
				appConfig->_window.setPosition({ appConfig->_scrX, appConfig->_scrY });
				
			}
			else
			{
				appConfig->_window.setSize({ (sf::Uint16)appConfig->_scrW, (sf::Uint16)appConfig->_scrH });
				sf::View v = appConfig->_window.getView();
				v.setSize({ appConfig->_scrW, appConfig->_scrH });
				v.setCenter({ appConfig->_scrW / 2, appConfig->_scrH / 2 });
				appConfig->_window.setView(v);

				auto pos = appConfig->_window.getPosition();
				if (appConfig->_scrX != pos.x || appConfig->_scrY != pos.y)
				{
					appConfig->_window.setPosition({ appConfig->_scrX, appConfig->_scrY });
				}
			}
		}

		appConfig->_menuRT.create(appConfig->_scrW, appConfig->_scrH);
		appConfig->_layersRT.create(appConfig->_scrW, appConfig->_scrH);

		float cornerGrabSize = 20 * appConfig->mainWindowScaling;

		uiConfig->_topLeftBox.setPosition(0, 0);
		uiConfig->_topLeftBox.setSize({ cornerGrabSize,cornerGrabSize });
		uiConfig->_topLeftBox.setFillColor({ 255,255,255,100 });
		uiConfig->_bottomRightBox = uiConfig->_topLeftBox;
		uiConfig->_bottomRightBox.setPosition({ appConfig->_scrW - cornerGrabSize, appConfig->_scrH - cornerGrabSize });
		uiConfig->_resizeBox.setSize({ appConfig->_scrW, appConfig->_scrH });
		uiConfig->_resizeBox.setOutlineThickness(1);
		uiConfig->_resizeBox.setFillColor({ 0,0,0,0 });
		uiConfig->_outlineBox.setPosition(2, 2);
		uiConfig->_outlineBox.setSize({ appConfig->_scrW - 4, appConfig->_scrH - 4 });
		uiConfig->_outlineBox.setOutlineThickness(2);
		uiConfig->_outlineBox.setFillColor({ 0,0,0,0 });
		uiConfig->_outlineBox.setOutlineColor(sf::Color(255, 255, 0, 100));

		appConfig->_wasFullScreen = appConfig->_isFullScreen;

		appConfig->_window.setVerticalSyncEnabled(appConfig->_enableVSync);

		appConfig->_window.setFramerateLimit(appConfig->_enableVSync ? 0 : appConfig->_fpsLimit);

#ifdef _WIN32
		HWND hwnd = appConfig->_window.getSystemHandle();
		HWND hwnd2 = appConfig->_menuWindow.getSystemHandle();

		if (appConfig->_alwaysOnTop)
		{
			SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			//if (appConfig->_menuWindow.isOpen())
			//	SetWindowPos(hwnd2, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
		}

		setWindowTransparency(hwnd, appConfig->_transparent);

#else
		Display* display = XOpenDisplay(NULL);
		if (display == NULL) {
			std::cerr << "Cannot open display" << std::endl;
			exit(1);
		}

		Window hwnd = appConfig->_window.getSystemHandle();
		Window hwnd2 = appConfig->_menuWindow.getSystemHandle();

		if (appConfig->_alwaysOnTop) {
			setWindowAlwaysOnTop(display, hwnd);
			if (appConfig->_menuWindow.isOpen()) {
				setWindowAlwaysOnTop(display, hwnd2);
			}
		}

		//setWindowTransparency(display, hwnd, appConfig->_transparent);

		XCloseDisplay(display);
#endif

		if (uiConfig->_ico.getPixelsPtr())
		{
			appConfig->_window.setIcon(uiConfig->_ico.getSize().x, uiConfig->_ico.getSize().y, uiConfig->_ico.getPixelsPtr());
			appConfig->_menuWindow.setIcon(uiConfig->_ico.getSize().x, uiConfig->_ico.getSize().y, uiConfig->_ico.getPixelsPtr());

		}
	}

	void swapFullScreen()
	{
		appConfig->_isFullScreen = !appConfig->_isFullScreen;
		initWindow();
	}

	void menuHelp(ImGuiStyle& style)
	{
		bool helpPressed = ImGui::Button("Help", { -1, ImGui::GetFrameHeight() });

		sf::Vector2f dotpos = toSFVector(ImGui::GetItemRectMax());
		dotpos -= sf::Vector2f(ImGui::GetFrameHeight()*0.52, ImGui::GetFrameHeight());
		uiConfig->_helpBtnPosition = dotpos;

		if(helpPressed)
		{
			float h = ImGui::GetWindowHeight();
			ImGui::SetNextWindowSize({ 400 * appConfig->scalingFactor, h });

			if (appConfig->_menuWindow.isOpen())
			{
				ImVec2 wSize = ImGui::GetCurrentWindow()->Size;
				ImGui::SetNextWindowPos({ wSize.x / 2 - 200, wSize.y / 6 });
			}
			else
			{
				ImGui::SetNextWindowPos({ appConfig->_scrW / 2 - 200, appConfig->_scrH / 6 });
			}
			ImGui::OpenPopup("Help");
		}

		ImGui::SetNextWindowSize({ 400 * appConfig->scalingFactor,-1 });

		uiConfig->_helpPopupActive = false;

		bool p_open = true;
		if (ImGui::BeginPopupModal("Help", &p_open, ImGuiWindowFlags_NoResize))
		{
			uiConfig->_helpPopupActive = true;

			float UIUnit = ImGui::GetFrameHeight();

			ImGui::SetWindowSize({ 400 * appConfig->scalingFactor,-1 });

			if (appConfig->_updateAvailable)
			{
				ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_ButtonActive]);
				char buf[256] = {};
#ifdef WIN32
				sprintf_s(buf, "Update %.3f Available!", appConfig->_versionAvailable);
#else
                sprintf(buf, "Update %.3f Available!", appConfig->_versionAvailable);
#endif
				ImGui::SeparatorText(buf);
				ImGui::PopStyleColor();

				if (ImGui::Button("Go to Downloads page", { -1, UIUnit }))
				{
					OsOpenInShell("https://rahisaurus.itch.io/rahituber/download/09yDNW4jF8gpROQenA8J4jTZl96VGo4YbzGez4ys");
				}
				ToolTip("Opens the RahiTuber Downloads page in your default web browser.", "https://rahisaurus.itch.io/rahituber/\ndownload/09yDNW4jF8gpROQenA8J4jTZl96VGo4YbzGez4ys",  & appConfig->_hoverTimer);

				ImGui::SeparatorText("");
			}

			if (ImGui::Button("Devlog (web link)", { -1, UIUnit }))
			{
				OsOpenInShell("https://rahisaurus.itch.io/rahituber/devlog");
			}
			ToolTip("Opens the RahiTuber Development log page\nin your default web browser.", "https://rahisaurus.itch.io/rahituber/devlog", & appConfig->_hoverTimer);

			if (ImGui::Button("FAQ (web link)", { -1, UIUnit }))
			{
				OsOpenInShell("https://itch.io/t/3967527/faq");
			}
			ToolTip("Opens the RahiTuber FAQ page in your default web browser.", "https://itch.io/t/3967527/faq", &appConfig->_hoverTimer);

			if (ImGui::Button("Tutorials (web link)", { -1, UIUnit }))
			{
				OsOpenInShell("https://itch.io/t/3967451/tutorials");
			}
			ToolTip("Opens the RahiTuber Tutorials page in your default web browser.", "https://itch.io/t/3967451/tutorials", &appConfig->_hoverTimer);

			ImGui::NewLine();

			ImGui::SeparatorText("General tips:");

			ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Text]);
			ImGui::TextWrapped("Hover over any control to see an explanation of what it does.");
			ImGui::TextWrapped("CTRL+click any input field to manually type the value. For some sliders you can type a value outside of the sliding range.");
			ImGui::NewLine();

			ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Window Controls:");
			ImGui::TextWrapped("Drag the squares in the top-left or bottom-right corner to resize the window.\nDrag with the middle mouse button, or use the move tab in the top centre, to move the whole window.");
			ImGui::PopStyleColor();

			ImGui::NewLine();

			ImVec4 dimText = style.Colors[ImGuiCol_Separator] * 0.8;
			ImGui::PushStyleColor(ImGuiCol_Text, dimText);
			ImGui::PushStyleColor(ImGuiCol_Separator, dimText);

			time_t timeNow = time(0);
			tm timeStruct;
#ifdef _WIN32
			localtime_s(&timeStruct, &timeNow);
#else
			localtime_r(&timeNow, &timeStruct);
#endif
			int year = timeStruct.tm_year + 1900;

			ImGui::SeparatorText("Acknowledgement");

			ImGui::TextWrapped("PortAudio (c) 1999-2006 Ross Bencina and Phil Burk");
			ImGui::TextWrapped("SFML (c) 2007-%d Laurent Gomila", year);
			ImGui::TextWrapped("Dear ImGui (c) 2014-%d Omar Cornut", year);
			ImGui::NewLine();
			ImGui::TextWrapped("RahiTuber (c) 2018-%d Rahisaurus (Tom Rule)", year);
			ImGui::PopStyleColor(2);

			ImGui::TextWrapped("Version: %.3f", appConfig->_versionNumber);

			ImGui::Separator();
			if (ImGui::Button("OK", { -1,20 }))
			{
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}

	void menuAdvanced(ImGuiStyle& style)
	{
		float UIUnit = ImGui::GetFrameHeight();

		if (ImGui::Button("Advanced", { -1, ImGui::GetFrameHeight() }))
		{
			float h = ImGui::GetWindowHeight();
			ImGui::SetNextWindowSize({ UIUnit * 24, h });

			if (appConfig->_menuWindow.isOpen())
			{
				ImVec2 wSize = ImGui::GetCurrentWindow()->Size;
				ImGui::SetNextWindowPos({ wSize.x / 2 - UIUnit*12, wSize.y / 6 });
			}
			else
			{
				ImGui::SetNextWindowPos({ appConfig->_scrW / 2 - UIUnit*12, appConfig->_scrH/2 - uiConfig->_advancedMenuHeight/2 });
			}
			ImGui::OpenPopup("Advanced Settings");
		}
		ImGui::SetNextWindowSize({ UIUnit * 24,-1 });


		uiConfig->_advancedMenuShowing = false;

		bool p_open = true;
		if (ImGui::BeginPopupModal("Advanced Settings", &p_open, ImGuiWindowFlags_NoResize))
		{
			ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2,2 });

			uiConfig->_advancedMenuShowing = true;

			ImGui::SeparatorText("Window Options");

			ImGui::BeginTable("##advancedOptions", 2, ImGuiTableFlags_SizingStretchSame);

			ImGui::TableNextColumn();
			ImGui::Checkbox("Menu On Start", &uiConfig->_showMenuOnStart);
			ToolTip("Start the application with the menu open.", &appConfig->_hoverTimer);

#ifdef _WIN32
			ImGui::TableNextColumn();
			if (ImGui::Checkbox("Always On Top", &appConfig->_alwaysOnTop))
			{

				HWND hwnd = appConfig->_window.getSystemHandle();

				if (appConfig->_alwaysOnTop)
					SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
				else
					SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			ToolTip("Keeps the app above all other windows on your screen.", &appConfig->_hoverTimer);
#endif

			ImGui::TableNextColumn();
			if (ImGui::Checkbox("VSync", &appConfig->_enableVSync))
			{
				initWindow();
			}
			ToolTip("Enable/Disable Vertical Sync.", &appConfig->_hoverTimer);

			ImGui::TableNextColumn();
			ImGui::BeginDisabled(appConfig->_enableVSync);
			ImGui::InputInt("FPS Limit", &appConfig->_fpsLimit, 10, 30);
			if (ImGui::IsItemDeactivatedAfterEdit())
			{
				initWindow();
			}
			ToolTip("Set the FPS limit for the application.", &appConfig->_hoverTimer);
			ImGui::EndDisabled();

			ImGui::TableNextColumn();
			if (ImGui::Checkbox("Name windows separately", &appConfig->_nameWindowWithSet))
			{
				appConfig->_nameLock.lock();
				if (appConfig->_nameWindowWithSet)
					appConfig->windowName = "RahiTuber - " + layerMan->LayerSetName();
				else
					appConfig->windowName = "RahiTuber";
				appConfig->_pendingNameChange = true;
				appConfig->_pendingSpoutNameChange = true;
				appConfig->_nameLock.unlock();
			}
			ToolTip("Name the window based on the Layer Set.\nUseful for if you want multiple instances of RahiTuber.", &appConfig->_hoverTimer);

			ImGui::EndTable();

			ImGui::SeparatorText("Appearance");

			if (ImGui::BeginCombo("Theme", uiConfig->_theme.c_str()))
			{
				for (auto& theme : uiConfig->_themes)
					if (ImGui::Selectable(theme.first.c_str(), theme.first == uiConfig->_theme))
						uiConfig->_theme = theme.first;

				ImGui::EndCombo();
			}
			ToolTip("Set the colors of the interface.\n(psst: you can edit these in config.xml!)", &appConfig->_hoverTimer);

			//ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("Layer Set UI", (uiConfig->_layerUITypeNames[uiConfig->_layersUIType]).c_str()))
			{
				for (auto& ltype : uiConfig->_layerUITypeNames)
					if (ImGui::Selectable(ltype.second.c_str(), uiConfig->_layersUIType == ltype.first))
						uiConfig->_layersUIType = ltype.first;

				ImGui::EndCombo();
			}
			ToolTip("Choose the UI for Layer Set controls.", &appConfig->_hoverTimer);

			if (ImGui::BeginTable("##AppearanceOptions", 2, ImGuiTableFlags_SizingStretchSame))
			{
				ImGui::TableNextColumn();
				float transChkBoxPos = ImGui::GetCursorPosY();
				if (ImGui::Checkbox("Transparent", &appConfig->_transparent))
				{
					if (appConfig->_transparent)
					{
#ifdef WIN32
						if (appConfig->_isFullScreen)
						{
							appConfig->_scrW = appConfig->_fullScrW + 1;
							appConfig->_scrH = appConfig->_fullScrH + 1;
							appConfig->_window.create(sf::VideoMode(appConfig->_scrW, appConfig->_scrH), appConfig->windowName, 0);
							appConfig->_window.setIcon((int)uiConfig->_ico.getSize().x, (int)uiConfig->_ico.getSize().y, uiConfig->_ico.getPixelsPtr());
							appConfig->_window.setSize({ (sf::Uint16)appConfig->_scrW, (sf::Uint16)appConfig->_scrH });
							appConfig->_window.setPosition({ 0,0 });
							sf::View v = appConfig->_window.getView();
							v.setSize({ appConfig->_scrW, appConfig->_scrH });
							v.setCenter({ appConfig->_scrW / 2, appConfig->_scrH / 2 });
							appConfig->_window.setView(v);
						}

						auto hwnd = appConfig->_window.getSystemHandle();

						setWindowTransparency(hwnd, true);

						// TODO this doesn't work yet. Probably needs a modification to SFML itself to get it working
// #else
						//             Display* display = XOpenDisplay(NULL);
						//             if (display != NULL)
						//             {
						//                 Window wind = appConfig->_window.getSystemHandle();

						//                 setWindowTransparency(display, wind, appConfig->_transparent);
						//                 setWindowProperties(display, wind);

						//                 enableWindow(display, wind, true);
						//                 XCloseDisplay(display);
						//             }
#endif
					}
					else
					{
#ifdef WIN32
						setWindowTransparency(appConfig->_window.getSystemHandle(), false);
#endif
					}


				}
				ToolTip("Turns the background transparent (Useful for screen capture).", &appConfig->_hoverTimer);

				ImVec4 imCol = toImVec4(appConfig->_bgColor);
				ImGui::TableNextColumn();
				ImGui::BeginDisabled(appConfig->_transparent);
				if (ImGui::Button("Background Color", { -1, ImGui::GetFrameHeight() })) {
					ImGui::OpenPopup("BGColorEdit");
				}

				if (ImGui::BeginPopup("BGColorEdit"))
				{
					if (ImGui::ColorPicker3("##bgColor", &imCol.x, ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_DisplayHex | ImGuiColorEditFlags_DisplayRGB))
					{
						appConfig->_bgColor = toSFColor(imCol);
					}
					ImGui::EndPopup();
				}
				ToolTip("Set a background color for the window.", &appConfig->_hoverTimer);
				ImGui::EndDisabled();

				ImGui::TableNextColumn();
				ImGui::Checkbox("Show Layer Bounds", &uiConfig->_showLayerBounds);
				ToolTip("Shows a box around each layer, and\na marker for the pivot point.", &appConfig->_hoverTimer);

				ImGui::TableNextColumn();
				ImGui::Checkbox("Highlight Hovered", &uiConfig->_hilightHovered);
				ToolTip("Highlight the layer bounds when the layer's menu\nis expanded or hovered", &appConfig->_hoverTimer);

				ImGui::TableNextColumn();
				ImGui::Checkbox("Show FPS", &uiConfig->_showFPS);
				ToolTip("Shows an FPS counter (when menu is inactive).", &appConfig->_hoverTimer);

				ImGui::TableNextColumn();
				ImGui::InputFloat("UI Scale", &appConfig->customScaling, 0.1, 0.5, "%.1f");
				ToolTip("Change the size of the user interface.", &appConfig->_hoverTimer);

				ImGui::TableNextRow();
				ImGui::TableNextColumn();
				ImGui::BeginDisabled(appConfig->_transparent);
				ImGui::Checkbox("Composite onto Background", &appConfig->_compositeOntoBackground);
				ToolTip("Composite the layers onto the application background color.", &appConfig->_hoverTimer);
				ImGui::EndDisabled();

				ImGui::TableNextColumn();
				ImGui::Checkbox("Premultiply Alpha", &appConfig->_alphaPremultiplied);
				ToolTip("Multiply the output color by the alpha value.\nUseful for glow effects etc.", &appConfig->_hoverTimer);

				ImGui::EndTable();
			}

			ImGui::SeparatorText("Behaviour");

			if (ImGui::BeginTable("##BehaviourOptions", 2, ImGuiTableFlags_SizingStretchSame))
			{
				ImGui::TableNextColumn();
				ImGui::Checkbox("Create minimal layers", &appConfig->_createMinimalLayers);
				ToolTip("Create layers without any default movement or additional sprites", &appConfig->_hoverTimer);

				ImGui::TableNextColumn();
				ImGui::Checkbox("Mouse Tracking", &appConfig->_mouseTrackingEnabled);
				ToolTip("Override setting to enable/disable all mouse tracking on layers.", &appConfig->_hoverTimer);

				ImGui::TableNextColumn();
				ImGui::Checkbox("Check for updates", &appConfig->_checkForUpdates);
				ToolTip("Automatically check for updates when the application starts.", &appConfig->_hoverTimer);


				ImGui::EndTable();

				if (ImGui::BeginCombo("Number Edit type", uiConfig->_numbertypes[uiConfig->_numberEditType].c_str()))
				{
					for (auto& numType : uiConfig->_numbertypes)
						if (ImGui::Selectable(numType.second.c_str(), numType.first == uiConfig->_numberEditType))
							uiConfig->_numberEditType = numType.first;

					ImGui::EndCombo();
				}
				ToolTip("Set the colors of the interface.\n(psst: you can edit these in config.xml!)", &appConfig->_hoverTimer);

				if (ImGui::Checkbox("Unload images while hidden", &appConfig->_unloadTimeoutEnabled))
				{
					if (appConfig->_unloadTimeoutEnabled)
						appConfig->_unloadTimeout = appConfig->_unloadTimeoutSetting;

					layerMan->SetUnloadingTimer(appConfig->_unloadTimeout);

				}
				ToolTip("Remove images from RAM while they're not being used.\n  WARNING: expect a delay in visibility\n  while an unloaded image is reloading!", &appConfig->_hoverTimer);
				if (appConfig->_unloadTimeoutEnabled)
				{
					if(ImGui::DragInt("Unload timeout", &appConfig->_unloadTimeoutSetting, 0.1f, 1, 60, "%d s", ImGuiSliderFlags_Logarithmic))
						appConfig->_unloadTimeout = appConfig->_unloadTimeoutSetting;

					layerMan->SetUnloadingTimer(appConfig->_unloadTimeout);
					ToolTip("Set how long to wait before unloading an image.\n  If you set this to less than any\n  frequent states/blinks, expect stutter!", &appConfig->_hoverTimer);
				}


				ImGui::Checkbox("Disable Rotation Effect Fix", &appConfig->_undoRotationEffectFix);
				ToolTip("Disable the fix for Rotation Effect on this Layer Set.", &appConfig->_hoverTimer);
			}

			ImGui::SeparatorText("Integration");

			std::string portString = std::to_string(appConfig->_httpPort);
			if (ImGui::BeginTable("##IntegrationOptions", 2, ImGuiTableFlags_SizingStretchSame))
			{
				ImGui::TableNextColumn();
				if (ImGui::Checkbox("Control States via HTTP", &appConfig->_listenHTTP))
				{
					if (appConfig->_listenHTTP && appConfig->_webSocket != nullptr)
					{
						appConfig->_webSocket->Start(appConfig->_httpPort);
					}
					else if (!appConfig->_listenHTTP && appConfig->_webSocket != nullptr)
					{
						appConfig->_webSocket->Stop();
					}
				}
				ToolTip(("Listens for HTTP messages in the format:\nhttp://127.0.0.1:" + portString + "/state?[stateIndex,active]\nhttp://127.0.0.1:" + portString + "/state?[\"state name\",active]").c_str(), &appConfig->_hoverTimer);

				ImGui::TableNextColumn();
				ImGui::BeginDisabled(!appConfig->_listenHTTP);
				ImGui::InputInt("HTTP Port", &appConfig->_httpPort);
				if (ImGui::IsItemDeactivatedAfterEdit() && appConfig->_listenHTTP)
				{
					appConfig->_webSocket->Stop();
					appConfig->_webSocket->Start(appConfig->_httpPort);
				}
				ToolTip("Set the port that RahiTuber listens for messages on", &appConfig->_hoverTimer);
				ImGui::EndDisabled();

#ifdef _WIN32
				ImGui::TableNextColumn();
				ImGui::Checkbox("Use Spout2", &appConfig->_useSpout2Sender);
				ToolTip("Send video output through Spout2.\n(Requires Spout2 plugin for your streaming software)", &appConfig->_hoverTimer);

				ImGui::TableNextColumn();
				ImGui::SetNextItemWidth(ImGui::CalcTextSize("RawInput").x + UIUnit*2);
				if (ImGui::BeginCombo("Gamepad API", g_gamepadAPINames[appConfig->_gamepadAPI]))
				{
					for (int api = 0; api < GAMEPAD_API_END; api++)
						if (api != GAMEPAD_API_XINPUT)
						{
							if (ImGui::Selectable(g_gamepadAPINames[api], appConfig->_gamepadAPI == api))
							{
								appConfig->_gamepadAPI = api;
								GamePad::setAPI(GamepadAPI(api));
							}
							ToolTip(g_gamepadAPITooltips[api], &appConfig->_hoverTimer);
						}

					ImGui::EndCombo();
				}
				ToolTip("Select which method to read joystick status.", &appConfig->_hoverTimer);
#endif

				ImGui::EndTable();
			}

			if (ImGui::Button("OK", { -1, ImGui::GetFrameHeight() }))
			{
				ImGui::CloseCurrentPopup();
			}

			ImGui::PopStyleVar();

			uiConfig->_advancedMenuHeight - ImGui::GetWindowHeight();

			ImGui::EndPopup();
		}
	}

	void menuAudio(ImGuiStyle& style)
	{
		ImGui::PushStyleColor(ImGuiCol_Header, style.Colors[ImGuiCol_ChildBg]);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, style.Colors[ImGuiCol_BorderShadow]);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, style.Colors[ImGuiCol_BorderShadow]);
		uiConfig->_audioExpanded = ImGui::CollapsingHeader("Audio Input", (uiConfig->_audioExpanded ? ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_UpsideDownArrow : 0));
		ImGui::PopStyleColor(3);

		if (uiConfig->_firstMenu)
		{
			audioConfig->_nDevices = Pa_GetDeviceCount();
			audioConfig->_deviceList.clear();
			for (int dI = 0; dI < audioConfig->_nDevices; dI++)
			{
				auto info = Pa_GetDeviceInfo(dI);
				if (info->hostApi == 0 && info->maxInputChannels > 0)
					audioConfig->_deviceList.push_back({ info->name, dI });
			}
		}

		ImGui::PushID("AudImpCombo");
		ImGui::PushItemWidth(-1);
		std::string deviceName = "None";
		if (audioConfig->_deviceList.size() > audioConfig->_devIdx)
			deviceName = audioConfig->_deviceList[audioConfig->_devIdx].first;
		if (ImGui::BeginCombo("", deviceName.c_str()))
		{
			for (auto& dev : audioConfig->_deviceList)
			{
				bool active = audioConfig->_devIdx == dev.second;
				if (ImGui::Selectable(dev.first.c_str(), &active))
				{
					Pa_StopStream(audioConfig->_audioStr);
					Pa_CloseStream(audioConfig->_audioStr);

					auto info = Pa_GetDeviceInfo(dev.second);
					float sRate;

					audioConfig->_devIdx = dev.second;
					audioConfig->_params.device = audioConfig->_devIdx;
					audioConfig->_params.channelCount = Min(2, info->maxInputChannels);
					audioConfig->_params.suggestedLatency = info->defaultLowInputLatency;
					audioConfig->_params.hostApiSpecificStreamInfo = nullptr;
					sRate = info->defaultSampleRate;

					Pa_OpenStream(&audioConfig->_audioStr, &audioConfig->_params, nullptr, sRate, FRAMES_PER_BUFFER, paClipOff, recordCallback, audioConfig->_streamData);
					Pa_StartStream(audioConfig->_audioStr);

					audioConfig->_frameMax = audioConfig->_fixedMax; //audioConfig->_cutoff;
					audioConfig->_frameHi = 0;
					audioConfig->_runningAverage = 0;

					audioConfig->_midMax = audioConfig->_fixedMax; // audioConfig->_cutoff;
					audioConfig->_midHi = 0;
					audioConfig->_midAverage = 0;

					audioConfig->_bassMax = audioConfig->_fixedMax; // audioConfig->_cutoff;
					audioConfig->_bassHi = 0;
					audioConfig->_bassAverage = 0;

					audioConfig->_trebleMax = audioConfig->_fixedMax; // audioConfig->_cutoff;
					audioConfig->_trebleHi = 0;
					audioConfig->_trebleAverage = 0;

				}
			}
			ImGui::EndCombo();
		}
		ImGui::PopID();
		ImGui::PopItemWidth();
		ToolTip("Choose an audio input device.", &appConfig->_hoverTimer);

		if (uiConfig->_audioExpanded)
		{
			ImGui::BeginTable("##audioOptions", 3, ImGuiTableFlags_SizingStretchProp);
			ImGui::TableNextColumn();

			ImGui::PushItemWidth(100);
			ImGui::DragFloat("Max Level", &audioConfig->_fixedMax, 0.01, 0.0, 5.0, "%.2f");
			ImGui::PopItemWidth();
			ToolTip("The maximum volume used in audio analysis.", &appConfig->_hoverTimer);

			ImGui::TableNextColumn();

			ImGui::Checkbox("Soft Max", &audioConfig->_softMaximum);
			ToolTip("Allow the Max Level to increase temporarily if exceeded.", &appConfig->_hoverTimer);

			ImGui::TableNextColumn();

			ImGui::PushItemWidth(100);
			float percentVal = 1.0 / 60.0;
			float smooth = (61.0 - audioConfig->_smoothFactor) * percentVal;
			smooth = powf(smooth, 2.f);
			if (FloatSliderDrag("Soft Fall", &smooth, 0.0, 1.0, "%.2f", 0, uiConfig->_numberEditType))
			{
				smooth = Clamp(smooth, 0.0f, 1.0f);
				smooth = powf(smooth, 0.5);
				audioConfig->_smoothFactor = 61 - smooth / percentVal;
			}
			ImGui::PopItemWidth();
			ToolTip("Let audio level fall slowly after speaking.", &appConfig->_hoverTimer);

			ImGui::TableNextColumn();

			ImGui::Checkbox("Audio Filter", &audioConfig->_doFiltering);
			ToolTip("Basic low-latency filtering to cancel keyboard and mouse noise frequencies.", &appConfig->_hoverTimer);

			ImGui::TableNextColumn();

			ImGui::Checkbox("Compression", &audioConfig->_compression);
			ToolTip("Use a compression curve for audio levels\n(the difference in effect reduces as the volume nears maximum).", &appConfig->_hoverTimer);

			ImGui::EndTable();
		}
	}

	void menuPresets(ImGuiStyle& style)
	{
		if (ImGui::Button("Window Presets", { -1,ImGui::GetFrameHeight() }))
		{
			if (appConfig->_menuWindow.isOpen())
			{
				ImVec2 wSize = ImGui::GetCurrentWindow()->Size;
				ImGui::SetNextWindowPos({ wSize.x / 2 - 225, wSize.y / 3 });
			}
			else
			{
				ImGui::SetNextWindowPos({ appConfig->_scrW / 2 - 225, appConfig->_scrH / 3 });
			}
			ImGui::OpenPopup("Window Presets");
		}

		uiConfig->_presetPopupActive = false;

		bool p_open = true;
		if (ImGui::BeginPopupModal("Window Presets", &p_open, ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
		{
			uiConfig->_presetPopupActive = true;

			ImGui::SetWindowSize({ 450 * appConfig->scalingFactor,-1 });
			//ImGui::SetWindowPos({ appConfig->_scrW / 2 - 200, appConfig->_scrH / 3 });

			ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Save or load presets for window attributes");
			ImGui::Separator();
			std::string prevName = "";
			if (uiConfig->_presetNames.size()) prevName = uiConfig->_presetNames[uiConfig->_presetIdx];
			if (ImGui::BeginCombo("Preset", prevName.c_str()))
			{
				for (unsigned int p = 0; p < uiConfig->_presetNames.size(); p++)
				{
					bool selected = uiConfig->_presetIdx == p;
					if (ImGui::Selectable(uiConfig->_presetNames[p].c_str(), &selected))
					{
						uiConfig->_presetIdx = p;
					}
				}
				ImGui::EndCombo();
			}
			ImGui::SameLine();
			if (ImGui::Button("Load", { -1,ImGui::GetFrameHeight() }) && uiConfig->_presetNames.size())
			{
				//gameConfig->loadFromSettingsFile(gameConfig->m_presetNames[gameConfig->m_presetIdx]);
				if (appConfig->_loader->loadPreset(uiConfig->_presetNames[uiConfig->_presetIdx]))
				{
					uiConfig->_settingsFileBoxName.clear();
					uiConfig->_settingsFileBoxName.resize(30);
					int i = 0;
					for (auto& c : uiConfig->_presetNames[uiConfig->_presetIdx])
						uiConfig->_settingsFileBoxName[i++] = c;
				}
				uiConfig->_settingsFileBoxName.clear();

				if (uiConfig->_windowSettingsChanged)
				{
					initWindow();
					uiConfig->_windowSettingsChanged = false;
				}
			}
			ImGui::Separator();
			ImGui::TextColored(style.Colors[ImGuiCol_Text], "Save Preset");
			ImGui::InputText("Name", uiConfig->_settingsFileBoxName.data(), 30 * appConfig->scalingFactor);
			ImGui::SameLine();
			if (ImGui::Button("x", { ImGui::GetFrameHeight(),ImGui::GetFrameHeight() }))
			{
				uiConfig->_settingsFileBoxName.clear();
				uiConfig->_settingsFileBoxName.resize(30);
			}
			ImGui::SameLine();
			if (ImGui::Button("Use Current", { -1,ImGui::GetFrameHeight() }) && uiConfig->_presetNames.size())
			{
				uiConfig->_settingsFileBoxName.clear();
				uiConfig->_settingsFileBoxName.resize(30);
				int i = 0;
				for (auto& c : uiConfig->_presetNames[uiConfig->_presetIdx])
					uiConfig->_settingsFileBoxName[i++] = c;
			}

			bool overwriting = false;
			for (auto& n : uiConfig->_presetNames)
			{
				if (n == std::string(uiConfig->_settingsFileBoxName.data()))
				{
					overwriting = true;
					break;
				}
			}

			std::string saveCheckBox = "Save";
			if (overwriting) saveCheckBox = "Update";

			uiConfig->_clearWindowInfo = false;
			uiConfig->_saveWindowInfo = true;


			std::string saveName = "Save";
			if (overwriting) saveName = "Overwrite";

			std::string name(uiConfig->_settingsFileBoxName.data());

			if ((name != "") && ImGui::Button(saveName.c_str(), { -1,ImGui::GetFrameHeight() }))
			{
				if (std::find(uiConfig->_presetNames.begin(), uiConfig->_presetNames.end(), name) == uiConfig->_presetNames.end())
				{
					uiConfig->_presetNames.push_back(name);
					uiConfig->_presetIdx = uiConfig->_presetNames.size() - 1;
				}
				//gameConfig->saveToSettingsFile(gameConfig->m_presetNames[gameConfig->m_presetIdx]);
				appConfig->_loader->savePreset(uiConfig->_presetNames[uiConfig->_presetIdx]);
				uiConfig->_settingsFileBoxName.clear();
			}

			ImGui::EndPopup();
		}
	}

	void RefreshStyle(ImGuiStyle& style, ImGuiIO& io, sf::Color& backdropCol)
	{
		style.FrameRounding = 4 * appConfig->scalingFactor;
		style.FramePadding = { 3 * appConfig->scalingFactor, 3 * appConfig->scalingFactor };
		style.WindowTitleAlign = style.ButtonTextAlign = { 0.5f, 0.5f };
		style.ItemSpacing = { 3 * appConfig->scalingFactor, 3 * appConfig->scalingFactor };
		style.GrabMinSize = ImGui::GetFrameHeight();
		style.ScrollbarSize = 18 * appConfig->scalingFactor;
		style.FrameBorderSize = 0;
		style.AntiAliasedLines = true;
		style.AntiAliasedFill = true;
		style.DisabledAlpha = 0.7;

#ifdef _DEBUG
		io.ConfigDebugHighlightIdConflicts = true;
		io.ConfigDebugIsDebuggerPresent = ::IsDebuggerPresent();
		io.ConfigErrorRecovery = true;
		io.ConfigErrorRecoveryEnableDebugLog = true;
		io.ConfigErrorRecoveryEnableTooltip = true;
#else
		io.ConfigDebugIsDebuggerPresent = false;
		io.ConfigDebugHighlightIdConflicts = false;
		io.ConfigErrorRecoveryEnableTooltip = false;
		io.ConfigErrorRecovery = false;
#endif

		auto thisTheme = uiConfig->_themes[uiConfig->_theme];

		ImVec4 baseColor(thisTheme.first);

		ImVec4 col_dark2(baseColor.x * 0.2f, baseColor.y * 0.2f, baseColor.z * 0.2f, 1.f);
		ImVec4 col_dark1a(baseColor.x * 0.32f, baseColor.y * 0.32f, baseColor.z * 0.32f, 1.f);
		ImVec4 col_dark1(baseColor.x * 0.5f, baseColor.y * 0.5f, baseColor.z * 0.5f, 1.f);
		ImVec4 col_dark(baseColor.x * 0.6f, baseColor.y * 0.6f, baseColor.z * 0.6f, 1.f);
		ImVec4 col_med3(baseColor.x * 0.8f, baseColor.y * 0.8f, baseColor.z * 0.8f, 1.f);
		ImVec4 col_med2(baseColor.x * 0.8f, baseColor.y * 0.8f, baseColor.z * 0.8f, 1.f);
		ImVec4 col_med(baseColor.x, baseColor.y, baseColor.z, 1.f);

		ImVec4 col_border = col_dark;
		ImVec4 col_shadow = col_dark1;

		ImVec4 baseColor2 = thisTheme.second;

		ImVec4 col_light(baseColor2.x * 0.8f, baseColor2.y * 0.8f, baseColor2.z * 0.8f, 1.f);
		ImVec4 col_light2(baseColor2);
		ImVec4 col_light2a(mean(baseColor2.x, 0.6f), mean(baseColor2.y, 0.6f), mean(baseColor2.z, 0.6f), 1.f);
		ImVec4 col_light3(powf(baseColor2.x, .3f), powf(baseColor2.y, .3f), powf(baseColor2.z, .3f), 1.f);
		ImVec4 greyoutCol(col_light3 * ImVec4(1.0, 1.0, 1.0, 0.6));

		backdropCol = toSFColor(col_med);

		auto oldFont = uiConfig->_fontName;
		if (uiConfig->_theme.find("Contrast") != std::string::npos)
		{
			col_dark2 = col_dark1a = col_med3 = ImVec4(0, 0, 0, 1.f);
			col_dark = col_med = baseColor;
			col_dark1 = col_med2 = ImVec4(baseColor.x * 0.5, baseColor.y * 0.5, baseColor.z * 0.5, 1.f);

			backdropCol = toSFColor(col_dark2);

			col_light3 = ImVec4(1, 1, 0.8, 1);
			col_border = ImVec4(powf(baseColor.x, .8f), powf(baseColor.y, .8f), powf(baseColor.z, .8f), 1.f);

			style.FrameBorderSize = 2.f;
			style.SeparatorTextBorderSize = 4.f;
			style.ItemSpacing = { 3 * appConfig->scalingFactor, 5 * appConfig->scalingFactor };
		}

		uiConfig->_fontName = thisTheme.fontName;
		uiConfig->_fontSize = thisTheme.fontSize;

		uiConfig->_fontReloadNeeded = uiConfig->_fontName != oldFont;

		col_shadow = { 0,0,0,0 };

		style.Colors[ImGuiCol_WindowBg] = col_dark2;
		style.Colors[ImGuiCol_ChildBg] = style.Colors[ImGuiCol_PopupBg] = col_dark1a;
		style.Colors[ImGuiCol_FrameBgHovered] = col_med2;
		style.Colors[ImGuiCol_ScrollbarBg] = style.Colors[ImGuiCol_FrameBg] = col_dark;
		style.Colors[ImGuiCol_SliderGrab] = style.Colors[ImGuiCol_ScrollbarGrab] = style.Colors[ImGuiCol_FrameBgActive] = col_med3;
		style.Colors[ImGuiCol_Button] = style.Colors[ImGuiCol_Header] = style.Colors[ImGuiCol_Tab] = col_med;
		style.Colors[ImGuiCol_ScrollbarGrabActive] = style.Colors[ImGuiCol_ButtonActive] = style.Colors[ImGuiCol_HeaderActive] = style.Colors[ImGuiCol_SliderGrabActive] = style.Colors[ImGuiCol_TabHovered] = col_light2;
		style.Colors[ImGuiCol_ScrollbarGrabHovered] = style.Colors[ImGuiCol_ButtonHovered] = style.Colors[ImGuiCol_HeaderHovered] = style.Colors[ImGuiCol_TabActive] = col_light;
		style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_TitleBg] = style.Colors[ImGuiCol_TitleBgCollapsed] = col_dark;
		style.Colors[ImGuiCol_CheckMark] = style.Colors[ImGuiCol_Text] = col_light3;
		style.Colors[ImGuiCol_TextDisabled] = greyoutCol;
		style.Colors[ImGuiCol_Separator] = col_light2a;
		style.Colors[ImGuiCol_BorderShadow] = col_shadow;
		style.Colors[ImGuiCol_Border] = col_border;

		uiConfig->_styleLoaded = true;

		float UIUnit = ImGui::GetFrameHeight();

		auto menuSize = appConfig->_menuWindow.getSize();
		if (appConfig->_menuPopped && menuSize.x < UIUnit * 25)
		{
			menuSize.x = UIUnit * 25;
			appConfig->_menuWindow.setSize(menuSize);
		}

		uiConfig->_lastTheme = uiConfig->_theme;
	}

	void menu()
	{
		bool menuUnpopped = appConfig->_menuPopped == true && appConfig->_menuPopPending == false;
		bool menuPoppedNow = appConfig->_menuPopped == false && appConfig->_menuPopPending == true;

		if(menuPoppedNow)
			uiConfig->_lastTheme = "";

		appConfig->_menuPopped = appConfig->_menuPopPending;

		if (appConfig->_menuPopped)
		{
			ImGui::SFML::SetCurrentWindow(appConfig->_menuWindow);
			appConfig->scalingFactor = appConfig->menuWindowScaling * appConfig->customScaling;
			ImGui::SFML::Update(appConfig->_menuWindow, appConfig->_timer.getElapsedTime());
		}
		else
		{
			ImGui::SFML::SetCurrentWindow(appConfig->_window);
			appConfig->scalingFactor = appConfig->mainWindowScaling * appConfig->customScaling;
			ImGui::SFML::Update(appConfig->_window, appConfig->_timer.getElapsedTime());
		}

		if (ImGui::IsAnyItemHovered() == false)
			appConfig->_hoverTimer.restart();

		sf::Color backdropCol;

		ImGuiIO& io = ImGui::GetIO();

		auto& style = ImGui::GetStyle();

		if(uiConfig->_theme != uiConfig->_lastTheme)
			RefreshStyle(style, io, backdropCol);

		io.FontGlobalScale = appConfig->scalingFactor * 0.5;

		if (appConfig->_menuWindow.isOpen())
		{
			appConfig->_menuWindow.clear(toSFColor(style.Colors[ImGuiCol_FrameBg]));
		}

		// Main menu window

		float UIUnit = ImGui::GetFrameHeight();

		float windowHeight = appConfig->_scrH - 20;
		if (appConfig->_menuPopped == false)
		{
			ImGui::SetNextWindowPos(ImVec2(10, 10));
			ImGui::SetNextWindowSize({ UIUnit*25, windowHeight });

			sf::RectangleShape backdrop({ UIUnit*25 - 4, windowHeight - 6 * appConfig->scalingFactor });
			backdrop.setPosition(13, 13);
			backdrop.setFillColor(backdropCol);
			appConfig->_menuRT.draw(backdrop);

			ImGui::Begin("RahiTuber", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

		}
		else
		{
			sf::Vector2u windSize = appConfig->_menuWindow.getSize();
			windSize.y -= 4;
			windowHeight = windSize.y;
			ImGui::SetNextWindowPos(ImVec2(0, 0));
			ImGui::SetNextWindowSize(ImVec2(windSize.x, windSize.y));
			
			ImGui::Begin("RahiTuber", 0, ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoScrollbar);

		}

		ImDrawList* dList = ImGui::GetWindowDrawList();

		std::string menuShowName = "Close Menu (Esc)";
		std::string menuPopName = "Popout Menu";
		if (appConfig->_menuPopped)
		{
			menuPopName = "Dock Menu";
			if (uiConfig->_menuShowing)
				menuShowName = "Hide Interface (Esc)";
			else
				menuShowName = "Show Interface (Esc)";
		}

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2,0 });
		ImGui::BeginTable("##menuVisibility", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchSame);
		ImGui::TableNextColumn();
		if (ImGui::Button(menuShowName.c_str(), { -1, UIUnit }))
		{
			uiConfig->_menuShowing = !uiConfig->_menuShowing;
		}
		ImGui::TableNextColumn();
		if (ImGui::Button(menuPopName.c_str(), { -1, UIUnit }))
		{
			appConfig->_menuPopPending = !appConfig->_menuPopPending;
			layerMan->CloseAllPopups();
		}
		ImGui::EndTable();
		ImGui::PopStyleVar();

		//	FULLSCREEN
		if (ImGui::Button(appConfig->_isFullScreen ? "Exit Fullscreen (F11)" : "Fullscreen (F11)", { -1, UIUnit }))
		{
			swapFullScreen();
		}

		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 2,0 });
		ImGui::BeginTable("##menuVisibility", 2, ImGuiTableFlags_NoBordersInBody | ImGuiTableFlags_SizingStretchSame);
		ImGui::TableNextColumn();
		menuHelp(style);

		ImGui::TableNextColumn();

		menuAdvanced(style);
		ImGui::EndTable();
		ImGui::PopStyleVar();

		float fontFrameHeight = style.FramePadding.y * 2 + uiConfig->_fontSize*appConfig->scalingFactor / 2 + style.ItemSpacing.y / 2;

		ImGui::Separator();
		float separatorPos = ImGui::GetCursorPosY();
		float nextSectionPos = windowHeight - fontFrameHeight * 7.4;
		if (!uiConfig->_audioExpanded)
			nextSectionPos = windowHeight - fontFrameHeight * 5.2;

		layerMan->DrawGUI(style, nextSectionPos - separatorPos);

		//	INPUT LIST
		ImGui::SetCursorPosY(nextSectionPos);
		ImGui::Separator();
		menuAudio(style);

		ImGui::SetCursorPosY(windowHeight - fontFrameHeight * 2.7);
		ImGui::Separator();
		menuPresets(style);

		ImGui::Separator();

		float milliseconds = appConfig->_runTime.getElapsedTime().asMilliseconds();
		float pulse = sin(milliseconds / 300.f);

		//	EXIT
		ImGui::PushStyleColor(ImGuiCol_Button, style.Colors[ImGuiCol_FrameBg]);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.5f + 0.2f * pulse, 0.f, 0.f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.3f, 0.f, 0.f, 1.0f });
		if (ImGui::Button("Exit RahiTuber", { -1, UIUnit }))
		{
			if (appConfig->_menuWindow.isOpen())
				appConfig->_lastMenuPopPosition = appConfig->_menuWindow.getPosition();

			appConfig->_menuWindow.close();
			appConfig->_window.close();
		}
		ImGui::PopStyleColor(3);

		bool popupOpen = ImGui::IsPopupOpen(ImGuiID(0), ImGuiPopupFlags_AnyPopup);

		ImGui::End();

		if (appConfig->_menuPopped)
		{
			ImGui::EndFrame();
			ImGui::SFML::Render(appConfig->_menuWindow);
			ImGui::SFML::Update(appConfig->_window, appConfig->_timer.getElapsedTime());
		}

		if (!appConfig->_isFullScreen && uiConfig->_menuShowing)
		{
			// Move tab in the top centre
			ImGui::SetNextWindowPos({ appConfig->_scrW / 2 - 40 * appConfig->mainWindowScaling, 0 });
			ImGui::SetNextWindowSize({ uiConfig->_moveTabSize.x * appConfig->mainWindowScaling, uiConfig->_moveTabSize.y * appConfig->mainWindowScaling });

			ImGui::Begin("move_tab", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
			ImGui::SetCursorPos({ uiConfig->_moveTabSize.x / 2 - 12 * appConfig->mainWindowScaling,4 * appConfig->mainWindowScaling });
			ImGui::Image(uiConfig->_moveIconSprite, sf::Vector2f(24 * appConfig->mainWindowScaling, 24 * appConfig->mainWindowScaling), toSFColor(style.Colors[ImGuiCol_Text]));
			ImGui::End();
		}

		ImGui::EndFrame();
		ImGui::SFML::Render(appConfig->_menuRT);

		if (appConfig->_updateAvailable && !popupOpen)
		{
			sf::Color pulseColor(255u, 255u, 255u, 128u + 127u * pulse);
			sf::CircleShape dotShape(ImGui::GetFrameHeight()*0.26, 10);
			dotShape.setFillColor(toSFColor(style.Colors[ImGuiCol_ButtonActive]) * pulseColor);
			dotShape.setOutlineThickness(0.5);
			dotShape.setOutlineColor(toSFColor(style.Colors[ImGuiCol_ButtonHovered]) * pulseColor);

			if (appConfig->_menuPopped)
			{
				dotShape.setPosition(uiConfig->_helpBtnPosition);// / appConfig->menuWindowScaling);
				appConfig->_menuWindow.draw(dotShape);
			}
			else
			{
				dotShape.setPosition(uiConfig->_helpBtnPosition);
				appConfig->_menuRT.draw(dotShape);
			}
		}

		if (menuUnpopped)
		{
			appConfig->_lastMenuPopPosition = appConfig->_menuWindow.getPosition();
			appConfig->_menuWindow.close();
			appConfig->_window.requestFocus();
		}

		if (menuPoppedNow)
		{
			float scaleFactor = CheckMonitorScaleFactor(appConfig->_lastMenuPopPosition, { (sf::Uint16)(UIUnit * 25), (sf::Uint16)appConfig->_scrH + 4u });
			float scaleFactorDiff = scaleFactor / appConfig->mainWindowScaling;

			appConfig->_menuWindow.create(sf::VideoMode(scaleFactorDiff * UIUnit * 25, appConfig->_scrH * scaleFactorDiff + 4), "RahiTuber - Menu", sf::Style::Default | sf::Style::Resize | sf::Style::Titlebar);
			appConfig->_menuWindow.setIcon(uiConfig->_ico.getSize().x, uiConfig->_ico.getSize().y, uiConfig->_ico.getPixelsPtr());

			appConfig->_menuWindow.setPosition(appConfig->_lastMenuPopPosition);
			appConfig->_menuWindow.requestFocus();


			sf::Event dummyFocus;
			dummyFocus.type = sf::Event::GainedFocus;
			ImGui::SFML::ProcessEvent(appConfig->_menuWindow, dummyFocus);
		}

		uiConfig->_firstMenu = false;
	}

	void render()
	{
		auto dt = appConfig->_timer.restart();
		appConfig->_fps = (1.0f / dt.asSeconds());

		if (appConfig->_transparent)
		{
			appConfig->_window.clear(sf::Color(0, 0, 0, 0));
			appConfig->_window.setActive(true);
			glClearColor(0.0, 0.0, 0.0, 0.0);
			glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

			appConfig->_layersRT.clear(sf::Color(0, 0, 0, 0));
		}
		else
		{
			appConfig->_window.clear(appConfig->_bgColor);
			if (appConfig->_compositeOntoBackground)
				appConfig->_layersRT.clear(appConfig->_bgColor);
			else
				appConfig->_layersRT.clear(sf::Color(0, 0, 0, 0));

		}

		appConfig->_menuRT.clear(sf::Color(0, 0, 0, 0));

		float audioLevel = audioConfig->_midAverage;

		if (audioConfig->_compression)
		{
			audioLevel = Clamp(audioLevel, 0.0, 1.0);
			audioLevel = (1.0 - audioLevel) * sin(PI * 0.5 * audioLevel) + audioLevel * sin(PI * 0.5 * powf(audioLevel, 0.5));
			audioLevel = Clamp(audioLevel, 0.0, 1.0);
		}

		layerMan->Draw(&appConfig->_layersRT, appConfig->_scrH, appConfig->_scrW, audioLevel, audioConfig->_midMax);

		if (layerMan && layerMan->IsEmptyAndIdle())
		{
			// no layers, show the menu to avoid showing a blank screen
			uiConfig->_menuShowing = true;
		}

		appConfig->_RTPlane = sf::RectangleShape({ appConfig->_scrW, appConfig->_scrH });

		if (uiConfig->_fontReloadNeeded)
		{
			ImGui::SFML::SetCurrentWindow(appConfig->_menuWindow);
			logToFile(appConfig, "Loading font in menu window.");
			LoadCustomFont();

			ImGui::SFML::SetCurrentWindow(appConfig->_window);
			logToFile(appConfig, "Loading font in main window.");
			LoadCustomFont();
		}

		if (uiConfig->_menuShowing)
		{
			if (!appConfig->_isFullScreen)
			{
				if (appConfig->_transparent)
				{
					uiConfig->_outlineBox.setSize({ appConfig->_scrW - 4, appConfig->_scrH - 4 });
					appConfig->_menuRT.draw(uiConfig->_outlineBox);
				}

				appConfig->_menuRT.draw(uiConfig->_topLeftBox);
				appConfig->_menuRT.draw(uiConfig->_bottomRightBox);
			}
			menu();
		}
		else if (appConfig->_menuWindow.isOpen())
		{
			menu();
		}
		else if (layerMan && layerMan->IsLoading())
		{
			if (uiConfig->_styleLoaded == false)
			{
				sf::Color backdropCol;
				RefreshStyle(ImGui::GetStyle(), ImGui::GetIO(), backdropCol);
			}
			
			ImGui::SFML::Update(appConfig->_window, dt);

			ImGui::SetNextWindowSizeConstraints(ImGui::CalcTextSize("...RahiTuber is Loading..."), { 500, 500 });
			ImGui::SetNextWindowPos({ appConfig->_scrW / 2, appConfig->_scrH / 2 }, 0, { 0.5, 0.5 });
			ImGui::Begin("RahiTuber is Loading", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_AlwaysAutoResize);

			layerMan->DrawLoadingMessage();

			ImGui::End();
			ImGui::EndFrame();
			ImGui::SFML::Render(appConfig->_menuRT);
		}

		if (uiConfig->_showFPS && (!uiConfig->_menuShowing || appConfig->_menuPopped))
		{
			if (dt <= sf::Time::Zero)
				dt = sf::milliseconds(1);

			ImGui::SFML::Update(appConfig->_window, dt);

			ImGui::Begin("FPS", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);

			ImGui::Text("FPS: %d", (int)appConfig->_fps);

			ImGui::End();
			ImGui::EndFrame();
			ImGui::SFML::Render(appConfig->_menuRT);
		}

#ifdef _DEBUG
		//draw debug audio bars
		{
			std::lock_guard<std::mutex> guard(audioConfig->_freqDataMutex);

			auto frames = audioConfig->_frequencyData;
			float barW = appConfig->_scrW / (frames.size() / 2);

			for (UINT bar = 0; bar < frames.size() / 2; bar++)
			{
				float height = (frames[bar] / audioConfig->_bassMax) * appConfig->_scrH;
				appConfig->bars[bar].setSize({ barW, height });
				appConfig->bars[bar].setOrigin({ 0.f, height });
				appConfig->bars[bar].setPosition({ barW * bar, appConfig->_scrH });
				appConfig->_menuRT.draw(appConfig->bars[bar]);
			}
		}
#endif

		if (uiConfig->_cornerGrabbed.first)
		{
			appConfig->_menuRT.draw(uiConfig->_topLeftBox);
			appConfig->_menuRT.draw(uiConfig->_resizeBox);
		}
		else if (uiConfig->_cornerGrabbed.second)
		{
			appConfig->_menuRT.draw(uiConfig->_bottomRightBox);
			appConfig->_menuRT.draw(uiConfig->_resizeBox);
		}

		appConfig->_layersRT.display();
		appConfig->_RTPlane.setTexture(&appConfig->_layersRT.getTexture(), true);
		sf::RenderStates states = sf::RenderStates::Default;

		if (appConfig->_compositeOntoBackground && !appConfig->_transparent)
		{
			states.blendMode = sf::BlendMode(sf::BlendMode::SrcAlpha, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add,
				sf::BlendMode::One, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add);
		}
		else
		{
			auto srcColorMult = sf::BlendMode::One;
			if(appConfig->_alphaPremultiplied)
				srcColorMult = sf::BlendMode::SrcAlpha;

			states.blendMode = sf::BlendMode(srcColorMult, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add,
				sf::BlendMode::SrcAlpha, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add);
		}

		appConfig->_window.draw(appConfig->_RTPlane, states);

#ifdef _WIN32
		if (appConfig->_useSpout2Sender)
		{
			appConfig->_nameLock.lock();
			if (spout == nullptr)
			{
				spout = new Spout();
				spout->SetSenderName(appConfig->windowName.c_str());
				spout->SetAutoShare(true);
				spout->OpenSpout();

				if (spout->IsGLDXready() == false)
				{
					logToFile(appConfig, "Spout2: graphics hardware is not compatible with NVIDIA NV_DX_interop2 extension");
					spout->ReleaseSender();
					spout->SetSenderName(appConfig->windowName.c_str());
					spout->SetCPUshare(true);
					appConfig->_spoutNeedsCPU = true;
					spout->OpenSpout();

					if (spout->GetCPU() == false && spout->GetGLDX() == false)
					{
						logToFile(appConfig, "Spout2: couldn't find a method to share the texture");
						appConfig->_useSpout2Sender = false;
						spout->ReleaseSender();
						delete spout;
					}
				}
			}
			else if (appConfig->_pendingSpoutNameChange)
			{
				spout->ReleaseSender();
				spout->SetSenderName(appConfig->windowName.c_str());
				spout->SetAutoShare(true);
				if (appConfig->_spoutNeedsCPU)
					spout->SetCPUshare(true);

				spout->OpenSpout();
				appConfig->_pendingSpoutNameChange = false;
			}
			appConfig->_nameLock.unlock();

			bool result = spout->SendFbo(0, appConfig->_scrW, appConfig->_scrH, true);
			if (result == false)
			{
				logToFile(appConfig, "Spout2: Failed sending FBO");
			}
		}
#endif

		appConfig->_menuRT.display();
		appConfig->_RTPlane.setTexture(&appConfig->_menuRT.getTexture(), true);
		states.blendMode = sf::BlendMode(sf::BlendMode::SrcAlpha, sf::BlendMode::OneMinusSrcAlpha, sf::BlendMode::Add,
			sf::BlendMode::One, sf::BlendMode::One, sf::BlendMode::Add);
		appConfig->_window.draw(appConfig->_RTPlane, states);

		appConfig->_window.display();
		if (appConfig->_menuWindow.isOpen())
		{
			appConfig->_menuWindow.display();
		}
	}

	std::map<sf::Joystick::Axis, sf::Event> axisEvents;

	void RecordHotkey(sf::Event& evt, int& retFlag)
	{
		retFlag = 1;
		if (evt.type == evt.JoystickMoved
			&& Abs(evt.joystickMove.position) >= 30
			&& evt.joystickMove.joystickId != -1)
		{
			axisEvents[evt.joystickMove.axis] = evt;
		}

		if ((evt.type == evt.JoystickButtonPressed && evt.joystickButton.joystickId != -1)
			|| (evt.type == evt.JoystickButtonReleased && evt.joystickButton.joystickId != -1)
			|| evt.type == evt.MouseButtonPressed
			|| evt.type == evt.MouseButtonReleased)
		{
			bool keyDown = evt.type == evt.JoystickButtonPressed || evt.type == evt.MouseButtonPressed;

			if (layerMan->PendingHotkey() && keyDown)
			{
				layerMan->SetHotkeys(evt);
			}
		}

		if ((evt.type == evt.KeyPressed || evt.type == evt.KeyReleased)
			&& evt.key.code != sf::Keyboard::LControl
			&& evt.key.code != sf::Keyboard::LShift
			&& evt.key.code != sf::Keyboard::LAlt
			&& evt.key.code != sf::Keyboard::LSystem
			&& evt.key.code != sf::Keyboard::RControl
			&& evt.key.code != sf::Keyboard::RShift
			&& evt.key.code != sf::Keyboard::RAlt
			&& evt.key.code != sf::Keyboard::RSystem
			&& evt.key.code != sf::Keyboard::Escape)
		{

			bool keyDown = evt.type == evt.KeyPressed;

			if (layerMan->PendingHotkey() && keyDown)
			{
				layerMan->SetHotkeys(evt);
				if (uiConfig->_menuShowing == true && appConfig->_menuPopped == false)
					ImGui::SFML::ProcessEvent(appConfig->_window, evt);
				{ retFlag = 3; return; };
			}
		}
	}

	void handleEvents()
	{
		GamePad::update();


		sf::Event menuEvt;
		if (appConfig->_menuWindow.isOpen())
		{
			while (appConfig->_menuWindow.pollEvent(menuEvt))
			{

				int retFlag;
				RecordHotkey(menuEvt, retFlag);
				if (retFlag == 3) continue;

				if (menuEvt.type == menuEvt.Closed)
				{
					appConfig->_menuPopPending = false;
					// menu function will close it later
					break;
				}
				else if (menuEvt.type == menuEvt.KeyPressed && menuEvt.key.code == sf::Keyboard::F11 && menuEvt.key.control == false)
				{
					//toggle fullscreen
					swapFullScreen();
				}
				else if ((menuEvt.type == menuEvt.KeyPressed && menuEvt.key.code == sf::Keyboard::Escape))
				{
					//toggle the menu visibility
					uiConfig->_menuShowing = !uiConfig->_menuShowing;
					if (uiConfig->_menuShowing)
						uiConfig->_firstMenu = true;

					appConfig->_timer.restart();

					break;
				}

				if (uiConfig->_menuShowing && !uiConfig->_presetPopupActive && !uiConfig->_helpPopupActive &&
					(menuEvt.type == menuEvt.MouseButtonPressed || menuEvt.type == menuEvt.MouseButtonReleased || menuEvt.type == menuEvt.MouseMoved))
				{
					auto pos = sf::Mouse::getPosition(appConfig->_menuWindow);
					bool mousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
					if (layerMan->HandleLayerDrag(pos.x, pos.y, mousePressed))
						continue;
				}

				if (menuEvt.type == menuEvt.Resized)
				{
					auto windowPos = appConfig->_menuWindow.getPosition();
					sf::Uint16 wHeight = appConfig->_menuWindow.getSize().y;

					float scaleFactor = CheckMonitorScaleFactor(windowPos, { (sf::Uint16)(480u * appConfig->customScaling), wHeight });

					if (scaleFactor != appConfig->menuWindowScaling)
					{
						float scaleFactorDiff = scaleFactor / appConfig->menuWindowScaling;

						appConfig->_menuWindow.setSize({ (sf::Uint16)(480u * scaleFactor * appConfig->customScaling), (sf::Uint16)(wHeight * scaleFactorDiff) });
						appConfig->menuWindowScaling = scaleFactor;

						ImGui::SFML::Init(appConfig->_menuWindow);
					}
				}

				if (menuEvt.type == menuEvt.KeyPressed || menuEvt.type == menuEvt.MouseButtonPressed)
				{
					appConfig->_menuWindow.requestFocus();
				}

				ImGui::SFML::ProcessEvent(appConfig->_menuWindow, menuEvt);
			}
		}

		if (appConfig->_window.hasFocus() && appConfig->_isFullScreen && (!uiConfig->_menuShowing || appConfig->_menuPopped))
		{
			appConfig->_window.setMouseCursorVisible(false);
		}
		else
		{
			appConfig->_window.setMouseCursorVisible(true);
		}

		if (uiConfig->_cornerGrabbed.first || uiConfig->_cornerGrabbed.second)
		{
			appConfig->_window.requestFocus();
		}

		layerMan->CheckHotkeys();

		sf::Event evt;
		while (appConfig->_window.pollEvent(evt))
		{
			if (evt.type == evt.KeyPressed || evt.type == evt.MouseButtonPressed)
			{
				appConfig->_window.requestFocus();
			}

			if (uiConfig->_menuShowing && !uiConfig->_presetPopupActive && !uiConfig->_helpPopupActive &&
				appConfig->_menuWindow.isOpen() == false && (evt.type == evt.MouseButtonPressed || evt.type == evt.MouseButtonReleased || evt.type == evt.MouseMoved))
			{
				auto pos = sf::Mouse::getPosition(appConfig->_window);
				bool mousePressed = sf::Mouse::isButtonPressed(sf::Mouse::Left);
				if (layerMan->HandleLayerDrag(pos.x, pos.y, mousePressed))
					continue;
			}

			int retFlag;
			RecordHotkey(evt, retFlag);
			if (retFlag == 3) continue;

			if (evt.type == evt.Closed)
			{
				layerMan->SaveLayers("lastLayers.xml");
				//close the application
				appConfig->_window.close();
				appConfig->_menuWindow.close();
				break;
			}
			else if ((evt.type == evt.KeyPressed && evt.key.code == sf::Keyboard::Escape) || (evt.type == evt.MouseButtonPressed && sf::Mouse::isButtonPressed(sf::Mouse::Right)))
			{
				//toggle the menu visibility
				uiConfig->_menuShowing = !uiConfig->_menuShowing;
				if (uiConfig->_menuShowing)
					uiConfig->_firstMenu = true;

				appConfig->_timer.restart();

				appConfig->_window.requestFocus();
				ImGui::SFML::SetCurrentWindow(appConfig->_window);

				break;
			}
			else if (evt.type == evt.MouseButtonPressed)
			{
				//check if user clicked in the corners for window resize
				if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && !appConfig->_isFullScreen)
				{
					float cornerGrabSize = 20 * appConfig->mainWindowScaling;

					auto pos = sf::Mouse::getPosition(appConfig->_window);
					if (pos.x < cornerGrabSize && pos.y < cornerGrabSize)
						uiConfig->_cornerGrabbed = { true, false };

					if (pos.x > appConfig->_scrW - cornerGrabSize && pos.y > appConfig->_scrH - cornerGrabSize)
						uiConfig->_cornerGrabbed = { false, true };


					if (pos.x > (appConfig->_scrW / 2 - uiConfig->_moveTabSize.x / 2) && pos.x < (appConfig->_scrW / 2 + uiConfig->_moveTabSize.x / 2)
						&& pos.y < uiConfig->_moveTabSize.y)
					{
						uiConfig->_moveGrabbed = true;
					}
				}
				else if (evt.mouseButton.button == sf::Mouse::Middle)
				{
					uiConfig->_middleClickGrabbed = true;
				}
			}
			else if (evt.type == evt.KeyPressed && evt.key.code == sf::Keyboard::F11 && evt.key.control == false)
			{
				//toggle fullscreen
				swapFullScreen();
			}
			else if (evt.type == evt.MouseButtonReleased)
			{
				auto windowPos = appConfig->_window.getPosition();

				//set the window if it was being resized
				if (uiConfig->_cornerGrabbed.first || uiConfig->_cornerGrabbed.second)
				{
					windowPos += sf::Vector2i(uiConfig->_resizeBox.getPosition());
					appConfig->_scrX = windowPos.x;
					appConfig->_scrY = windowPos.y;
					appConfig->_window.setPosition(windowPos);
					appConfig->_minScrH = uiConfig->_resizeBox.getGlobalBounds().height;
					appConfig->_minScrW = uiConfig->_resizeBox.getGlobalBounds().width;
					uiConfig->_cornerGrabbed = { false, false };

					initWindow();
				}

				if (uiConfig->_moveGrabbed || uiConfig->_middleClickGrabbed)
				{
#ifdef _WIN32
					float scaleFactor = CheckMonitorScaleFactor(windowPos, appConfig->_window.getSize());

					if (scaleFactor != appConfig->mainWindowScaling)
					{
						appConfig->mainWindowScaling = scaleFactor;
						initWindow();

						ImGui::SFML::Init(appConfig->_window);
					}
#endif
				}

				uiConfig->_lastMiddleClickPosition = sf::Vector2i(-1, -1);
				uiConfig->_moveGrabbed = false;
				uiConfig->_middleClickGrabbed = false;
			}
			else if (evt.type == evt.MouseMoved)
			{
				auto pos = sf::Vector2f(sf::Mouse::getPosition(appConfig->_window));
				if (uiConfig->_cornerGrabbed.first)
				{
					uiConfig->_topLeftBox.setPosition(pos);
					uiConfig->_resizeBox.setPosition(pos);
					uiConfig->_resizeBox.setSize({ appConfig->_scrW - pos.x, appConfig->_scrH - pos.y });
				}
				else if (uiConfig->_cornerGrabbed.second)
				{
					uiConfig->_bottomRightBox.setPosition({ pos.x - 20, pos.y - 20 });
					uiConfig->_resizeBox.setPosition(0, 0);
					uiConfig->_resizeBox.setSize({ pos.x, pos.y });
				}
				else if (!appConfig->_isFullScreen && (uiConfig->_moveGrabbed || uiConfig->_middleClickGrabbed))
				{
					auto mousePos = sf::Mouse::getPosition();
					sf::Vector2i windowPos;
					if (uiConfig->_moveGrabbed)
					{
						windowPos = mousePos - sf::Vector2i(appConfig->_scrW / 2.f, uiConfig->_moveTabSize.y / 2.f);
					}
					else if (uiConfig->_middleClickGrabbed)
					{
						if (uiConfig->_lastMiddleClickPosition == sf::Vector2i(-1, -1))
							uiConfig->_lastMiddleClickPosition = sf::Mouse::getPosition(appConfig->_window);

						windowPos = mousePos - uiConfig->_lastMiddleClickPosition;
					}
					appConfig->_scrX = windowPos.x;
					appConfig->_scrY = windowPos.y;
					appConfig->_window.setPosition(windowPos);
				}
			}

			if (uiConfig->_menuShowing && appConfig->_menuWindow.isOpen() == false)
				ImGui::SFML::ProcessEvent(appConfig->_window, evt);
		}

		for (auto& posn : axisEvents)
		{
			if (posn.second.type == sf::Event::JoystickMoved)
			{
				auto& jMove = posn.second.joystickMove;

				if (Abs(jMove.position) >= 30)
				{
					// active / held
					if (layerMan->PendingHotkey())
						layerMan->SetHotkeys(posn.second);

					// update it in case it's been released and didn't trigger an event
					jMove.position = GamePad::getAxisPosition(jMove.joystickId, jMove.axis);
				}
				else
				{
					jMove.position = 0.f;
					// released
					// update it in case it's been released and didn't trigger an event
					jMove.position = GamePad::getAxisPosition(jMove.joystickId, jMove.axis);
				}
			}
		}
	}

	void doAudioAnalysis()
	{
		if (audioConfig->_processedNew)
		{
			audioConfig->_recordTimer.restart();
			audioConfig->_processedNew = false;

			audioConfig->_bassHi = 0;
			audioConfig->_midHi = 0;
			audioConfig->_trebleHi = 0;
			audioConfig->_frameHi = 0;

			//Do fourier transform
			{ //lock for frequency data
				std::lock_guard<std::mutex> guard(audioConfig->_freqDataMutex);
				if (audioConfig->_frames.size() >= (FRAMES_PER_BUFFER * 2))
					audioConfig->_fftData = std::vector<SAMPLE>(audioConfig->_frames.begin(), audioConfig->_frames.begin() + (FRAMES_PER_BUFFER * 2));
			} //end lock

			auto halfSize = audioConfig->_fftData.size() / 2;

			four1(audioConfig->_fftData.data(), halfSize);
			float factor = 50;
			audioConfig->_frequencyData.clear();

			for (unsigned int it = 0; it < halfSize; it++)
			{
				auto re = audioConfig->_fftData[it];
				auto im = audioConfig->_fftData[it + 1];
				auto magnitude = std::sqrt(re * re + im * im);

				//Compensations for the fact my FFT implementation is probably wrong
				float point = it / factor + 0.3f;
				magnitude = magnitude * atan(point);
				if (it == 0) magnitude *= 0.7f;

				//Store the magnitude
				audioConfig->_frequencyData.push_back(magnitude);

				//store data for bass and treble
				if (it < FRAMES_PER_BUFFER / 12 && magnitude > audioConfig->_bassHi)
					audioConfig->_bassHi = magnitude;

				if (it > FRAMES_PER_BUFFER / 12 && it < FRAMES_PER_BUFFER / 3 && magnitude > audioConfig->_midHi)
					audioConfig->_midHi = magnitude;

				if (it > FRAMES_PER_BUFFER / 3 && it < FRAMES_PER_BUFFER / 2 && magnitude > audioConfig->_trebleHi)
					audioConfig->_trebleHi = magnitude;

				if (magnitude > audioConfig->_frameHi && it < FRAMES_PER_BUFFER / 2)
					audioConfig->_frameHi = magnitude;
			}
		}
		else if (audioConfig->_recordTimer.getElapsedTime().asSeconds() > (0.1))
		{
			// 0.1s without any audio input, clear the data
			audioConfig->_bassHi = 0;
			audioConfig->_midHi = 0;
			audioConfig->_trebleHi = 0;
			audioConfig->_frameHi = 0;
		}


		if (appConfig->_fps != 0)
			audioConfig->_smoothAmount = appConfig->_fps / audioConfig->_smoothFactor;
		else
			audioConfig->_smoothAmount = 1.0;

	
		//update audio data for this frame
		audioConfig->_frame = Abs(audioConfig->_frameHi);

		audioConfig->_runningAverage -= audioConfig->_runningAverage / audioConfig->_smoothAmount;
		audioConfig->_runningAverage += audioConfig->_frame / audioConfig->_smoothAmount;
		if (audioConfig->_frame > audioConfig->_runningAverage)
			audioConfig->_runningAverage = audioConfig->_frame;

		if (audioConfig->_frame > audioConfig->_fixedMax && audioConfig->_softMaximum)
			audioConfig->_frameMax = audioConfig->_frame;
		else
			audioConfig->_frameMax = audioConfig->_fixedMax;

		if (audioConfig->_bassHi < 0) audioConfig->_bassHi *= -1.0;

		audioConfig->_bassAverage -= audioConfig->_bassAverage / audioConfig->_smoothAmount;
		audioConfig->_bassAverage += audioConfig->_bassHi / audioConfig->_smoothAmount;
		if (audioConfig->_bassHi > audioConfig->_bassAverage)
			audioConfig->_bassAverage = audioConfig->_bassHi;

		if (audioConfig->_bassHi > audioConfig->_fixedMax && audioConfig->_softMaximum)
			audioConfig->_bassMax = audioConfig->_bassHi;
		else
			audioConfig->_bassMax = audioConfig->_fixedMax;

		if (audioConfig->_midHi < 0) audioConfig->_midHi *= -1.0;

		float midHi = audioConfig->_midHi;
		if (audioConfig->_doFiltering)
			midHi = Max(0.f, midHi - (audioConfig->_trebleHi + 0.2f * audioConfig->_bassHi));

		audioConfig->_midAverage -= audioConfig->_midAverage / audioConfig->_smoothAmount;
		audioConfig->_midAverage += midHi / audioConfig->_smoothAmount;
		if (midHi > audioConfig->_midAverage)
			audioConfig->_midAverage = midHi;

		if (midHi > audioConfig->_fixedMax && audioConfig->_softMaximum)
			audioConfig->_midMax = midHi;
		else
			audioConfig->_midMax = audioConfig->_fixedMax;

		if (audioConfig->_trebleHi < 0) audioConfig->_trebleHi *= -1.0;

		audioConfig->_trebleAverage -= audioConfig->_trebleAverage / audioConfig->_smoothAmount;
		audioConfig->_trebleAverage += audioConfig->_trebleHi / audioConfig->_smoothAmount;
		if (audioConfig->_trebleHi > audioConfig->_trebleAverage)
			audioConfig->_trebleAverage = audioConfig->_trebleHi;

		if (audioConfig->_trebleHi > audioConfig->_fixedMax && audioConfig->_softMaximum)
			audioConfig->_trebleMax = audioConfig->_trebleHi;
		else
			audioConfig->_trebleMax = audioConfig->_fixedMax;


		//As long as the music is loud enough the current max is good
		if (audioConfig->_frame > audioConfig->_cutoff * 2)
		{
			audioConfig->_quietTimer.restart();
		}
		else if (audioConfig->_quietTimer.getElapsedTime().asSeconds() > 0.3)
		{
			//if the quietTimer reaches 1.5s, start reducing the max

			float maxFallSpeed = 0.001f;

			audioConfig->_frameMax -= (audioConfig->_frameMax - (audioConfig->_cutoff * 2)) * maxFallSpeed;
			audioConfig->_bassMax -= (audioConfig->_bassMax - (audioConfig->_cutoff * 2)) * maxFallSpeed;
			audioConfig->_midMax -= (audioConfig->_midMax - (audioConfig->_cutoff * 2)) * maxFallSpeed;
			audioConfig->_trebleMax -= (audioConfig->_trebleMax - (audioConfig->_cutoff * 2)) * maxFallSpeed;

			if (audioConfig->_frameMax < audioConfig->_fixedMax)
				audioConfig->_frameMax = audioConfig->_fixedMax;

			if (audioConfig->_bassMax < audioConfig->_fixedMax)
				audioConfig->_bassMax = audioConfig->_fixedMax;

			if (audioConfig->_midMax < audioConfig->_fixedMax)
				audioConfig->_midMax = audioConfig->_fixedMax;

			if (audioConfig->_trebleMax < audioConfig->_fixedMax)
				audioConfig->_trebleMax = audioConfig->_fixedMax;

		}

		
	}

	void CheckUpdates()
	{
		appConfig->_checkUpdateThread = new std::thread([&]
			{

				std::string queryTxt = appConfig->_appLocation + "updateQuery.txt";

				std::string cmd = "curl \"https://itch.io/api/1/x/wharf/latest?target=rahisaurus/rahituber&channel_name=win\" > \"" + queryTxt + "\"";

				if (runProcess(cmd, true))
				{
					std::ifstream updateFile;
					updateFile.open(queryTxt);
					if (updateFile)
					{
						updateFile.seekg(0, updateFile.end);
						int length2 = updateFile.tellg();
						updateFile.seekg(0, updateFile.beg);

						if (length2 != 0)
						{
							std::string buf2;
							buf2.resize(length2 + 1, 0);
							updateFile.read(buf2.data(), length2);

							if (buf2.find("\"latest\"") != std::string::npos)
							{
								size_t numPos = buf2.find_first_not_of("{\"latest\":\"");

								float latest = std::stod(buf2.substr(numPos), nullptr);
								appConfig->_versionAvailable = latest;
								appConfig->_updateAvailable = appConfig->_versionNumber < latest;

								logToFile(appConfig, "Available version: " + std::to_string(latest));
							}
						}
						updateFile.close();

						std::remove(queryTxt.c_str());

					}
				}
				else
				{
#ifdef _WIN32
					printf("CreateProcess failed (%d).\n", GetLastError());
#endif
				}
			});
	}

	void showFirstStartupWindow(bool& cancelStart)
	{
		bool startWindowOpen = true;
		sf::RenderWindow tmpStartWindow;
		tmpStartWindow.create(sf::VideoMode(1024, 720, 32), "RahiTuber Startup", sf::Style::Titlebar);
		ImGui::SFML::Init(tmpStartWindow);
		ImGui::SFML::SetCurrentWindow(tmpStartWindow);
		ImGui::StyleColorsClassic();
		tmpStartWindow.setActive();
		tmpStartWindow.setVisible(true);
		sf::Clock dt;

		auto& style = ImGui::GetStyle();
		auto& io = ImGui::GetIO();

		auto oldFnt = uiConfig->_fontName;
		uiConfig->_fontName = "res/verdana.ttf";
		
		LoadCustomFont();
		sf::Color backCol;

		RefreshStyle(style, io, backCol);
		
		style.ScaleAllSizes(2);
		io.FontGlobalScale = 0.8;
		style.WindowPadding = { 100, 40 };
		io.FontDefault->Descent = 30;

		while (startWindowOpen)
		{
			ImGui::SFML::Update(tmpStartWindow, dt.restart());

			ImGui::SetNextWindowSize({ 1024, 800 }, ImGuiCond_Appearing);
			ImGui::SetNextWindowPos({ 0, 0 }, ImGuiCond_Appearing);
			ImGui::Begin("RahiTuber Startup##internal", 0, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove);
			ImGui::TextWrapped(R"intro(Welcome to RahiTuber!

Please read and accept the license below.)intro");

			ImGui::PushStyleVarX(ImGuiStyleVar_WindowPadding, 20);
			ImGui::BeginChild("LicenseScroll", { 824, 400 }, ImGuiChildFlags_Border, ImGuiWindowFlags_AlwaysVerticalScrollbar);

			ImGui::TextWrapped(R"intro(--- BSD License ---
Copyright (c) 2025, Rahisaurus (Tom Rule). All rights reserved.

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

-    Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.
-    Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.
-    All advertising materials mentioning features or use of this software must display the following acknowledgement: This product includes software developed by Rahisaurus.
-    Neither the name of Rahisaurus / Tom Rule nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY RAHISAURUS (TOM RULE) AS IS AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL RAHISAURUS (TOM RULE) BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
---

You are free to use this software for any streams or video content on any platform.
There is no obligation to credit me, but I would appreciate if you could use #RahiTuber when talking about it on social media, or add a link to https://rahisaurus.itch.io/rahituber and/or credit me (Tom Rule / Rahisaurus) in your content description, if you want.

As the user, you take responsibility for the content you display using this software.

This software is pay-what-you-want and the only official download site is itch.io. The developer accepts no responsibity for any other download source, or any payments made to any other site. Please report any other sites hosting it to me.
			)intro");

			ImGui::EndChild();
			ImGui::PopStyleVar();


			ImGui::TextWrapped(R"intro(
By continuing to use this software, you accept these terms.
If you accept, please click the Accept button.
			)intro");

			if (LesserButton("Cancel"))
			{
				cancelStart = true;
				startWindowOpen = false;
			}

			ImGui::SameLine();
			if (ImGui::Button("Accept"))
			{
				startWindowOpen = false;
			}
			ImGui::End();

			tmpStartWindow.clear();
			ImGui::SFML::Render(tmpStartWindow);
			tmpStartWindow.display();

			sf::Event ev;
			while (tmpStartWindow.pollEvent(ev))
			{
				ImGui::SFML::ProcessEvent(ev);
			}
		}

		ImGui::SFML::Shutdown();
	}

	void Init()
	{
		PaError err = paNoError;

		time_t current_time = time(nullptr);

#ifdef _WIN32
		char buf[26];
		ctime_s(buf, sizeof(buf), &current_time);
		std::string time_string(buf);
		time_string.erase(24);
#else
		char buf[26];
		std::strftime(buf, sizeof(buf), "%c", std::localtime(&current_time));
		std::string time_string(buf);
#endif

		std::srand(time(0));

		appConfig = new AppConfig();

#ifdef _WIN32
		CHAR fname[512];
		GetModuleFileNameA(NULL, fname, 512);
		if (GetLastError() == ERROR_SUCCESS)
		{
			fs::path exe = fname;
			appConfig->_appLocation = exe.parent_path().string() + "\\";
		}

		SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
#else
		appConfig->_appLocation = getAppLocation();
#endif

		logToFile(appConfig, "Creating UIConfig");
		uiConfig = new UIConfig();

		bool cancelStart = false;
		std::error_code ec = {};
		if (!fs::exists(appConfig->_appLocation + "config.xml", ec ))
		{
			showFirstStartupWindow(cancelStart);
		}

		if (cancelStart)
		{
			logToFile(appConfig, "RahiTuber init cancelled " + time_string, true);
			return;
		}

		logToFile(appConfig, "RahiTuber started " + time_string, true);

		appConfig->lastLayerSettingsFile = appConfig->_appLocation + "lastLayers.xml";

		logToFile(appConfig, "Creating AudioConfig");
		audioConfig = new AudioConfig();
		g_audioConfig = audioConfig;

		logToFile(appConfig, "Creating LayerManager");
		layerMan = new LayerManager();
		logToFile(appConfig, "Initialising LayerManager");
		layerMan->Init(appConfig, uiConfig);

		//kbdTrack = new KeyboardTracker();
		//kbdTrack->_layerMan = layerMan;

		GetWindowSizes();

		logToFile(appConfig, "Creating XML loader");
		appConfig->_loader = new xmlConfigLoader(appConfig, uiConfig, audioConfig, appConfig->_appLocation + "config.xml");

		bool retry = true;
		while (retry)
		{
			retry = false;
			bool loadValid = false;
			logToFile(appConfig, "Loading common settings");
			loadValid |= appConfig->_loader->loadCommon();

			logToFile(appConfig, "Loading preset settings");
			loadValid |= appConfig->_loader->loadPresetNames();

			if (loadValid == false)
			{
				logToFile(appConfig, "Failed to load config.xml");
#ifdef _WIN32
				std::wstring message(L"Failed to load config.xml.");
				std::string errMsgCopy = appConfig->_loader->_errorMessage;
				if (errMsgCopy.empty() == false)
				{
					message += L"\n\nMessage: ";
					message += std::wstring(errMsgCopy.begin(), errMsgCopy.end());
				}
				message += L"\n\nPress OK to recreate config.xml and try again. Press Cancel to try manually fixing the config.xml file.";

				int result = MessageBox(NULL, message.c_str(), L"Load failed", MB_ICONERROR | MB_OKCANCEL);
				if (result == IDOK)
				{
#else
				std::string message("Failed to load config.xml.");
				if (appConfig->_loader->_errorMessage.empty() == false)
				{
					message += "\n\nMessage: ";
					message += appConfig->_loader->_errorMessage;
				}
				message += "\n\nPress 'y' to recreate config.xml and try again. Press 'n' to try manually fixing the config.xml file.";

				std::cout << message << std::endl;
				char result;
				std::cin >> result;

				if (result == 'y')
				{
#endif
					retry = true;
					int ret = remove((appConfig->_appLocation + "config.xml").c_str());

					if (ret == 0 || errno == ENOENT)
					{
						printf("File deleted successfully");
						appConfig->_loader->saveCommon();
						break;
					}
					else
					{
#ifdef _WIN32
						int result = MessageBox(NULL, L"Could not delete config.xml. Stopping.", L"Error", MB_ICONERROR | MB_OK);
#endif
						retry = false;
					}
				}

				if (retry == false)
				{
					delete appConfig;
					delete uiConfig;
					delete audioConfig;
					delete layerMan;
#ifdef _WIN32
					if (spout != nullptr)
					{
						spout->ReleaseSender();
						delete spout;
					}
#endif
					//delete kbdTrack;
					return;
				}
			}
		}

		if (appConfig->_unloadTimeoutEnabled)
			appConfig->_unloadTimeout = appConfig->_unloadTimeoutSetting;
		else
			appConfig->_unloadTimeout = 0;

		{ // scope to destruct verFile when finished
			logToFile(appConfig, "Checking Version");
			std::ifstream verFile;
			verFile.open(appConfig->_appLocation + "buildnumber.txt");
			if (verFile)
			{
				verFile.seekg(0, verFile.end);
				int length = verFile.tellg();
				verFile.seekg(0, verFile.beg);

				std::string buf;
				buf.resize(length + 1, 0);
				verFile.read(buf.data(), length);
				appConfig->_versionNumber = std::stod(buf, nullptr);

				logToFile(appConfig, "Current version: " + buf);

				if (appConfig->_checkForUpdates)
				{
					logToFile(appConfig, "Checking for Updates");
					CheckUpdates();
				}

				verFile.close();
			}
		}

		logToFile(appConfig, "Initialising app windows");
		initWindow(true);

		if (appConfig->_lastLayerSet.empty())
		{
			layerMan->LoadLayers(appConfig->lastLayerSettingsFile);
		}

		layerMan->SetLayerSet(appConfig->_lastLayerSet);

		if (appConfig->_lastLayerSet.empty() == false)
			layerMan->LoadLayers(appConfig->_lastLayerSet);

		//kbdTrack->SetHook(appConfig->_useKeyboardHooks);

		uiConfig->_menuShowing = uiConfig->_showMenuOnStart;

		if (appConfig->_startMaximised)
			appConfig->_isFullScreen = true;

		uiConfig->_ico.loadFromFile(appConfig->_appLocation + "res/icon.png");
		uiConfig->_settingsFileBoxName.resize(30);

		uiConfig->_moveIcon.loadFromFile(appConfig->_appLocation + "res/move.png");
		uiConfig->_moveIcon.setSmooth(true);
		uiConfig->_moveIconSprite.setTexture(uiConfig->_moveIcon, true);

		ImGui::SFML::Init(appConfig->_window);
		ImGui::SFML::Init(appConfig->_menuWindow);

		ImGui::GetStyle().WindowRounding = 4.f;
		ImGui::GetStyle().Alpha = 1.0;

		ImGui::SFML::SetCurrentWindow(appConfig->_menuWindow);
		logToFile(appConfig, "Loading font in menu window.");
		LoadCustomFont();

		ImGui::SFML::SetCurrentWindow(appConfig->_window);
		logToFile(appConfig, "Loading font in main window.");
		LoadCustomFont();

		//setup debug bars
		audioConfig->_frames.resize(FRAMES_PER_BUFFER * 3);
		appConfig->bars.resize(FRAMES_PER_BUFFER / 2);
		float barW = appConfig->_scrW / (FRAMES_PER_BUFFER / 2);
		for (int b = 0; b < appConfig->bars.size(); b++)
		{
			appConfig->bars[b].setPosition((float)b * barW, 0);
			appConfig->bars[b].setSize({ barW, 0.f });
			appConfig->bars[b].setFillColor({ 255, 255, 255, 50 });
		}

		logToFile(appConfig, "Initializing PortAudio");
		//initialise PortAudio
		err = Pa_Initialize();
		if (err != paNoError)
		{
			printf(Pa_GetErrorText(err));
			exit(1);
		}

		int apiCount = Pa_GetHostApiCount();
		if (apiCount <= 0)
		{
			logToFile(appConfig, "PortAudio found no host APIs");
			std::cout << "PortAudio found no host APIs" << std::endl;
		}
		for (int api = 0; api < apiCount; api++)
		{
			const PaHostApiInfo* hostInfo = Pa_GetHostApiInfo(api);
			std::cout << hostInfo->name << std::endl;
		}

		audioConfig->_nDevices = Pa_GetDeviceCount();
		audioConfig->_params.sampleFormat = PA_SAMPLE_TYPE;
		double sRate = 44100;
		int defInputIdx = Pa_GetDefaultInputDevice();

		logToFile(appConfig, "PortAudio found " + std::to_string(audioConfig->_nDevices) + " devices");

		if (audioConfig->_devIdx == -1 && audioConfig->_nDevices > 0)
		{
			//find an audio input
			for (PaDeviceIndex dI = 0; dI < audioConfig->_nDevices; dI++)
			{
				auto info = Pa_GetDeviceInfo(dI);
				std::string name = info->name;
				if (dI == defInputIdx)
				{
					logToFile(appConfig, "Auto-selecting device " + std::to_string(dI) + ": " + name);
					audioConfig->_devIdx = dI;
					break;
				}
			}

			if (audioConfig->_devIdx == -1)
				audioConfig->_devIdx = 0;
		}

		if (audioConfig->_devIdx != -1 && audioConfig->_nDevices > audioConfig->_devIdx)
		{
			auto info = Pa_GetDeviceInfo(audioConfig->_devIdx);
			audioConfig->_params.device = audioConfig->_devIdx;
			audioConfig->_params.channelCount = Min(1, info->maxInputChannels);
			audioConfig->_params.suggestedLatency = info->defaultLowInputLatency;
			audioConfig->_params.hostApiSpecificStreamInfo = nullptr;
			sRate = info->defaultSampleRate;
		}

		logToFile(appConfig, "PortAudio opening stream");
		err = Pa_OpenStream(&audioConfig->_audioStr, &audioConfig->_params, nullptr, sRate, FRAMES_PER_BUFFER, paClipOff, recordCallback, audioConfig->_streamData);
		if (err != paNoError)
		{
			auto errorMsg = Pa_GetErrorText(err);
			logToFile(appConfig, errorMsg);
		}
		err = Pa_StartStream(audioConfig->_audioStr);
		if (err != paNoError)
		{
			auto errorMsg = Pa_GetErrorText(err);
			logToFile(appConfig, errorMsg);
		}

		logToFile(appConfig, "Focusing main window");
		//request focus and start the game loop

		try
		{
			appConfig->_window.requestFocus();
		}
		catch (...)
		{
			logToFile(appConfig, "Exception while requesting focus on main window. ");
			exit(1);
		}

		try
		{
			// SFML doesn't always actually focus
			sf::Event dummyFocus;
			dummyFocus.type = sf::Event::GainedFocus;
			ImGui::SFML::ProcessEvent(appConfig->_window, dummyFocus);
		}
		catch (...)
		{
			logToFile(appConfig, "Exception while attempting to force SFML into focus. ");
			exit(1);
		}


		audioConfig->_frameMax = audioConfig->_fixedMax; //audioConfig->_cutoff;
		audioConfig->_frameHi = 0;
		audioConfig->_runningAverage = 0;

		audioConfig->_midMax = audioConfig->_fixedMax; // audioConfig->_cutoff;
		audioConfig->_midHi = 0;
		audioConfig->_midAverage = 0;

		audioConfig->_bassMax = audioConfig->_fixedMax; // audioConfig->_cutoff;
		audioConfig->_bassHi = 0;
		audioConfig->_bassAverage = 0;

		audioConfig->_trebleMax = audioConfig->_fixedMax; // audioConfig->_cutoff;
		audioConfig->_trebleHi = 0;
		audioConfig->_trebleAverage = 0;

		audioConfig->_quietTimer.restart();

		appConfig->_webSocket = new WebSocket();
		if (appConfig->_listenHTTP)
			appConfig->_webSocket->Start(appConfig->_httpPort);

        GamePad::init((void*)appConfig->_window.getSystemHandle(), (GamepadAPI)appConfig->_gamepadAPI);

		logToFile(appConfig, "Setup Complete!");

		return;
	}

	void MainLoop()
	{
		while (appConfig->_window.isOpen())
		{
			doAudioAnalysis();

			//appConfig->_webSocket->Poll();

			handleEvents();
			if (!appConfig->_window.isOpen())
				break;

			render();

			if (appConfig->_pendingNameChange)
			{
				appConfig->_window.setTitle(appConfig->windowName);
				appConfig->_pendingNameChange = false;
			}
		}
	}

	void Cleanup()
	{
		//kbdTrack->SetHook(false);

		if (audioConfig)
		{
			Pa_StopStream(audioConfig->_audioStr);
			Pa_CloseStream(audioConfig->_audioStr);
			Pa_Terminate();
		}
		

		if (appConfig->_checkUpdateThread != nullptr)
		{
			if (appConfig->_checkUpdateThread->joinable())
				appConfig->_checkUpdateThread->join();

			delete appConfig->_checkUpdateThread;
			appConfig->_checkUpdateThread = nullptr;
		}

		if (layerMan)
		{
			appConfig->_lastLayerSet = layerMan->LastUsedLayerSet();

			appConfig->_loader->saveCommon();

			layerMan->SaveLayers(appConfig->lastLayerSettingsFile);

			delete layerMan;
		}
		

		ImGui::SFML::Shutdown();

		if (appConfig->_window.isOpen())
			appConfig->_window.close();

		if (appConfig->_menuWindow.isOpen())
			appConfig->_menuWindow.close();

		delete appConfig->_loader;
		delete appConfig->_webSocket;

		appConfig->_logStream.close();

		delete appConfig;
		delete uiConfig;
		delete audioConfig;
#ifdef _WIN32
		if (spout != nullptr)
		{
			spout->ReleaseSender();
			delete spout;
		}
#endif
		//delete kbdTrack;
	}



};

