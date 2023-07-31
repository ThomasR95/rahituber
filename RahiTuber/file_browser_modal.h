#pragma once

// NOTE - FILE BROWSER IMPLEMENTATION LIFTED FROM
// https://codereview.stackexchange.com/questions/194553/c-dear-imgui-file-browser
// 

#include <vector>
#include <string>
#include <filesystem>

// With Visual Studio compiler, filesystem is still "experimental"
namespace fs = std::filesystem;

namespace imgui_ext {

  const struct file {
    std::string alias;
    fs::path path;
  };

  class file_browser_modal final {

    //static const int modal_flags;

    const char* m_title;

    bool m_oldVisibility;

    int m_selection;

    fs::path m_currentPath;
    bool m_currentPathIsDir;

    fs::path m_chosenDir;

    std::vector<file> m_filesInScope;

  public:

    file_browser_modal(const char* title);

    const bool render(const bool isVisible, std::string& outPath);

    fs::path GetLastChosenDir() { return m_chosenDir; }
    void SetStartingDir(fs::path _newDir) { m_chosenDir = _newDir; }

  };

};