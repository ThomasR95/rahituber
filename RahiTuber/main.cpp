#include <string>
#include <iostream>

#include <random>
#include <deque>
#include "wtypes.h"

#include "imgui.h"
#include "imgui-SFML.h"

#include "Config.h"
#include "xmlConfig.h"

#include "LayerManager.h"

#include <fstream>

#include <Windows.h>
#include <Dwmapi.h>
#pragma comment (lib, "Dwmapi.lib")

AppConfig* appConfig = nullptr;
AudioConfig* audioConfig = nullptr;
UIConfig* uiConfig = nullptr;
LayerManager* layerMan = nullptr;

float mean(float a, float b) { return a + (b-a)*0.5;}
float between(float a, float b) { return a*0.5f + b*0.5f; }

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
		wtemp = sin(0.5*theta);
		wpr = -2.0*wtemp*wtemp;
		wpi = sin(theta);
		wr = 1.0;
		wi = 0.0;
		for (m = 1; m < mmax; m += 2) {
			for (i = m; i <= n; i += istep) {
				j = i + mmax;
				tempr = wr*data[j - 1] - wi*data[j];
				tempi = wr * data[j] + wi*data[j - 1];

				data[j - 1] = data[i - 1] - tempr;
				data[j] = data[i] - tempi;
				data[i - 1] += tempr;
				data[i] += tempi;
			}
			wtemp = wr;
			wr += wr*wpr - wi*wpi;
			wi += wi*wpr + wtemp*wpi;
		}
		mmax = istep;
	}
}

static int recordCallback(const void *inputBuffer, void *outputBuffer,
	unsigned long framesPerBuffer,
	const PaStreamCallbackTimeInfo* timeInfo,
	PaStreamCallbackFlags statusFlags,
	void *userData)
{
	int checkSize = framesPerBuffer;

	int numChannels = audioConfig->_params.channelCount;

	//Erase old frames, leave enough in the vector for 2 checkSizes
	{ //lock for frequency data
		std::lock_guard<std::mutex> guard(audioConfig->_freqDataMutex);
		if (audioConfig->_frames.size() > checkSize*2)
			audioConfig->_frames.erase(audioConfig->_frames.begin(), audioConfig->_frames.begin() + (audioConfig->_frames.size() - checkSize*2));
	}

	SAMPLE *rptr = (SAMPLE*)inputBuffer;

	int s = 0;

	while (s < checkSize)
	{
		SAMPLE splLeft = (*rptr++);
		SAMPLE splRight = splLeft;
		if (numChannels == 2)
			splRight = (*rptr++);

		SAMPLE spl = (splLeft + splRight) / 2.f;

		spl = spl*spl;

		{ //lock for frequency data
			std::lock_guard<std::mutex> guard(audioConfig->_freqDataMutex);

			audioConfig->_frames.push_back(fabs(spl));
		}

		s++;
	}

	return paContinue;
}

void getWindowSizes()
{
	RECT desktop;
	// Get a handle to the desktop window
	const HWND hDesktop = GetDesktopWindow();
	// Get the size of screen to the variable desktop
	GetWindowRect(hDesktop, &desktop);

	appConfig->_fullScrW = desktop.right;
	appConfig->_fullScrH = desktop.bottom;

	appConfig->_minScrW = SCRW;
	appConfig->_minScrH = SCRH;

	appConfig->_scrW = appConfig->_minScrW;
	appConfig->_scrH = appConfig->_minScrH;

	appConfig->_ratio = appConfig->_scrW / appConfig->_scrH;
}

void initWindow(bool firstStart = false)
{
	if (appConfig->_isFullScreen)
	{
		if (appConfig->_currentWindow)
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
				appConfig->_window.create(sf::VideoMode(appConfig->_fullScrW, appConfig->_fullScrH), "RahiTuber", 0);
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
		appConfig->_scrW = appConfig->_minScrW;
		appConfig->_scrH = appConfig->_minScrH;
		if (appConfig->_wasFullScreen || firstStart)
		{
			appConfig->_window.create(sf::VideoMode(appConfig->_scrW, appConfig->_scrH), "RahiTuber", 0);
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

	appConfig->_RT.create(appConfig->_scrW, appConfig->_scrH);

	appConfig->_currentWindow = &appConfig->_window;

	uiConfig->_topLeftBox.setPosition(0, 0);
	uiConfig->_topLeftBox.setSize({ 20,20 });
	uiConfig->_topLeftBox.setFillColor({ 255,255,255,100 });
	uiConfig->_bottomRightBox = uiConfig->_topLeftBox;
	uiConfig->_bottomRightBox.setPosition({ appConfig->_scrW - 20, appConfig->_scrH - 20 });
	uiConfig->_resizeBox.setSize({ appConfig->_scrW, appConfig->_scrH });
	uiConfig->_resizeBox.setOutlineThickness(1);
	uiConfig->_resizeBox.setFillColor({ 0,0,0,0 });

	appConfig->_wasFullScreen = appConfig->_isFullScreen;

	appConfig->_window.setVerticalSyncEnabled(appConfig->_enableVSync);

	appConfig->_window.setFramerateLimit(appConfig->_enableVSync? 60 : 200);

	if (appConfig->_alwaysOnTop)
	{
		HWND hwnd = appConfig->_window.getSystemHandle();
		SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
	}

	if (appConfig->_transparent)
	{
		MARGINS margins;
		margins.cxLeftWidth = -1;

		HWND hwnd = appConfig->_window.getSystemHandle();
		//SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);

		SetWindowLong(hwnd, GWL_STYLE, WS_POPUP | WS_VISIBLE);
		DwmExtendFrameIntoClientArea(hwnd, &margins);
	}

	if (uiConfig->_ico.getPixelsPtr())
	{
		appConfig->_window.setIcon(uiConfig->_ico.getSize().x, uiConfig->_ico.getSize().y, uiConfig->_ico.getPixelsPtr());
	}
}

void swapFullScreen()
{
	appConfig->_isFullScreen = !appConfig->_isFullScreen;
	initWindow();
}

void menuHelp(ImGuiStyle& style)
{
	if (ImGui::Button("Help", { ImGui::GetWindowWidth() / 2 - 10, 20 }))
	{
		float h = ImGui::GetWindowHeight();
		ImGui::SetNextWindowSize({ 400, h });
		ImGui::OpenPopup("Help");
	}
	ImGui::SetNextWindowPosCenter();
	ImGui::SetNextWindowSize({ 400,-1 });
	//ImGui::SetNextWindowSizeConstraints({ 400, 400 }, { -1,-1 });
	if (ImGui::BeginPopup("Help", ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize))
	{
		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Text]);
		ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Close Menu:");				ImGui::SameLine(160); ImGui::TextWrapped("Closes the main menu. ");
		ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Fullscreen:");				ImGui::SameLine(160); ImGui::TextWrapped("Toggles fullscreen.");
		ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Show Advanced...:");		ImGui::SameLine(160); ImGui::TextWrapped("Reveals some advanced options.");
		ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Audio Input:");			ImGui::SameLine(160); ImGui::TextWrapped("Lets you select which audio device affects the visuals.");
		ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Presets:");				ImGui::SameLine(160); ImGui::TextWrapped("Save and load window settings.");
		ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Exit RahiTuber:");		ImGui::SameLine(160); ImGui::TextWrapped("Saves the layers and closes the program.");
		ImGui::NewLine();
		ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Window Controls:");
		ImGui::TextWrapped("Drag from the top-left or bottom-right corner to resize the window.\nDrag with the middle mouse button, or use the move tab in the top centre, to move the whole window.");
		ImGui::NewLine();
		ImGui::NewLine();
		ImGui::NewLine();
		ImGui::NewLine();
		ImGui::PopStyleColor();
		ImGui::PushStyleColor(ImGuiCol_Text, { 0.3f,0.3f,0.3f,1.f });

		time_t timeNow = time(0);
		tm timeStruct;
		localtime_s(&timeStruct, &timeNow);
		int year = timeStruct.tm_year + 1900;

		ImGui::TextWrapped("PortAudio (c) 1999-2006 Ross Bencina and Phil Burk");
		ImGui::TextWrapped("SFML (c) 2007-%d Laurent Gomila", year);
		ImGui::TextWrapped("Dear ImGui (c) 2014-%d Omar Cornut", year);
		ImGui::NewLine();
		ImGui::TextWrapped("RahiTuber (c) 2018-%d Tom Rule", year);
		ImGui::PopStyleColor();
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
	if (ImGui::Button(uiConfig->_advancedMenuShowing ? "Hide Advanced" : "Show Advanced...", { -1, 20 }))
	{
		uiConfig->_advancedMenuShowing = !uiConfig->_advancedMenuShowing;
	}
	if (uiConfig->_advancedMenuShowing)
	{
		ImGui::Checkbox("Menu On Start", &uiConfig->_showMenuOnStart);
		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Separator]);
		ImGui::SameLine(140); ImGui::TextWrapped("Start the application with the menu open.");
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Separator]);
		float col[3] = {(float)appConfig->_bgColor.r / 255, (float)appConfig->_bgColor.g / 255, (float)appConfig->_bgColor.b / 255};
		if (ImGui::ColorEdit3("(<- click)", col, ImGuiColorEditFlags_NoAlpha | ImGuiColorEditFlags_NoInputs))
		{
			appConfig->_bgColor = sf::Color(int(255.f * col[0]), int(255.f * col[1]), int(255.f * col[2]));
		}
		
		ImGui::SameLine(140); ImGui::TextWrapped("Background Color");
		ImGui::PopStyleColor();

		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Text]);
		float transChkBoxPos = ImGui::GetCursorPosY();
		if (ImGui::Checkbox("Transparent", &appConfig->_transparent))
		{
			if (appConfig->_transparent)
			{
				if (appConfig->_isFullScreen)
				{
					appConfig->_scrW = appConfig->_fullScrW + 1;
					appConfig->_scrH = appConfig->_fullScrH + 1;
					appConfig->_window.create(sf::VideoMode(appConfig->_scrW, appConfig->_scrH), "VisualiStar", 0);
					appConfig->_window.setIcon((int)uiConfig->_ico.getSize().x, (int)uiConfig->_ico.getSize().y, uiConfig->_ico.getPixelsPtr());
					appConfig->_window.setSize({ (sf::Uint16)appConfig->_scrW, (sf::Uint16)appConfig->_scrH });
					appConfig->_window.setPosition({ 0,0 });
					sf::View v = appConfig->_window.getView();
					v.setSize({ appConfig->_scrW, appConfig->_scrH });
					v.setCenter({ appConfig->_scrW / 2, appConfig->_scrH / 2 });
					appConfig->_window.setView(v);
				}

				MARGINS margins;
				margins.cxLeftWidth = -1;
				//SetWindowLong(gameConfig->_window.getSystemHandle(), GWL_EXSTYLE, WS_EX_TRANSPARENT | WS_EX_LAYERED);
				//EnableWindow(gameConfig->_window.getSystemHandle(), false);

				DwmExtendFrameIntoClientArea(appConfig->_window.getSystemHandle(), &margins);

				//gameConfig->menuShowing = false;
			}
			else
			{
				SetWindowLong(appConfig->_window.getSystemHandle(), GWL_EXSTYLE, 0);
				EnableWindow(appConfig->_window.getSystemHandle(), true);
			}
		}
		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Separator]);
		ImGui::SameLine(140); ImGui::TextWrapped("Turns the app background transparent (Useful for screen capture).");
		ImGui::PopStyleColor();

		if (ImGui::Checkbox("Always On Top", &appConfig->_alwaysOnTop))
		{
			if (appConfig->_alwaysOnTop)
			{
				HWND hwnd = appConfig->_window.getSystemHandle();
				SetWindowPos(hwnd, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
			else
			{
				HWND hwnd = appConfig->_window.getSystemHandle();
				SetWindowPos(hwnd, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE);
			}
		}
		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Separator]);
		ImGui::SameLine(140); ImGui::TextWrapped("Keeps the app above all other windows on your screen.");
		ImGui::PopStyleColor();

		ImGui::Checkbox("Show FPS", &uiConfig->_showFPS);
		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Separator]);
		ImGui::SameLine(140); ImGui::TextWrapped("Shows an FPS counter (when menu is inactive).");
		ImGui::PopStyleColor();

		if (ImGui::Checkbox("VSync", &appConfig->_enableVSync))
		{
			initWindow();
		}
		ImGui::PushStyleColor(ImGuiCol_Text, style.Colors[ImGuiCol_Separator]);
		ImGui::SameLine(140); ImGui::TextWrapped("Enable/Disable Vertical Sync.");
		ImGui::PopStyleColor();

		ImGui::PopStyleColor();
	}
}

void menuAudio(ImGuiStyle& style)
{
	ImGui::TextColored(style.Colors[ImGuiCol_Text], "Audio Input");

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

	//ImGui::TextColored({ 0.5, 0.5, 0.5, 1.0 }, "Current:\n  %s", gameConfig->deviceList[gameConfig->devIdx].first.c_str());
	ImGui::PushID("AudImpCombo");
	ImGui::PushItemWidth(200);
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
				audioConfig->_params.channelCount = min(2, info->maxInputChannels);
				audioConfig->_params.suggestedLatency = info->defaultLowInputLatency;
				audioConfig->_params.hostApiSpecificStreamInfo = nullptr;// (PaHostApiInfo*)Pa_GetHostApiInfo(Pa_GetDefaultHostApi());
				sRate = info->defaultSampleRate;

				Pa_OpenStream(&audioConfig->_audioStr, &audioConfig->_params, nullptr, sRate, FRAMES_PER_BUFFER, paClipOff, recordCallback, audioConfig->_streamData);
				Pa_StartStream(audioConfig->_audioStr);

				audioConfig->_frameMax = audioConfig->_cutoff;
				audioConfig->_frameHi = 0;
				audioConfig->_runningAverage = 0;

				audioConfig->_bassMax = audioConfig->_cutoff;
				audioConfig->_bassHi = 0;
				audioConfig->_bassAverage = 0;

				audioConfig->_trebleMax = audioConfig->_cutoff;
				audioConfig->_trebleHi = 0;
				audioConfig->_trebleAverage = 0;

			}
		}
		ImGui::EndCombo();
	}
	ImGui::PopID();
	ImGui::PopItemWidth();
}

void menuPresets(ImGuiStyle& style)
{
	if (ImGui::Button("Presets", { -1,20 }))
	{
		ImGui::OpenPopup("Presets");
	}
	if (ImGui::BeginPopup("Presets", ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings))
	{
		ImGui::SetWindowSize({ 400,-1 });
		ImGui::SetWindowPos({ appConfig->_scrW / 2 - 200, appConfig->_scrH / 3 });

		ImGui::TextColored(style.Colors[ImGuiCol_Separator], "Save or load presets for window attributes");
		ImGui::Separator();
		std::string prevName = "";
		if (uiConfig->_presetNames.size()) prevName = uiConfig->_presetNames[uiConfig->_presetIdx];
		if (ImGui::BeginCombo("Preset", prevName.c_str()))
		{
			for (int p = 0; p < uiConfig->_presetNames.size(); p++)
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
		if (ImGui::Button("Load", { 100,20 }) && uiConfig->_presetNames.size())
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
		ImGui::InputText("Name", uiConfig->_settingsFileBoxName.data(), 30);
		ImGui::SameLine();
		if (ImGui::Button("x", { 20,20 }))
		{
			uiConfig->_settingsFileBoxName.clear();
			uiConfig->_settingsFileBoxName.resize(30);
		}
		ImGui::SameLine();
		if (ImGui::Button("Use Current", { -1,20 }) && uiConfig->_presetNames.size())
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

		//ImGui::PushID("windowsettings_id");
		//ImGui::TextColored(style.Colors[ImGuiCol_Text], "Window Settings");
		//ImGui::SameLine(); ImGui::TextColored(style.Colors[ImGuiCol_Separator], "(Size, position)");

		//if (ImGui::Checkbox(saveCheckBox.c_str(), &uiConfig->_saveWindowInfo))
		//{
		//	if (uiConfig->_saveWindowInfo)
		//		uiConfig->_clearWindowInfo = false;
		//}
		//if (overwriting)
		//{
		//	ImGui::SameLine();
		//	if (ImGui::Checkbox("Clear", &uiConfig->_clearWindowInfo))
		//	{
		//		if (uiConfig->_clearWindowInfo)
		//			uiConfig->_saveWindowInfo = false;
		//	}
		//}
		//ImGui::PopID();

		uiConfig->_clearWindowInfo = false;
		uiConfig->_saveWindowInfo = true;
		

		std::string saveName = "Save";
		if (overwriting) saveName = "Overwrite";

		std::string name(uiConfig->_settingsFileBoxName.data());

		if ((name != "") && ImGui::Button(saveName.c_str(), { -1,20 }))
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

void menu()
{
	ImGui::SFML::Update(appConfig->_window, appConfig->_timer.getElapsedTime());

	auto& style = ImGui::GetStyle();
	style.FrameRounding = 4;
	style.WindowTitleAlign = style.ButtonTextAlign;

	ImVec4 baseColor(1.0, 0.0, 1.0, 1.0);

	ImVec4 col_dark2(baseColor.x * 0.1f, baseColor.y * 0.1f, baseColor.z * 0.1f, 1.f);
	ImVec4 col_dark(baseColor.x*0.3f, baseColor.y*0.3f, baseColor.z*0.3f, 1.f);
	ImVec4 col_med(baseColor.x*0.5f, baseColor.y*0.5f, baseColor.z*0.5f, 1.f);
	ImVec4 col_light(baseColor.x*0.8f, baseColor.y*0.8f, baseColor.z*0.8f, 1.f);
	ImVec4 col_light2(baseColor);
	ImVec4 col_light2a(mean(baseColor.x, 0.6f), mean(baseColor.y, 0.6f), mean(baseColor.z, 0.6f), 1.f);
	ImVec4 col_light3(mean(baseColor.x,1.f), mean(baseColor.y, 1.f), mean(baseColor.z, 1.f), 1.f);
	ImVec4 greyoutCol(0.3, 0.3, 0.3, 1.0);

	style.Colors[ImGuiCol_WindowBg] = style.Colors[ImGuiCol_ChildWindowBg] = { 0.05f,0.05f,0.05f, 1.0f };
	style.Colors[ImGuiCol_PopupBg] = { 0.08f,0.08f,0.08f, 1.0f };
	style.Colors[ImGuiCol_ChildBg] = col_dark2;
	style.Colors[ImGuiCol_FrameBg] = col_dark;
	style.Colors[ImGuiCol_FrameBgActive] = style.Colors[ImGuiCol_Button] = style.Colors[ImGuiCol_Header] = style.Colors[ImGuiCol_SliderGrab] = col_med;
	style.Colors[ImGuiCol_ButtonActive] = style.Colors[ImGuiCol_HeaderActive] = style.Colors[ImGuiCol_SliderGrabActive] = col_light2;
	style.Colors[ImGuiCol_ButtonHovered] = style.Colors[ImGuiCol_HeaderHovered] = col_light;
	style.Colors[ImGuiCol_TitleBgActive] = style.Colors[ImGuiCol_TitleBg] = style.Colors[ImGuiCol_TitleBgCollapsed] = col_dark;
	style.Colors[ImGuiCol_Text] = col_light3;
	style.Colors[ImGuiCol_TextDisabled] = greyoutCol;
	style.Colors[ImGuiCol_Separator] = col_light2a;
	 
	style.WindowTitleAlign = { 0.5f, 0.5f };

	if (!appConfig->_isFullScreen)
	{
		// Move tab in the top centre
		ImGui::SetNextWindowPos({ appConfig->_scrW / 2 - 40, 0 });
		ImGui::SetNextWindowSize(uiConfig->_moveTabSize);

		ImGui::Begin("move_tab", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoScrollbar);
		ImGui::SetCursorPos({ uiConfig->_moveTabSize.x / 2 - 12,4 });
		ImGui::Image(uiConfig->_moveIconSprite, sf::Vector2f(24, 24), col_light3);
		ImGui::End();
	}

	// Main menu window
	ImGui::SetNextWindowPos(ImVec2(10,10));
	float windowHeight = appConfig->_scrH - 20;
	ImGui::SetNextWindowSize({ 480, windowHeight });
	ImGui::Begin("RahiTuber", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings);

	ImDrawList* dList = ImGui::GetWindowDrawList();

	if (ImGui::Button("Close Menu (Esc)", { -1,20 }))
	{
		uiConfig->_menuShowing = false;
		if (appConfig->_transparent)
			SetWindowLong(appConfig->_window.getSystemHandle(), GWL_EXSTYLE, WS_EX_TRANSPARENT);
	}

	//	FULLSCREEN
	if (ImGui::Button(appConfig->_isFullScreen? "Exit Fullscreen (F11)" : "Fullscreen (F11)", { -1,20 }))
	{
		swapFullScreen();
	}

	menuHelp(style);

	ImGui::SameLine(); 
	
	menuAdvanced(style);

	ImGui::Separator();
	float separatorPos = ImGui::GetCursorPosY();
	float nextSectionPos = windowHeight - 120;

	layerMan->DrawGUI(style, nextSectionPos - separatorPos);

	//	INPUT LIST
	ImGui::SetCursorPosY(nextSectionPos);
	ImGui::Separator();
	menuAudio(style);
	
	ImGui::NewLine();

	ImGui::Columns();

	menuPresets(style);

	ImGui::Separator();

	time_t timeNow = time(0);
	tm timeStruct;
	localtime_s(&timeStruct, &timeNow);

	int milliseconds = appConfig->_timer.getElapsedTime().asMilliseconds();

	//	EXIT
	ImGui::PushStyleColor(ImGuiCol_Button, col_dark);
	ImGui::PushStyleColor(ImGuiCol_ButtonHovered, { 0.5f + 0.2f * ((milliseconds/300) % 2), 0.f, 0.f, 1.0f });
	ImGui::PushStyleColor(ImGuiCol_ButtonActive, { 0.3f, 0.f, 0.f, 1.0f });
	if (ImGui::Button("Exit RahiTuber", { -1, 20 }))
	{
		appConfig->_window.close();
	}
	ImGui::PopStyleColor(3);

	ImGui::End();
	ImGui::EndFrame();
	ImGui::SFML::Render(appConfig->_window);

	uiConfig->_firstMenu = false;
}

void handleEvents()
{
	if (appConfig->_currentWindow->hasFocus() && appConfig->_isFullScreen && !uiConfig->_menuShowing)
	{
		appConfig->_currentWindow->setMouseCursorVisible(false);
	}
	else
	{
		appConfig->_currentWindow->setMouseCursorVisible(true);
	}

	if (uiConfig->_cornerGrabbed.first || uiConfig->_cornerGrabbed.second)
	{
		appConfig->_currentWindow->requestFocus();
	}

	sf::Event evt;
	while (appConfig->_currentWindow->pollEvent(evt))
	{
		if (evt.type == evt.KeyPressed
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
			sf::Keyboard::Key mod = sf::Keyboard::Unknown;
			if (evt.key.control)
				mod = sf::Keyboard::LControl;
			if (evt.key.shift)
				mod = sf::Keyboard::LShift;
			if (evt.key.alt)
				mod = sf::Keyboard::LAlt;
			if (evt.key.system)
				mod = sf::Keyboard::LSystem;

			if (layerMan->PendingHotkey())
			{
				layerMan->SetHotkeys(evt.key.code, mod);
				continue;
			}
			else
			{
				layerMan->HandleHotkey(evt.key.code, mod);
			}
		}

		if (evt.type == evt.Closed)
		{
			layerMan->SaveLayers("lastLayers.xml");
			//close the application
			appConfig->_currentWindow->close();
			break;
		}
		else if (evt.type == evt.MouseButtonPressed && sf::Mouse::isButtonPressed(sf::Mouse::Left))
		{
			//check if user clicked in the corners for window resize
			if (sf::Mouse::isButtonPressed(sf::Mouse::Left) && !appConfig->_isFullScreen)
			{
				auto pos = sf::Mouse::getPosition(appConfig->_window);
				if (pos.x < 20 && pos.y < 20)
					uiConfig->_cornerGrabbed = { true, false };

				if (pos.x > appConfig->_scrW - 20 && pos.y > appConfig->_scrH - 20)
					uiConfig->_cornerGrabbed = { false, true };

				if (pos.x > (appConfig->_scrW / 2 - uiConfig->_moveTabSize.x / 2) && pos.x < (appConfig->_scrW / 2 + uiConfig->_moveTabSize.x / 2) 
					&& pos.y < uiConfig->_moveTabSize.y)
				{
					uiConfig->_moveGrabbed = true;
				}
			}
		}
		else if ((evt.type == evt.KeyPressed && evt.key.code == sf::Keyboard::Escape) || (evt.type == evt.MouseButtonPressed && sf::Mouse::isButtonPressed(sf::Mouse::Right)))
		{
			//toggle the menu visibility
			uiConfig->_menuShowing = !uiConfig->_menuShowing;
			if (uiConfig->_menuShowing)
				uiConfig->_firstMenu = true;
			else if (appConfig->_transparent)
				SetWindowLong(appConfig->_window.getSystemHandle(), GWL_EXSTYLE, WS_EX_TRANSPARENT);

			appConfig->_timer.restart();

			break;
		}
		else if (evt.type == evt.KeyPressed && evt.key.code == sf::Keyboard::F11 && evt.key.control == false)
		{
			//toggle fullscreen
			swapFullScreen();
		}
		else if (evt.type == evt.MouseButtonReleased)
		{
			//set the window if it was being resized
			if (uiConfig->_cornerGrabbed.first || uiConfig->_cornerGrabbed.second)
			{
				auto windowPos = appConfig->_window.getPosition();
				windowPos += sf::Vector2i(uiConfig->_resizeBox.getPosition());
				appConfig->_scrX = windowPos.x;
				appConfig->_scrY = windowPos.y;
				appConfig->_window.setPosition(windowPos);
				appConfig->_minScrH = uiConfig->_resizeBox.getGlobalBounds().height;
				appConfig->_minScrW = uiConfig->_resizeBox.getGlobalBounds().width;
				uiConfig->_cornerGrabbed = { false, false };

				initWindow();
			}
			uiConfig->_lastMiddleClickPosition = sf::Vector2i(-1, -1);
			uiConfig->_moveGrabbed = false;
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
			else if (uiConfig->_moveGrabbed)
			{
				auto mousePos = sf::Mouse::getPosition();
				auto windowPos = mousePos - sf::Vector2i(appConfig->_scrW / 2, uiConfig->_moveTabSize.y / 2);
				appConfig->_scrX = windowPos.x;
				appConfig->_scrY = windowPos.y;
				appConfig->_window.setPosition(windowPos);
			}
			else if (sf::Mouse::isButtonPressed(sf::Mouse::Middle) && !appConfig->_isFullScreen)
			{
				if (uiConfig->_lastMiddleClickPosition == sf::Vector2i(-1, -1))
					uiConfig->_lastMiddleClickPosition = sf::Mouse::getPosition(appConfig->_window);
				auto mousePos = sf::Mouse::getPosition();
				auto windowPos = mousePos - uiConfig->_lastMiddleClickPosition;
				appConfig->_scrX = windowPos.x;
				appConfig->_scrY = windowPos.y;
				appConfig->_window.setPosition(windowPos);
			}
		}

		if (uiConfig->_menuShowing)
			ImGui::SFML::ProcessEvent(evt);
	}
}

void render()
{
	auto dt = appConfig->_timer.restart();
	appConfig->_fps = (1.0 / dt.asSeconds());

	if (appConfig->_transparent)
		appConfig->_window.clear(sf::Color(0, 0, 0, 0));
	else
		appConfig->_window.clear(appConfig->_bgColor);

 	layerMan->Draw(&appConfig->_window, appConfig->_scrH, appConfig->_scrW, audioConfig->_midAverage - (audioConfig->_trebleAverage + 0.3*audioConfig->_bassAverage), audioConfig->_midMax);

	if (uiConfig->_menuShowing)
	{
		if (!appConfig->_isFullScreen)
		{
			appConfig->_window.draw(uiConfig->_topLeftBox);
			appConfig->_window.draw(uiConfig->_bottomRightBox);
		}
		menu();
	}
	else if(uiConfig->_showFPS)
	{
		
		ImGui::SFML::Update(appConfig->_window, dt);

		ImGui::Begin("", 0, ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoTitleBar);

		ImGui::Text("FPS: %d", (int)appConfig->_fps);

		ImGui::End();
		ImGui::EndFrame();
		ImGui::SFML::Render(appConfig->_window);
	}

#ifdef _DEBUG
	//draw debug audio bars
	{
		std::lock_guard<std::mutex> guard(audioConfig->_freqDataMutex);

		auto frames = audioConfig->_frequencyData;
		float barW = appConfig->_scrW / (frames.size() / 2);

		for (int bar = 0; bar < frames.size() / 2; bar++)
		{
			float height = (frames[bar] / audioConfig->_bassMax)*appConfig->_scrH;
			appConfig->bars[bar].setSize({ barW, height });
			appConfig->bars[bar].setOrigin({ 0.f, height });
			appConfig->bars[bar].setPosition({ barW*bar, appConfig->_scrH });
			appConfig->_window.draw(appConfig->bars[bar]);
	}
}
#endif


	if (uiConfig->_cornerGrabbed.first)
	{
		appConfig->_window.draw(uiConfig->_topLeftBox);
		appConfig->_window.draw(uiConfig->_resizeBox);
	}
	else if (uiConfig->_cornerGrabbed.second)
	{
		appConfig->_window.draw(uiConfig->_bottomRightBox);
		appConfig->_window.draw(uiConfig->_resizeBox);
	}

	appConfig->_window.display();
}

void doAudioAnalysis()
{
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

	for (int it = 0; it < halfSize; it++)
	{
		auto re = audioConfig->_fftData[it];
		auto im = audioConfig->_fftData[it + 1];
		auto magnitude = std::sqrt(re * re + im * im);

		//Compensations for the fact my FFT implementation is probably wrong
		float point = it / factor + 0.3;
		magnitude = magnitude * atan(point);
		if (it == 0) magnitude *= 0.7;

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

	audioConfig->_smoothAmount = appConfig->_fps / 6;

	if (audioConfig->_frameHi != 0.0)
	{
		//update audio data for this frame
		if (audioConfig->_frameHi < 0) audioConfig->_frameHi *= -1.0;
			audioConfig->_frame = audioConfig->_frameHi;

		audioConfig->_runningAverage -= audioConfig->_runningAverage / audioConfig->_smoothAmount;
		audioConfig->_runningAverage += audioConfig->_frame / audioConfig->_smoothAmount;
		if (audioConfig->_frame > audioConfig->_runningAverage)
			audioConfig->_runningAverage = audioConfig->_frame;
		if (audioConfig->_frame > audioConfig->_frameMax)
			audioConfig->_frameMax = audioConfig->_frame;

		if (audioConfig->_bassHi < 0) audioConfig->_bassHi *= -1.0;

		audioConfig->_bassAverage -= audioConfig->_bassAverage / audioConfig->_smoothAmount;
		audioConfig->_bassAverage += audioConfig->_bassHi / audioConfig->_smoothAmount;
		if (audioConfig->_bassHi > audioConfig->_bassAverage)
			audioConfig->_bassAverage = audioConfig->_bassHi;
		if (audioConfig->_bassHi > audioConfig->_bassMax)
			audioConfig->_bassMax = audioConfig->_bassHi;

		if (audioConfig->_midHi < 0) audioConfig->_midHi *= -1.0;

		audioConfig->_midAverage -= audioConfig->_midAverage / audioConfig->_smoothAmount;
		audioConfig->_midAverage += audioConfig->_midHi / audioConfig->_smoothAmount;
		if (audioConfig->_midHi > audioConfig->_midAverage)
			audioConfig->_midAverage = audioConfig->_midHi;
		if (audioConfig->_midHi > audioConfig->_midMax)
			audioConfig->_midMax = audioConfig->_midHi;

		if (audioConfig->_trebleHi < 0) audioConfig->_trebleHi *= -1.0;

		audioConfig->_trebleAverage -= audioConfig->_trebleAverage / audioConfig->_smoothAmount;
		audioConfig->_trebleAverage += audioConfig->_trebleHi / audioConfig->_smoothAmount;
		if (audioConfig->_trebleHi > audioConfig->_trebleAverage)
			audioConfig->_trebleAverage = audioConfig->_trebleHi;
		if (audioConfig->_trebleHi > audioConfig->_trebleMax)
			audioConfig->_trebleMax = audioConfig->_trebleHi;
	}

	//As long as the music is loud enough the current max is good
	if (audioConfig->_frame > audioConfig->_cutoff * 2)
	{
		audioConfig->_quietTimer.restart();
	}
	else if (audioConfig->_quietTimer.getElapsedTime().asSeconds() > 0.3)
	{
		//if the quietTimer reaches 1.5s, start reducing the max

		float maxFallSpeed = 0.005;

		audioConfig->_frameMax -= (audioConfig->_frameMax - (audioConfig->_cutoff * 2)) * maxFallSpeed;
		audioConfig->_bassMax -= (audioConfig->_bassMax - (audioConfig->_cutoff * 2)) * maxFallSpeed;
		audioConfig->_midMax -= (audioConfig->_midMax - (audioConfig->_cutoff * 2)) * maxFallSpeed;
		audioConfig->_trebleMax -= (audioConfig->_trebleMax - (audioConfig->_cutoff * 2)) * maxFallSpeed;

		if (audioConfig->_frameMax < 0.2)
			audioConfig->_frameMax = 0.2;

		if (audioConfig->_bassMax < 0.2)
			audioConfig->_bassMax = 0.2;

		if (audioConfig->_midMax < 0.2)
			audioConfig->_midMax = 0.2;

		if (audioConfig->_trebleMax < 0.2)
			audioConfig->_trebleMax = 0.2;
	}
}


int main()
{
	PaError err = paNoError;

	std::srand(time(0));


	appConfig = new AppConfig();
	uiConfig = new UIConfig();
	audioConfig = new AudioConfig();

	layerMan = new LayerManager();
	const std::string lastLayerSettingsFile = "lastLayers.xml";
	layerMan->LoadLayers(lastLayerSettingsFile);

	getWindowSizes();

	xmlConfigLoader xmlLoader(appConfig, uiConfig, audioConfig, "config.xml");
	appConfig->_loader = &xmlLoader;
	xmlLoader.loadCommon();
	xmlLoader.loadPresetNames();

	layerMan->SetLayerSet(appConfig->_lastLayerSet);

	uiConfig->_menuShowing = uiConfig->_showMenuOnStart;

	if (appConfig->_startMaximised)
		appConfig->_isFullScreen = true;

	uiConfig->_ico.loadFromFile("icon.png");
	uiConfig->_settingsFileBoxName.resize(30);

	uiConfig->_moveIcon.loadFromFile("move.png");
	uiConfig->_moveIconSprite.setTexture(uiConfig->_moveIcon, true);

	initWindow(true);
	ImGui::SFML::Init(appConfig->_window);

	//setup debug bars
	audioConfig->_frames.resize(FRAMES_PER_BUFFER * 3);
	appConfig->bars.resize(FRAMES_PER_BUFFER / 2);
	float barW = appConfig->_scrW / (FRAMES_PER_BUFFER / 2);
	for (int b = 0; b < appConfig->bars.size(); b++)
	{
		appConfig->bars[b].setPosition((float)b*barW, 0);
		appConfig->bars[b].setSize({ barW, 0.f });
		appConfig->bars[b].setFillColor({ 255, 255, 255, 50 });
	}

	//initialise PortAudio
	err = Pa_Initialize();
	audioConfig->_nDevices = Pa_GetDeviceCount();
	audioConfig->_params.sampleFormat = PA_SAMPLE_TYPE;
	double sRate;
	auto defOutInf = Pa_GetDeviceInfo(Pa_GetDefaultOutputDevice());

	if (audioConfig->_devIdx == -1 && audioConfig->_nDevices > 0)
	{
		//find an audio input
		for (PaDeviceIndex dI = 0; dI < audioConfig->_nDevices; dI++)
		{
			auto info = Pa_GetDeviceInfo(dI);
			std::string name = info->name;
			if (name.find("Microphone") != std::string::npos)
			{
				audioConfig->_devIdx = dI;
				break;
			}
		}

		if (audioConfig->_devIdx == -1)
			audioConfig->_devIdx = 0;
	}
	
	if(audioConfig->_nDevices > audioConfig->_devIdx)
	{
		auto info = Pa_GetDeviceInfo(audioConfig->_devIdx);
		audioConfig->_params.device = audioConfig->_devIdx;
		audioConfig->_params.channelCount = min(1, info->maxInputChannels);
		audioConfig->_params.suggestedLatency = info->defaultLowInputLatency;
		audioConfig->_params.hostApiSpecificStreamInfo = nullptr;
		sRate = info->defaultSampleRate;
	}

	err = Pa_OpenStream(&audioConfig->_audioStr, &audioConfig->_params, nullptr, sRate, FRAMES_PER_BUFFER, paClipOff, recordCallback, audioConfig->_streamData);
	auto errorMsg = Pa_GetErrorText(err);
	err = Pa_StartStream(audioConfig->_audioStr);
	errorMsg = Pa_GetErrorText(err);

	//request focus and start the game loop
	appConfig->_currentWindow->requestFocus();

	audioConfig->_quietTimer.restart();

	////////////////////////////////////// MAIN LOOP /////////////////////////////////////
	while (appConfig->_currentWindow->isOpen())
	{
		doAudioAnalysis();

		handleEvents();
		if (!appConfig->_currentWindow->isOpen())
			break;

		render();

		audioConfig->_frameHi = 0;
		audioConfig->_bassHi = 0;
		audioConfig->_midHi = 0;
		audioConfig->_trebleHi = 0;

		sf::sleep(sf::milliseconds(8));
	}

	Pa_StopStream(audioConfig->_audioStr);
	Pa_CloseStream(audioConfig->_audioStr);
	Pa_Terminate();

	appConfig->_lastLayerSet = layerMan->LastUsedLayerSet();

	xmlLoader.saveCommon();

	layerMan->SaveLayers(lastLayerSettingsFile);

	ImGui::SFML::Shutdown();

	if (appConfig->_currentWindow->isOpen())
		appConfig->_currentWindow->close();

	delete appConfig;
	delete uiConfig;
	delete audioConfig;
	delete layerMan;
	return 0;

}
