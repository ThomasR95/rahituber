#include "file_browser_modal.h"

#include <limits>
#include <imgui.h>
#include "imgui-SFML.h"
#include "imgui_internal.h"

#ifdef _WIN32
    #include <Windows.h>
    #include <fileapi.h>
#else
    #define MAX_PATH 4095
    #include <mntent.h>
#endif

#include "defines.h"

using namespace imgui_ext;

#ifdef _WIN32
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
#else
std::vector<std::wstring> GetVolumePaths(const wchar_t* VolumeName)
{
    std::vector<std::wstring> outNames;
    FILE* mountTable = setmntent("/etc/mtab", "r");
    if (mountTable == NULL)
    {
        outNames.push_back(L"Failed to open mount table");
        return outNames;
    }

    struct mntent* mountEntry;
    while ((mountEntry = getmntent(mountTable)) != NULL)
    {
        if (wcscmp(VolumeName, std::wstring(mountEntry->mnt_dir, mountEntry->mnt_dir + strlen(mountEntry->mnt_dir)).c_str()) == 0)
        {
            outNames.push_back(std::wstring(mountEntry->mnt_dir, mountEntry->mnt_dir + strlen(mountEntry->mnt_dir)));
        }
    }

    endmntent(mountTable);
    return outNames;
}
#endif

static void get_files_in_path(const fs::path& path, std::vector<file>& files, std::vector<std::string>& acceptedExt) {
  files.clear();

  if (path.has_parent_path()) 
  {
    if (path.parent_path() == path)
    {

#ifdef _WIN32
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
              std::error_code ec;
              valid = fs::exists(drive, ec);
            }
            catch (fs::filesystem_error err) {
              valid = false;
            }
            
            if (valid)
            {
              files.push_back({
              "[Switch to " + drive.u8string() + "]",
              drive
                });
            }
            
          }
          foundDrive = FindNextVolumeW(handle, volumeName, ARRAYSIZE(volumeName));
        }
        FindVolumeClose(handle);
#else
        wchar_t volumeName[MAX_PATH] = L"";
        wchar_t deviceName[MAX_PATH] = L"";
        FILE* handle = setmntent("/etc/mtab", "r");

        if (handle != NULL)
        {
            bool foundDrive = true;
            struct mntent* mountEntry;
            while (foundDrive && (mountEntry = getmntent(handle)) != NULL)
            {
                std::string volumeStr = mountEntry->mnt_dir;
                std::mbstowcs(volumeName, volumeStr.c_str(), volumeStr.size() + 1);
                auto paths = GetVolumePaths(volumeName);

                if (!paths.empty())
                {
                    std::wstring driveNameString = paths[0];
                    fs::path drive(driveNameString);
                    bool valid = true;
                    try{
                        std::error_code ec;
                        valid = fs::exists(drive, ec);
                    } catch (fs::filesystem_error& err){
                        valid = false;
                    }

                    if (valid)
                    {
                        files.push_back({
                            "[Switch to " + drive.u8string() + "]",
                            drive
                        });
                    }
                }
                foundDrive = (mountEntry != NULL);
            }
            endmntent(handle);

#endif
       
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
          dirPath.filename().u8string() + "/",
          dirPath
        });
  }

  for (const fs::directory_entry& dirEntry : fs::directory_iterator(path)) {
    const fs::path& dirPath = dirEntry.path();

    if (!fs::is_directory(dirPath))
    {
      std::string ext = dirPath.extension().u8string();
      for (char& c : ext)
        c = tolower(c);

      bool accepted = false;
      for (auto& str : acceptedExt)
        if (str == ext)
          accepted = true;

      if(accepted)
      files.push_back({
          " " + dirPath.filename().u8string(),
          dirPath
        });
    }
      
  }
}

static const int clamp_size_t_to_int(const size_t data) {
  static const int max_int = (std::numeric_limits<int>::max)();
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
const bool file_browser_modal::render(const bool isVisible, std::string& outPath, bool saving) {
  bool result = false;

  auto& style = ImGui::GetStyle();
  auto& io = ImGui::GetIO();
  float fontSize = ImGui::GetFont()->FontSize;
  float fontFrameHeight = style.FramePadding.y * 2 + fontSize * 0.5 + style.ItemSpacing.y * 0.5;
  float uiScale = io.FontGlobalScale * 2;

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

      if (initDirectory.empty() || initDirectory == "")
      {
        m_currentPath = fs::current_path();
        m_currentPathIsDir = true;
      }

      //Update paths based on current path
      get_files_in_path(initDirectory, m_filesInScope, _acceptedExt);

      ImVec2 wSize = ImGui::GetWindowSize();
      ImVec2 wPos = ImGui::GetWindowPos();

      m_width = 480;
      //m_height = 23 * fontFrameHeight;

      ImGui::SetNextWindowSize({ m_width * uiScale, -1 });
      ImGui::SetNextWindowPos({ wPos.x, wPos.y });

      //Make the modal visible.
      ImGui::OpenPopup(m_title);
    }

  }

  bool isOpen = true;
  if (ImGui::BeginPopupModal(m_title, &isOpen, modal_flags)) {


    ImGui::SetWindowSize({ m_width * uiScale, -1 });

    std::string dir = m_currentPath.parent_path().u8string();
    if(fs::is_directory(m_currentPath))
      dir = m_currentPath.u8string();

    ImGui::PushItemWidth(-1);

    ImGui::PushID("DirectoryBox");
    char dirBuf[MAX_PATH] = " ";
    ANSIToUTF8(dir).copy(dirBuf, MAX_PATH);
    if (ImGui::InputText("", dirBuf, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll))
    {
      fs::path newPath(UTF8ToANSI(dirBuf));
      bool valid = false;
      std::error_code ec;
      try { valid = fs::exists(newPath, ec); }
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

    int sel = 0;
    for (file& filename : m_filesInScope)
    {
      if (filename.path == m_currentPath)
      {
        m_selection = sel;
        break;
      }
      sel++;
    }
    
    ImGui::PushID(m_currentPath.u8string().c_str());
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

    std::string filepath = "";

    std::string selectBtn = "Select";
    if (saving)
    {
      if (!fs::is_directory(m_currentPath))
        filepath = m_currentPath.string();
      ImGui::TextWrapped(ANSIToUTF8(filepath).data());

      std::string editName = m_currentPath.filename().replace_extension("").string();
      char editBuf[MAX_PATH] = " ";
      ANSIToUTF8(editName).copy(editBuf, MAX_PATH);
      if (ImGui::InputText("Layer Set Name", editBuf, MAX_PATH, ImGuiInputTextFlags_AutoSelectAll | ImGuiInputTextFlags_ElideLeft))
      {
        m_currentPath.replace_filename(UTF8ToANSI(editBuf));
        if (m_currentPath.has_extension() == false)
        {
          m_currentPath = fs::path(m_currentPath.string() + ".xml");
        }
        else if (m_currentPath.extension().string() != ".xml")
        {
          m_currentPath.replace_extension(".xml");
        }
        
        m_currentPathIsDir = fs::is_directory(m_currentPath);
      }

      std::error_code ec;
      if (fs::exists(m_currentPath, ec))
        selectBtn = "Overwrite";
      else
        selectBtn = "Save";
    }
    else
    {
      
      if (!fs::is_directory(m_currentPath))
        filepath = m_currentPath.filename().u8string();
      ImGui::TextWrapped(filepath.data());
    }

    float buttonsYPos = ImGui::GetCursorPosY();

    ImGui::Spacing();

    float selectBtnWidth = ImGui::CalcTextSize(selectBtn.c_str()).x + style.FramePadding.x * 2;
    float cancelBtnWidth = ImGui::CalcTextSize("Cancel").x + style.FramePadding.x * 2;

    ImGui::SetCursorPos({ ImGui::GetContentRegionAvail().x - (selectBtnWidth + cancelBtnWidth + style.ItemSpacing.x), buttonsYPos });

    if (LesserButton("Cancel"))
    {
      ImGui::CloseCurrentPopup();
    }

    ImGui::SetCursorPos({ ImGui::GetContentRegionAvail().x - selectBtnWidth , buttonsYPos });

    // Make the "Select" button look / act disabled if the current selection is a directory.
    ImGui::BeginDisabled(m_currentPathIsDir);
    if (ImGui::Button(selectBtn.c_str())) {
      ImGui::CloseCurrentPopup();

      m_chosenDir = m_currentPath.parent_path();

      outPath = m_currentPath.string();
      result = true;
    }
    ImGui::EndDisabled();

    ImGui::EndPopup();

  }

  return result;
}
