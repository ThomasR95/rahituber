#include "file_browser_modal.h"

#include <limits>
#include <imgui.h>

#include <Windows.h>
#include <fileapi.h>

using namespace imgui_ext;

std::vector<std::wstring> GetVolumePaths(
  __in PWCHAR VolumeName
)
{
  DWORD  CharCount = MAX_PATH + 1;
  PWCHAR Names = NULL;
  PWCHAR NameIdx = NULL;
  BOOL   Success = FALSE;

  std::vector<std::wstring> outNames;

  for (;;)
  {
    //  Allocate a buffer to hold the paths.
    Names = (PWCHAR) new BYTE[CharCount * sizeof(WCHAR)];

    if (!Names)
    {
      //  If memory can't be allocated, return.
      return outNames;
    }

    //  Obtain all of the paths for this volume.
    Success = GetVolumePathNamesForVolumeNameW(
      VolumeName, Names, CharCount, &CharCount
    );

    if (Success)
    {
      break;
    }

    if (GetLastError() != ERROR_MORE_DATA)
    {
      DWORD Error = GetLastError();
      outNames.push_back(L"GetVolumePathNamesForVolumeNameW failed with error code " + std::to_wstring(Error));
      break;
    }

    //  Try again with the new suggested size.
    delete[] Names;
    Names = NULL;
  }

  if (Success)
  {
    //  output the various paths.
    for (NameIdx = Names;
      NameIdx[0] != L'\0';
      NameIdx += wcslen(NameIdx) + 1)
    {
      outNames.push_back(NameIdx);
      //wprintf(L"  %s", NameIdx);
    }
  }

  if (Names != NULL)
  {
    delete[] Names;
    Names = NULL;
  }

  return outNames;
}

static void get_files_in_path(const fs::path& path, std::vector<file>& files, std::vector<std::string>& acceptedExt) {
  files.clear();

  if (path.has_parent_path()) 
  {
    if (path.parent_path() == path)
    {


      WCHAR  volumeName[MAX_PATH] = L"";
      WCHAR  deviceName[MAX_PATH] = L"";
      HANDLE handle = INVALID_HANDLE_VALUE;
      handle = FindFirstVolumeW(volumeName, ARRAYSIZE(volumeName));
      if (handle != INVALID_HANDLE_VALUE && handle != nullptr)
      {
        bool foundDrive = true;
        while (foundDrive)
        {
          size_t index = wcslen(volumeName) - 1;

          if (volumeName[0] != L'\\' ||
            volumeName[1] != L'\\' ||
            volumeName[2] != L'?' ||
            volumeName[3] != L'\\' ||
            volumeName[index] != L'\\')
          {
            // Bad path
            break;
          }

          //
          //  QueryDosDeviceW does not allow a trailing backslash,
          //  so temporarily remove it.
          volumeName[index] = L'\0';

          DWORD CharCount = QueryDosDeviceW(&volumeName[4], deviceName, ARRAYSIZE(deviceName));

          volumeName[index] = L'\\';

          auto paths = GetVolumePaths(volumeName);
          if (!paths.empty())
          {
            std::wstring driveNameString = paths[0];
            fs::path drive(driveNameString);
            bool valid = true;
            try {
              valid = fs::exists(drive);
            }
            catch (fs::filesystem_error err) {
              valid = false;
            }
            
            if (valid)
            {
              files.push_back({
              "[Switch to " + drive.string() + "]",
              drive
                });
            }
            
          }
          foundDrive = FindNextVolumeW(handle, volumeName, ARRAYSIZE(volumeName));
        }
        FindVolumeClose(handle);
       
      }
    }
    else
    {
      files.push_back({
        ".. [go up]",
        path.parent_path()
        });
    }
    
  }

  for (const fs::directory_entry& dirEntry : fs::directory_iterator(path)) {
    const fs::path& dirPath = dirEntry.path();

    if(fs::is_directory(dirPath))
      files.push_back({
          dirPath.filename().string() + "/",
          dirPath
        });
  }

  for (const fs::directory_entry& dirEntry : fs::directory_iterator(path)) {
    const fs::path& dirPath = dirEntry.path();

    if (!fs::is_directory(dirPath))
    {
      std::string ext = dirPath.extension().string();
      for (char& c : ext)
        c = tolower(c);

      bool accepted = false;
      for (auto& str : acceptedExt)
        if (str == ext)
          accepted = true;

      if(accepted)
      files.push_back({
          " " + dirPath.filename().string(),
          dirPath
        });
    }
      
  }
}

static const int clamp_size_t_to_int(const size_t data) {
  static const int max_int = std::numeric_limits<int>::max();
  return static_cast<int>(data > max_int ? max_int : data);
}

static bool vector_file_items_getter(void* data, int idx, const char** out_text) {
  const std::vector<file>* v = reinterpret_cast<std::vector<file>*>(data);
  const int elementCount = clamp_size_t_to_int(v->size());
  if (idx < 0 || idx >= elementCount) return false;
  *out_text = v->at(idx).alias.data();
  return true;
}

static constexpr int modal_flags =
ImGuiWindowFlags_NoResize |
ImGuiWindowFlags_NoCollapse |
ImGuiWindowFlags_NoScrollbar;

file_browser_modal::file_browser_modal(const char* title) :
  m_title(title),
  m_oldVisibility(false),
  m_selection(0),
  m_currentPath(fs::current_path()),
  m_currentPathIsDir(true) {

}

// Will return true if file selected.
const bool file_browser_modal::render(const bool isVisible, std::string& outPath) {
  bool result = false;

  if (m_oldVisibility != isVisible) {
    m_oldVisibility = isVisible;
    //Visiblity has changed.

    if (isVisible) {
      //Only run when the visibility state changes to visible.

      //Reset the path to the initial path.
      if(m_chosenDir.empty())
        m_currentPath = fs::current_path();
      else
        m_currentPath = m_chosenDir;

      m_currentPathIsDir = fs::is_directory(m_currentPath);

      fs::path initDirectory = m_currentPath;
      if (!m_currentPathIsDir)
        initDirectory = m_currentPath.parent_path();

      //Update paths based on current path
      get_files_in_path(initDirectory, m_filesInScope, _acceptedExt);

      ImGui::SetNextWindowSize({ 400, 460 });
      //Make the modal visible.
      ImGui::OpenPopup(m_title);
    }

  }

  bool isOpen = true;
  if (ImGui::BeginPopupModal(m_title, &isOpen, modal_flags)) {

    std::string dir = m_currentPath.parent_path().string();
    if(fs::is_directory(m_currentPath))
      dir = m_currentPath.string();

    ImGui::PushItemWidth(-1);

    ImGui::PushID("DirectoryBox");
    char dirBuf[MAX_PATH] = " ";
    dir.copy(dirBuf, dir.length());
    if (ImGui::InputText("", dirBuf, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll))
    {
      fs::path newPath(dirBuf);
      bool valid = false;
      try { valid = fs::exists(newPath); }
      catch (fs::filesystem_error) { valid = false; }
      if (valid)
      {
        m_currentPath = newPath;
        m_currentPathIsDir = fs::is_directory(m_currentPath);

        //If the selection is a directory, repopulate the list with the contents of that directory.
        if (m_currentPathIsDir) {
          get_files_in_path(m_currentPath, m_filesInScope, _acceptedExt);
          m_selection = 0;
        }
      }
    }
    ImGui::PopID();
    
    ImGui::PushID(m_currentPath.string().c_str());
    if (ImGui::ListBox("##", &m_selection, vector_file_items_getter, &m_filesInScope, m_filesInScope.size(), 20)) {

      //Update current path to the selected list item.
      m_currentPath = m_filesInScope[m_selection].path;
      m_currentPathIsDir = fs::is_directory(m_currentPath);

      //If the selection is a directory, repopulate the list with the contents of that directory.
      if (m_currentPathIsDir) {
        get_files_in_path(m_currentPath, m_filesInScope, _acceptedExt);
        m_selection = 0;
      }
    }
    ImGui::PopID();
    ImGui::PopItemWidth();

    std::string file = "";
    if (!fs::is_directory(m_currentPath))
      file = m_currentPath.filename().string();
    ImGui::TextWrapped(file.data());

    ImGui::Spacing();
    ImGui::SameLine(ImGui::GetWindowWidth() - 60);

    // Make the "Select" button look / act disabled if the current selection is a directory.
    if (m_currentPathIsDir) {

      static const ImVec4 disabledColor = { 0.3f, 0.3f, 0.3f, 1.0f };

      ImGui::PushStyleColor(ImGuiCol_Button, disabledColor);
      ImGui::PushStyleColor(ImGuiCol_ButtonActive, disabledColor);
      ImGui::PushStyleColor(ImGuiCol_ButtonHovered, disabledColor);

      ImGui::Button("Select");

      ImGui::PopStyleColor();
      ImGui::PopStyleColor();
      ImGui::PopStyleColor();

    }
    else {

      if (ImGui::Button("Select")) {
        ImGui::CloseCurrentPopup();

        m_chosenDir = m_currentPath.parent_path();

        outPath = m_currentPath.string();
        result = true;
      }

    }

    ImGui::EndPopup();

  }

  return result;
}