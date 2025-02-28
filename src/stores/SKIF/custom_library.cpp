
#include <stores/skif/custom_library.h>
#include <wtypes.h>
#include <filesystem>
#include <fsutil.h>


bool SKIF_RemoveCustomAppID (uint32_t appid)
{
  std::wstring SKIFRegistryKey = SK_FormatStringW (LR"(SOFTWARE\Kaldaien\Special K\Games\%i\)", appid);
  std::wstring SKIFCustomPath  = SK_FormatStringW (LR"(%ws\Assets\Custom\%i\)", path_cache.specialk_userdata, appid);
  SKIFCustomPath += L"\0\0";

  SHFILEOPSTRUCTW fileOps{};
  fileOps.wFunc = FO_DELETE;
  fileOps.pFrom = SKIFCustomPath.c_str();
  fileOps.fFlags = FOF_NO_UI;

  SHFileOperationW (&fileOps);
  RegDeleteKeyW    (HKEY_CURRENT_USER, SKIFRegistryKey.c_str());

  return true;
}

int SKIF_AddCustomAppID (
  std::vector<std::pair<std::string, app_record_s>>* apps,
    std::wstring name, std::wstring path, std::wstring args)
{
  /*
    name          - String -- Title/Name
    exe           - String -- Full Path to executable
    launchOptions - String -- Cmd line args
    id            - Autogenerated
    installDir    - Autogenerated
    exeFileName   - Autogenerated
  */

  uint32_t appId = 1; // Assume 1 in case the registry fails

  HKEY    hKey;
  DWORD32 dwData = 0;
  DWORD   dwSize = sizeof (DWORD32);
  bool    failed = false;

  // We're expressively using .c_str() and not .wstring()
  //   because the latter adds a bunch of undesireable
  //     null terminators that screw the registry over.
  std::filesystem::path p = std::filesystem::path (path);
  std::wstring exe         = std::wstring(p.c_str()),
               exeFileName = std::wstring(p.filename().c_str()),
               installDir  = std::wstring(p.parent_path().c_str());
  
  // Strip null terminators
  //name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
  //args.erase(std::find(args.begin(), args.end(), '\0'), args.end());

  if (RegCreateKeyExW (HKEY_CURRENT_USER, LR"(SOFTWARE\Kaldaien\Special K\Games\)", 0, NULL, REG_OPTION_NON_VOLATILE, KEY_READ | KEY_WRITE, NULL, &hKey, NULL) == ERROR_SUCCESS)
  {
    if (RegGetValueW (hKey, NULL, L"NextID", RRF_RT_REG_DWORD, NULL, &dwData, &dwSize) == ERROR_SUCCESS)
      appId = dwData;

    std::wstring wsAppID = std::to_wstring(appId);

    if (ERROR_SUCCESS != RegSetKeyValue (hKey, wsAppID.c_str(), L"ID",            REG_DWORD, &appId, sizeof(DWORD)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, wsAppID.c_str(), L"Name",          REG_SZ,    (LPBYTE) name       .data ( ),
                                                                                              (DWORD) name       .size ( ) * sizeof(wchar_t)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, wsAppID.c_str(), L"Exe",           REG_SZ,    (LPBYTE) exe        .data ( ),
                                                                                              (DWORD) exe        .size ( ) * sizeof(wchar_t)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, wsAppID.c_str(), L"ExeFileName",   REG_SZ,    (LPBYTE) exeFileName.data ( ),
                                                                                              (DWORD) exeFileName.size ( ) * sizeof(wchar_t)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, wsAppID.c_str(), L"InstallDir",    REG_SZ,    (LPBYTE) installDir. data ( ),
                                                                                              (DWORD) installDir. size ( ) * sizeof(wchar_t)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, wsAppID.c_str(), L"LaunchOptions", REG_SZ,    (LPBYTE) args.       data ( ),
                                                                                              (DWORD) args.       size ( ) * sizeof(wchar_t)))
      failed = true;

    if (! failed)
    {
      PLOG_INFO << "Custom game:";
      PLOG_INFO << "ID:            " << std::to_string(appId);
      PLOG_INFO << "Name:          " << name;
      PLOG_INFO << "Exe:           " << exe;
      PLOG_INFO << "ExeFileName:   " << exeFileName;
      PLOG_INFO << "InstallDir:    " << installDir;
      PLOG_INFO << "LaunchOptions: " << args;
      PLOG_INFO << "Successfully added custom game to SKIF!";

      uint32_t appIdNext = appId + 1;
      RegSetKeyValue(hKey, NULL, L"NextID", REG_DWORD, &appIdNext, sizeof(DWORD));
    }

    RegCloseKey (hKey);
  }

  if (! failed)
  {
    app_record_s record(appId);

    record.store = "SKIF";
    record.type  = "Game";
    record._status.installed = true;
    record.names.normal = SK_WideCharToUTF8(name);
    
    // Strip game names from special symbols
    const char *chars = (const char *)u8"���";
    for (unsigned int i = 0; i < strlen(chars); ++i)
      record.names.normal.erase(std::remove(record.names.normal.begin(), record.names.normal.end(), chars[i]), record.names.normal.end());

    // Strip null terminators
    record.names.normal.erase(std::find(record.names.normal.begin(), record.names.normal.end(), '\0'), record.names.normal.end());

    // Add (recently added) at the end of the name
    //record.names.normal = SK_FormatString("%s (recently added)", record.names.normal.c_str());

    record.ImGuiLabelAndID = SK_FormatString("%s (recently added)###%s%i", record.names.normal.c_str(), record.store.c_str(), record.id);

    record.names.all_upper = record.names.normal;
    std::for_each (record.names.all_upper.begin ( ), record.names.all_upper.end ( ), [](char& c) {c = static_cast<char>(::toupper (c));});

    record.install_dir = installDir;
    
    app_record_s::launch_config_s lc;
    lc.id               = 0;
    lc.valid            = true;
    lc.store            = L"SKIF";
    lc.executable       = exeFileName;
    lc.executable_path  = exe;
    lc.working_dir      = record.install_dir;
    lc.launch_options   = args;

    record.launch_configs[0] = lc;
    record.specialk.profile_dir = exeFileName; // THIS CAN BE WRONG!!!!
    record.specialk.injection.injection.type = sk_install_state_s::Injection::Type::Global;

    std::pair <std::string, app_record_s>
      SKIF(record.names.normal, record);

    apps->emplace_back(SKIF);

    return appId;
  }

  return 0;
}

bool SKIF_ModifyCustomAppID (app_record_s* pApp, std::wstring name, std::wstring path, std::wstring args)
{
  /*
    name          - String -- Title/Name
    exe           - String -- Full Path to executable
    launchOptions - String -- Cmd line args
    id            - Autogenerated
    installDir    - Autogenerated
    exeFileName   - Autogenerated
  */

  HKEY hKey;
  bool failed = false;

  // We're expressively using .c_str() and not .wstring()
  //   because the latter adds a bunch of undesireable
  //     null terminators that screw the registry over.
  std::filesystem::path p = std::filesystem::path (path);
  std::wstring exe         = std::wstring(p.c_str()),
               exeFileName = std::wstring(p.filename().c_str()),
               installDir  = std::wstring(p.parent_path().c_str());

  std::wstring key = SK_FormatStringW(LR"(SOFTWARE\Kaldaien\Special K\Games\%lu)", pApp->id);
  
  // Strip null terminators
  //name.erase(std::find(name.begin(), name.end(), '\0'), name.end());
  //args.erase(std::find(args.begin(), args.end(), '\0'), args.end());

  if (RegOpenKeyExW (HKEY_CURRENT_USER, key.c_str(), 0, KEY_READ | KEY_WRITE, &hKey) == ERROR_SUCCESS)
  {
    std::wstring wsAppID = std::to_wstring (pApp->id);

    if (ERROR_SUCCESS != RegSetKeyValue (hKey, NULL, L"Name",          REG_SZ,    (LPBYTE) name       .data ( ),
                                                                                   (DWORD) name       .size ( ) * sizeof(wchar_t)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, NULL, L"Exe",           REG_SZ,    (LPBYTE) exe        .data ( ),
                                                                                   (DWORD) exe        .size ( ) * sizeof(wchar_t)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, NULL, L"ExeFileName",   REG_SZ,    (LPBYTE) exeFileName.data ( ),
                                                                                   (DWORD) exeFileName.size ( ) * sizeof(wchar_t)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, NULL, L"InstallDir",    REG_SZ,    (LPBYTE) installDir. data ( ),
                                                                                   (DWORD) installDir. size ( ) * sizeof(wchar_t)))
      failed = true;
    if (ERROR_SUCCESS != RegSetKeyValue (hKey, NULL, L"LaunchOptions", REG_SZ,    (LPBYTE) args.       data ( ),
                                                                                   (DWORD) args.       size ( ) * sizeof(wchar_t)))
      failed = true;

    RegCloseKey (hKey);
  }

  if (failed == false)
  {
    pApp->names.normal = SK_WideCharToUTF8(name).c_str();
    
    // Strip game names from special symbols
    const char* chars = (const char *)u8"���";
    for (unsigned int i = 0; i < strlen(chars); ++i)
      pApp->names.normal.erase(std::remove(pApp->names.normal.begin(), pApp->names.normal.end(), chars[i]), pApp->names.normal.end());

    // Strip null terminators
    pApp->names.normal.erase(std::find(pApp->names.normal.begin(), pApp->names.normal.end(), '\0'), pApp->names.normal.end());

    pApp->ImGuiLabelAndID = SK_FormatString("%s###%s%i", pApp->names.normal.c_str(), pApp->store.c_str(), pApp->id);

    pApp->install_dir = installDir;
    pApp->launch_configs[0].executable = exeFileName;
    pApp->launch_configs[0].executable_path = exe;
    pApp->launch_configs[0].working_dir = pApp->install_dir;
    pApp->launch_configs[0].launch_options = args;
    pApp->specialk.profile_dir = exeFileName; // THIS CAN BE WRONG!!!!
  }

  return ! failed;
}

void SKIF_GetCustomAppIDs (std::vector<std::pair<std::string, app_record_s>>* apps)
{
  PLOG_INFO << "Detecting custom SKIF games...";

  HKEY    hKey;
  DWORD   dwIndex = 0, dwResult, dwSize;
  DWORD32 dwData  = 0;
  WCHAR   szSubKey[MAX_PATH * sizeof (WCHAR)];
  WCHAR   szData  [     500 * sizeof (WCHAR)];

  extern uint32_t SelectNewSKIFGame;

  /* Load custom titles from registry */
  if (RegOpenKeyExW (HKEY_CURRENT_USER, LR"(SOFTWARE\Kaldaien\Special K\Games\)", 0, KEY_READ, &hKey) == ERROR_SUCCESS)
  {
    if (RegQueryInfoKeyW (hKey, NULL, NULL, NULL, &dwResult, NULL, NULL, NULL, NULL, NULL, NULL, NULL) == ERROR_SUCCESS)
    {
      do
      {
        dwSize   = sizeof (szSubKey) / sizeof (WCHAR);
        dwResult = RegEnumKeyExW (hKey, dwIndex, szSubKey, &dwSize, NULL, NULL, NULL, NULL);

        if (dwResult == ERROR_NO_MORE_ITEMS)
          break;

        if (dwResult == ERROR_SUCCESS)
        {
          dwSize = sizeof (DWORD32);
          if (RegGetValueW (hKey, szSubKey, L"ID", RRF_RT_REG_DWORD, NULL, &dwData, &dwSize) == ERROR_SUCCESS)
          {
            app_record_s record (dwData);

            record.id = dwData;
            record.store = "SKIF";
            record.type  = "Game";
            record._status.installed = true;

            dwSize = sizeof(szData) / sizeof (WCHAR);
            if (RegGetValueW (hKey, szSubKey, L"Name", RRF_RT_REG_SZ, NULL, &szData, &dwSize) == ERROR_SUCCESS)
            {
              record.names.normal = SK_WideCharToUTF8 (szData);

              // Strip null terminators
              //record.names.normal.erase(std::find(record.names.normal.begin(), record.names.normal.end(), '\0'), record.names.normal.end());

              // Add (recently added) at the end of a newly added game
              if (SelectNewSKIFGame == record.id)
                record.names.normal = SK_FormatString("%s (recently added)", record.names.normal.c_str());
            }

            dwSize = sizeof (szData) / sizeof (WCHAR);
            if (RegGetValueW (hKey, szSubKey, L"InstallDir", RRF_RT_REG_SZ, NULL, &szData, &dwSize) == ERROR_SUCCESS)
              record.install_dir = szData;

            dwSize = sizeof (szData) / sizeof (WCHAR);
            if (RegGetValueW (hKey, szSubKey, L"ExeFileName", RRF_RT_REG_SZ, NULL, &szData, &dwSize) == ERROR_SUCCESS) // L"Exe"
            {
              app_record_s::launch_config_s lc;
              lc.id           = 0;
              lc.valid        = true;
              lc.store        = L"SKIF";
              lc.executable   = szData;
              lc.working_dir  = record.install_dir;

              dwSize = sizeof (szData) / sizeof (WCHAR);
              if (RegGetValueW(hKey, szSubKey, L"Exe", RRF_RT_REG_SZ, NULL, &szData, &dwSize) == ERROR_SUCCESS)
                lc.executable_path = szData;

              dwSize = sizeof (szData) / sizeof (WCHAR);
              if (RegGetValueW(hKey, szSubKey, L"LaunchOptions", RRF_RT_REG_SZ, NULL, &szData, &dwSize) == ERROR_SUCCESS)
                lc.launch_options = szData;

              record.launch_configs[0] = lc;

              /*
              dwSize = sizeof (szData) / sizeof (WCHAR);
              if (RegGetValueW (hKey, szSubKey, L"ExeFileName", RRF_RT_REG_SZ, NULL, &szData, &dwSize) == ERROR_SUCCESS)
                record.specialk.profile_dir = szData;
              */
              record.specialk.profile_dir = lc.executable;

              record.specialk.injection.injection.type = sk_install_state_s::Injection::Type::Global;

              std::pair <std::string, app_record_s>
                pair (record.names.normal, record);

              apps->emplace_back (pair);
            }
          }
        }

        dwIndex++;

      } while (1);
    }

    RegCloseKey (hKey);
  }
}

void app_skif_s::launchGame(void)
{

}

ID3D11ShaderResourceView*
app_skif_s::getCover (void)
{
  if (this->textures.cover != nullptr)
    return this->textures.cover.p;

  std::wstring path;
  std::wstring SKIFCustomPath;

  SKIFCustomPath = SK_FormatStringW (LR"(%ws\Assets\Custom\%i\)", path_cache.specialk_userdata, this->id);
  SKIFCustomPath += L"cover";

  if      (PathFileExistsW ((SKIFCustomPath + L".png").c_str()))
    path =                   SKIFCustomPath + L".png";
  else if (PathFileExistsW ((SKIFCustomPath + L".jpg").c_str()))
    path =                   SKIFCustomPath + L".jpg";

  this->textures.isCustomCover = (path != L"\0");

  if (this->textures.isCustomCover && this->loadCoverAsFile (path))
    return this->textures.cover.p;

  return nullptr;
}

ID3D11ShaderResourceView*
app_skif_s::getIcon (void)
{
  return nullptr;
}
