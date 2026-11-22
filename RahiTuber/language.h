#ifndef __LANGUAGE_H__
#define __LANGUAGE_H__

#include <unordered_map>
#include <memory>
#include <list>
#include <string>

namespace Lang {
	static std::unique_ptr<std::unordered_map<std::string, std::string>> langStringsSingleton;
}

class LangInstance
{
public:
	static std::unordered_map<std::string, std::string>& Get()
	{
		if (!Lang::langStringsSingleton)
			Lang::langStringsSingleton = std::make_unique<std::unordered_map<std::string, std::string>>();

		return *(Lang::langStringsSingleton.get());
	}
};

static const std::string& LangStr(const std::string& key)
{
	return LangInstance::Get()[key];
}
static const char* LangCStr(const std::string& key)
{
	return LangInstance::Get()[key].c_str();
}

static void SetLangStr(const std::string& key, const std::string& str)
{
	LangInstance::Get()[key] = str;
}

static const std::unordered_map<std::string, std::string>& GetLangMap()
{
	return LangInstance::Get();
}

static void InitLangEN()
{
	SetLangStr("common.Menu", "Menu");
	SetLangStr("common.Interface", "Interface");
	SetLangStr("common.Hide", "Hide");
	SetLangStr("common.Show", "Show");
	SetLangStr("common.Close", "Close");
	SetLangStr("common.Exit", "Exit");
	SetLangStr("common.Popout", "Popout");
	SetLangStr("common.Dock", "Dock");
	SetLangStr("common.OK", "OK");
	SetLangStr("common.Yes", "Yes");
	SetLangStr("common.No", "No");
	SetLangStr("common.Cancel", "Cancel");
	SetLangStr("common.Save", "Save");
	SetLangStr("common.Update", "Update");
	SetLangStr("common.Overwrite", "Overwrite");
	SetLangStr("common.Load", "Load");

	SetLangStr("common.LoadingMsg", "RahiTuber is Loading");

	SetLangStr("menu.Fullscreen", "Fullscreen (F11)");
	SetLangStr("menu.FPS", "FPS");

	SetLangStr("help.Help", "Help");
	SetLangStr("help.HelpBtn", "Help & Tips");
	SetLangStr("help.Update", "Update %.3f Available!");
	SetLangStr("help.Downloads", "Go to Downloads page");
	SetLangStr("help.DownloadsTT", "Opens the RahiTuber Downloads page in your default web browser.");
	SetLangStr("help.Devlog", "Devlog (web link)");
	SetLangStr("help.DevlogTT", "Opens the RahiTuber Development log page\nin your default web browser.");
	SetLangStr("help.FAQ", "FAQ (web link)");
	SetLangStr("help.FAQTT", "Opens the RahiTuber FAQ page in your default web browser.");
	SetLangStr("help.Tutorials", "Tutorials (web link)");
	SetLangStr("help.TutorialsTT", "Opens the RahiTuber Tutorials page in your default web browser.");

	SetLangStr("help.TipTitle", "General tips:");
	SetLangStr("help.Tip1", "Hover over (or right-click) any control to see an explanation of what it does.");
	SetLangStr("help.Tip2", "CTRL+click any input field to manually type the value. For most sliders you can type a value outside of the sliding range.");

	SetLangStr("help.ControlTitle", "Window Controls:");
	SetLangStr("help.Controls", "Drag the squares in the top-left or bottom-right corner to resize the window.\nDrag with the middle mouse button, or use the move tab in the top centre, to move the whole window.");

	SetLangStr("help.Acknowledgement", "Acknowledgement");

	SetLangStr("menu.Preferences", "Preferences");

	SetLangStr("track.Tracking", "Tracking");
	SetLangStr("track.TrackingTT", "Global settings for Mouse/Controller tracking");
	SetLangStr("track.MouseTracking", "Mouse Tracking");
	SetLangStr("track.MouseTrackingTT", "Override setting to enable/disable all mouse tracking on layers.");
	SetLangStr("track.CtrlTracking", "Controller Tracking");
	SetLangStr("track.CtrlTrackingTT", "Override setting to enable/disable all controller tracking on layers.");
	SetLangStr("track.GlobalHdr", "Global Tracking Settings");
	SetLangStr("track.GlobalTxt", "These settings will apply to all layers with the 'Use Global Settings' option enabled.");

	SetLangStr("phnm.PhonemesTT", "Phoneme setup & calibration");
	SetLangStr("phnm.Phonemes", "Phonemes");

	SetLangStr("phnm.ShowDebug", "Show Debug Audio Display");
	SetLangStr("phnm.SubSplit", "Sub Split");
	SetLangStr("phnm.BassSplit", "Bass Split");
	SetLangStr("phnm.MidSplit", "Mid Split");
	SetLangStr("phnm.SubSplitTT", "Adjust where the border is between sub and bass frequencies.\nShow the debug audio bars to see the border.\nSub (blue) should be louder when saying 'oo',\nand bass (red) should be louder when saying 'Ah'.");
	SetLangStr("phnm.BassSplitTT", "Adjust where the border is between bass and mid frequencies.\nShow the debug audio bars to see the border.\nBass (red) should be louder when saying 'Ah',\nand mid (yellow) should be louder when saying 'Ee'.");
	SetLangStr("phnm.MidSplitTT", "Adjust where the border is between mid and treble frequencies.\nShow the debug audio bars to see the border.\nMid (yellow) should be louder when saying 'Ee',\nand treble (green) should be louder when saying 'SS'.");
	SetLangStr("phnm.Adjustment", "Adjustment");
	SetLangStr("phnm.Phoneme", "Phoneme");
	SetLangStr("phnm.Sensitivity", "Sensitivity");
	SetLangStr("phnm.Confidence", "Confidence");

	SetLangStr("phnm.ConfidenceSl", "Adjust Confidence");
	SetLangStr("phnm.ConfidenceSlTT", "Boost confidence for detecting this phoneme\n(i.e. reduces how many frames a phoneme must be detected\nbefore being displayed)");

	SetLangStr("phnm.SensitivitySl", "Adjust Sensitivity");
	SetLangStr("phnm.SensitivitySlTT", "Raise or lower the sensitivity for detecting this phoneme");

	SetLangStr("phnm.E", "E");
	SetLangStr("phnm.S", "S");
	SetLangStr("phnm.oo", "oo");
	SetLangStr("phnm.P", "P");
	SetLangStr("phnm.PBurst", "burst");
	SetLangStr("phnm.PLow", "low");

	SetLangStr("intg.IntegrationTT", "Settings related to integration with other software");
	SetLangStr("intg.Integration", "Integration");
	SetLangStr("intg.HTTP", "Control States via HTTP");
	SetLangStr("intg.HTTPTT_T", "Use this with your automation tools (check Tutorials)!");
	SetLangStr("intg.HTTPTT_1", "Listens for HTTP messages in the format:\nhttp://127.0.0.1:");
	SetLangStr("intg.HTTPTT_2", "/state?[stateIndex,active]\nhttp://127.0.0.1:");
	SetLangStr("intg.HTTPTT_3", "/state?[\"state name\",active]");
	SetLangStr("intg.HTTPPort", "HTTP Port");
	SetLangStr("intg.HTTPPortTT", "Set the port that RahiTuber listens for messages on");

	SetLangStr("intg.GamePadAPI", "Gamepad API");
	SetLangStr("intg.GamePadAPITT", "Select which method to read joystick status.");
	SetLangStr("intg.MouseAPI", "Mouse API");
	SetLangStr("intg.MouseAPITT", "Select which method to read mouse status.");
	SetLangStr("intg.MouseHandling", "Mouse Handling");
	SetLangStr("intg.MovementBased", "Movement-based");
	SetLangStr("intg.MovementTT", "Track Mouse movement instead of position\nThis is useful for games that lock your cursor");
	SetLangStr("intg.MReturnSpd", "Return Speed");
	SetLangStr("intg.MReturnSpdTT", "How fast the mouse returns to the GLOBAL neutral point");
	SetLangStr("intg.MDelay", "Delay");
	SetLangStr("intg.MDelayTT", "How long the mouse waits before starting to return");
	SetLangStr("intg.MEase", "Ease");
	SetLangStr("intg.MEaseTT", "How long the mouse accelerates/decelerates for");
	SetLangStr("intg.MDeadzone", "Deadzone");
	SetLangStr("intg.MDeadzoneTT", "How many dots the mouse must move before restarting the delay\n(Note: dots not pixels - this is raw device movement data)");
	SetLangStr("intg.MConstrain", "Constrain to bounds");
	SetLangStr("intg.MConstrainTT", "Constrain the simulated mouse within the 'distance factor' set in the Global tracking settings tab");
	SetLangStr("intg.GamePadThread", "Dedicated GamePad Thread");
	SetLangStr("intg.GamePadThreadTTT", "Requires restart when changing this and other input API settings.");
	SetLangStr("intg.GamePadThreadTT", "Handle GamePad inputs on a separate thread.\nCan improve application smoothness\nbut some reports of losing input while out of focus.");
	SetLangStr("intg.Spout", "Use Spout2");
	SetLangStr("intg.SpoutTT", "Send video output through Spout2.\n(Requires Spout2 plugin for your streaming software)");

	SetLangStr("appr.AppearanceTT", "Settings related to appearance and rendering");
	SetLangStr("appr.Appearance", "Appearance");
	SetLangStr("appr.Theme", "Theme");
	SetLangStr("appr.ThemeTT", "Set the colors of the interface.\n(psst: you can edit these in config.xml!)");
	SetLangStr("appr.LayerSetUI", "Layer Set UI");
	SetLangStr("appr.LayerSetUITT", "Choose the UI for Layer Set controls.");
	SetLangStr("appr.LayersUI_Old", "Legacy");
	SetLangStr("appr.LayersUI_Menus", "Menus");
	SetLangStr("appr.LayersUI_Buttons", "Buttons");
	SetLangStr("appr.UIScale", "UI Scale");
	SetLangStr("appr.UIScaleTT", "Change the size of the user interface.");
	SetLangStr("appr.Transparent", "Transparent");
	SetLangStr("appr.TransparentTT", "Turns the background transparent (Useful for screen capture).");
	SetLangStr("appr.BackgroundCol", "Background Color");
	SetLangStr("appr.BackgroundColTT", "Set a background color for the window.");
	SetLangStr("appr.LayerBounds", "Show Layer Bounds");
	SetLangStr("appr.LayerBoundsTT", "Shows a box around each layer, and\na marker for the pivot point.");
	SetLangStr("appr.Highlight", "Highlight Hovered");
	SetLangStr("appr.HighlightTT", "Highlight the layer bounds when the layer's menu\nis expanded or hovered");
	SetLangStr("appr.ShowFPS", "Show FPS");
	SetLangStr("appr.ShowFPSTT", "Shows an FPS counter (when menu is inactive).");
	SetLangStr("appr.Composite", "Composite onto Background");
	SetLangStr("appr.CompositeTT", "Composite the layers onto the application background color.");
	SetLangStr("appr.Premult", "Premultiply Alpha");
	SetLangStr("appr.PremultTT", "Multiply the output (captured) color by the alpha value.\nDisabling is useful for glow effects etc.");
	SetLangStr("appr.fxaa", "FXAA (Anti-Aliasing)");
	SetLangStr("appr.fxaaTT", "Use Fast approXimate Anti Aliasing.");
	SetLangStr("appr.Cutoff", "Default Alpha Cutoff");
	SetLangStr("appr.CutoffTT", "Define the minimum alpha (transparency) needed for visibility.\nValues below this will be fully transparent.\nUseful for removing unwanted soft edges from the Linear filter.");

	SetLangStr("wnd.WindowTT", "Settings related to the app window");
	SetLangStr("wnd.Window", "Window");
	SetLangStr("wnd.MenuStart", "Menu On Start");
	SetLangStr("wnd.MenuStartTT", "Start the application with the menu open.");
	SetLangStr("wnd.MonitorScale", "Follow Monitor Scale");
	SetLangStr("wnd.MonitorScaleTT", "RahiTuber attempts to match the resolution scaling\nsetting on your monitor.");
	SetLangStr("wnd.OnTop", "Always On Top");
	SetLangStr("wnd.OnTopTT", "Keeps the app above all other windows on your screen.");
	SetLangStr("wnd.VSync", "VSync");
	SetLangStr("wnd.VSyncTT", "Enable/Disable Vertical Sync.");
	SetLangStr("wnd.FPSLimit", "FPS Limit");
	SetLangStr("wnd.FPSLimitTT", "Set the FPS limit for the application.");
	SetLangStr("wnd.NameSep", "Name windows separately");
	SetLangStr("wnd.NameSepTT", "Name the window based on the Layer Set.\nUseful for if you want multiple instances of RahiTuber.");

	SetLangStr("bhv.BehaviourTT", "Settings related to app behaviour");
	SetLangStr("bhv.Behaviour", "Behaviour");
	SetLangStr("bhv.MinLayers", "Create minimal layers");
	SetLangStr("bhv.MinLayersTT1", "Recommended when creating more complex 'paper doll' style rigs");
	SetLangStr("bhv.MinLayersTT2", "New layers begin without any default movement or additional sprites");
	SetLangStr("bhv.CheckUpdates", "Check for updates");
	SetLangStr("bhv.CheckUpdatesTT", "Automatically check for updates when the application starts.");
	SetLangStr("bhv.NumberType", "Number Edit type");
	SetLangStr("bhv.NumberType0", "Slider");
	SetLangStr("bhv.NumberType1", "Drag (use Alt for fine adjustment)");
	SetLangStr("bhv.NumberType2", "Input Only");
	SetLangStr("bhv.NumberType0TT", "(Drag to adjust. CTRL + Click to type a value.)");
	SetLangStr("bhv.NumberType1TT", "(Drag, or double-click to type.\nHold Alt for fine adjustment.)");
	SetLangStr("bhv.NumberType2TT", "(Click to type a value.)");
	SetLangStr("bhv.NumberTypeTT", "Change the input method for most number fields");

	SetLangStr("bhv.Unload", "Unload images while hidden");
	SetLangStr("bhv.UnloadTT", "Remove images from RAM while they're not being used.\n  WARNING: expect a delay in visibility\n  while an unloaded image is reloading!");
	SetLangStr("bhv.UnloadTime", "Unload timeout");
	SetLangStr("bhv.UnloadTimeTT", "Set how long to wait before unloading an image.\n  If you set this to less than any\n  frequent states/blinks, expect stutter!");

	SetLangStr("bhv.AcceptDupes", "Accept duplicate layers when merging");
	SetLangStr("bhv.AcceptDupesTT", "Merged layers with duplicate GUIDs will be\nassigned a new guid instead of being discarded.");
	SetLangStr("bhv.PortablePath", "Make Portable saves image paths relative to:");
	SetLangStr("bhv.PortablePathRT", "RahiTuber");
	SetLangStr("bhv.PortablePathRTTT", "The location of RahiTuber_64.exe\n(old behaviour)");
	SetLangStr("bhv.PortablePathXML", "Portable XML");
	SetLangStr("bhv.PortablePathXMLTT", "The location of the\nportable XML being created\n(recommended)");

	SetLangStr("kbd.ShortcutsTT", "Keyboard Shortcuts");
	SetLangStr("kbd.Shortcuts", "Shortcuts");
	SetLangStr("kbd.PTT", "Push To Talk");
	SetLangStr("kbd.Button", "Button:");
	SetLangStr("kbd.Empty", "empty");
	SetLangStr("kbd.Record", "Record Key");

	SetLangStr("audio.Title", "Audio Input");
	SetLangStr("audio.DevNone", "None");
	SetLangStr("audio.DevTT", "Choose an audio input device.");
	SetLangStr("audio.RefreshTT", "Refresh input device list");
	SetLangStr("audio.MaxLevel", "Max Level");
	SetLangStr("audio.MaxLevelTT", "The maximum volume used in audio analysis.");
	SetLangStr("audio.SoftMax", "Soft Max");
	SetLangStr("audio.SoftMaxTT", "Allow the Max Level to increase temporarily if exceeded.");
	SetLangStr("audio.SoftFall", "Soft Fall");
	SetLangStr("audio.SoftFallTT", "Let audio level fall slowly after speaking.");
	SetLangStr("audio.Filter", "Audio Filter");
	SetLangStr("audio.FilterTT", "Basic low-latency filtering to cancel keyboard and mouse noise frequencies.");
	SetLangStr("audio.Compress", "Compression");
	SetLangStr("audio.CompressTT", "Use a compression curve for audio levels\n(the difference in effect reduces as the volume nears maximum).");
	SetLangStr("audio.NotFound", "Audio Device Not Found");
	SetLangStr("audio.NotFoundTxt", "The Audio Input device saved in your settings is not available.\nSwitch to the system default input device?");
	SetLangStr("audio.UseDefault", "Use Default");
	SetLangStr("audio.Ignore", "Ignore");

	SetLangStr("menuP.WndPresets", "Window Presets");
	SetLangStr("menuP.WndPresetsTxt", "Save or load presets for window attributes");
	SetLangStr("menuP.Preset", "Preset");
	SetLangStr("menuP.SavePreset", "Save Preset");
	SetLangStr("menuP.Name", "Name");
	SetLangStr("menuP.UseCurrent", "Use Current");



}


#endif