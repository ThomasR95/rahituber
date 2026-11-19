#ifndef __LANGUAGE_H__
#define __LANGUAGE_H__

#include <map>
#include <memory>
#include <list>
#include <string>


std::unique_ptr<std::map<std::string, std::string>> langStringsSingleton;



const std::string& LangStr(const std::string& key)
{
	if (!langStringsSingleton)
		langStringsSingleton = std::make_unique<std::map<std::string, std::string>>();

	return (*langStringsSingleton)[key];
}
const char* LangCStr(const std::string& key)
{
	if (!langStringsSingleton)
		langStringsSingleton = std::make_unique<std::map<std::string, std::string>>();

	return (*langStringsSingleton)[key].c_str();
}

void SetLangStr(const std::string& key, const std::string& str)
{
	if (!langStringsSingleton)
		langStringsSingleton = std::make_unique<std::map<std::string, std::string>>();

	(*langStringsSingleton)[key] = str;
}

void InitLangEN()
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

}


#endif