#include "app.h"
#include <windows.h>
#include <winhttp.h>
#include <fstream>
#include <thread>
#include <sstream>
#include <chrono>

#pragma comment(lib, "winhttp.lib")
namespace fs = std::filesystem;

bool DownloadImageWinHTTP(LPCWSTR url, LPCWSTR savePath) {
    HINTERNET hSession = WinHttpOpen(L"HammerLauncher/1.0", WINHTTP_ACCESS_TYPE_DEFAULT_PROXY, WINHTTP_NO_PROXY_NAME, WINHTTP_NO_PROXY_BYPASS, 0);
    if (!hSession) return false;

    URL_COMPONENTS urlComp;
    ZeroMemory(&urlComp, sizeof(urlComp));
    urlComp.dwStructSize = sizeof(urlComp);
    urlComp.dwHostNameLength = (DWORD)-1;
    urlComp.dwUrlPathLength = (DWORD)-1;

    if (!WinHttpCrackUrl(url, (DWORD)wcslen(url), 0, &urlComp)) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    std::wstring hostName(urlComp.lpszHostName, urlComp.dwHostNameLength);
    std::wstring urlPath(urlComp.lpszUrlPath, urlComp.dwUrlPathLength);

    HINTERNET hConnect = WinHttpConnect(hSession, hostName.c_str(), urlComp.nPort, 0);
    if (!hConnect) {
        WinHttpCloseHandle(hSession);
        return false;
    }

    DWORD flags = (urlComp.nScheme == INTERNET_SCHEME_HTTPS) ? WINHTTP_FLAG_SECURE : 0;
    HINTERNET hRequest = WinHttpOpenRequest(hConnect, L"GET", urlPath.c_str(), NULL, WINHTTP_NO_REFERER, WINHTTP_DEFAULT_ACCEPT_TYPES, flags);
    if (!hRequest) {
        WinHttpCloseHandle(hConnect);
        WinHttpCloseHandle(hSession);
        return false;
    }

    bool success = false;
    if (WinHttpSendRequest(hRequest, WINHTTP_NO_ADDITIONAL_HEADERS, 0, WINHTTP_NO_REQUEST_DATA, 0, 0, 0) &&
        WinHttpReceiveResponse(hRequest, NULL)) {

        std::ofstream file(savePath, std::ios::binary);
        if (file.is_open()) {
            DWORD dwSize = 0;
            do {
                DWORD dwDownloaded = 0;
                if (!WinHttpQueryDataAvailable(hRequest, &dwSize) || dwSize == 0) break;

                char* pszOutBuffer = new char[dwSize + 1];
                if (WinHttpReadData(hRequest, (LPVOID)pszOutBuffer, dwSize, &dwDownloaded)) {
                    file.write(pszOutBuffer, dwDownloaded);
                }
                delete[] pszOutBuffer;
            } while (dwSize > 0);
            success = true;
        }
    }

    WinHttpCloseHandle(hRequest);
    WinHttpCloseHandle(hConnect);
    WinHttpCloseHandle(hSession);
    return success;
}

void LoadSteamGames(wxEvtHandler* handler, std::vector<GameData>& games, fs::path& steamRootPath) {
    std::vector<fs::path> steamappsPaths;
    HKEY hKey;
    char steamPath[512];
    DWORD bufferSize = sizeof(steamPath);

    if (RegOpenKeyExA(HKEY_CURRENT_USER, "Software\\Valve\\Steam", 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, "SteamPath", nullptr, nullptr, (LPBYTE)steamPath, &bufferSize) == ERROR_SUCCESS) {
            steamRootPath = fs::path(steamPath);
            steamappsPaths.push_back(steamRootPath / "steamapps");
        }
        RegCloseKey(hKey);
    }

    for (char drive = 'C'; drive <= 'Z'; ++drive) {
        std::string root = std::string(1, drive) + ":\\";
        std::vector<fs::path> possiblePaths = {
            fs::path(root) / "SteamLibrary" / "steamapps",
            fs::path(root) / "Steam" / "steamapps"
        };

        for (const auto& p : possiblePaths) {
            try {
                if (fs::exists(p) && fs::is_directory(p)) {
                    bool alreadyExists = false;
                    for (const auto& existing : steamappsPaths) {
                        if (fs::equivalent(p, existing)) { alreadyExists = true; break; }
                    }
                    if (!alreadyExists) steamappsPaths.push_back(p);
                }
            }
            catch (...) { continue; }
        }
    }

    for (const auto& steamappsPath : steamappsPaths) {
        try {
            if (!fs::exists(steamappsPath)) continue;
            for (const auto& entry : fs::directory_iterator(steamappsPath)) {
                if (entry.is_regular_file() && entry.path().filename().string().find("appmanifest_") == 0) {
                    std::ifstream acfFile(entry.path());
                    if (acfFile.is_open()) {
                        std::string line, appId = "", gameName = "", installDir = "";
                        while (std::getline(acfFile, line)) {
                            if (line.find("\"appid\"") != std::string::npos) {
                                size_t q1 = line.find('"', line.find("\"appid\"") + 7); size_t q2 = line.find('"', q1 + 1);
                                if (q1 != std::string::npos && q2 != std::string::npos) appId = line.substr(q1 + 1, q2 - q1 - 1);
                            }
                            if (line.find("\"name\"") != std::string::npos) {
                                size_t q1 = line.find('"', line.find("\"name\"") + 6); size_t q2 = line.find('"', q1 + 1);
                                if (q1 != std::string::npos && q2 != std::string::npos) gameName = line.substr(q1 + 1, q2 - q1 - 1);
                            }
                            if (line.find("\"installdir\"") != std::string::npos) {
                                size_t q1 = line.find('"', line.find("\"installdir\"") + 12); size_t q2 = line.find('"', q1 + 1);
                                if (q1 != std::string::npos && q2 != std::string::npos) installDir = line.substr(q1 + 1, q2 - q1 - 1);
                            }
                        }

                        if (!installDir.empty() && !gameName.empty()) {
                            fs::path gamePath = steamappsPath / "common" / installDir;
                            std::string foundPath = "";
                            std::string hppPath = "";

                            if (fs::exists(gamePath / "bin" / "hammer.exe")) foundPath = (gamePath / "bin" / "hammer.exe").string();
                            else if (fs::exists(gamePath / "bin" / "win64" / "hammer.exe")) foundPath = (gamePath / "bin" / "win64" / "hammer.exe").string();
                            else if (fs::exists(gamePath / "bin" / "x64" / "hammer.exe")) foundPath = (gamePath / "bin" / "x64" / "hammer.exe").string();

                            if (fs::exists(gamePath / "bin" / "hammerplusplus.exe")) hppPath = (gamePath / "bin" / "hammerplusplus.exe").string();
                            else if (fs::exists(gamePath / "bin" / "x64" / "hammerplusplus.exe")) hppPath = (gamePath / "bin" / "x64" / "hammerplusplus.exe").string();
                            else if (fs::exists(gamePath / "bin" / "win64" / "hammerplusplus.exe")) hppPath = (gamePath / "bin" / "win64" / "hammerplusplus.exe").string();

                            if (foundPath.empty() && hppPath.empty()) {
                                if (fs::exists(gamePath / "game" / "csgo") || fs::exists(gamePath / "game" / "dota") || fs::exists(gamePath / "game" / "hlvr")) {
                                }
                                else { continue; }
                            }

                            GameData data;
                            data.name = wxString::FromUTF8(gameName);
                            data.appId = wxString::FromUTF8(appId);
                            data.gamePath = wxString::FromUTF8(gamePath.string());
                            data.hammerPath = wxString::FromUTF8(foundPath);
                            data.hppPath = wxString::FromUTF8(hppPath);
                            games.push_back(data);

                            wxString strAppId = data.appId;
                            fs::path localSteamRootPath = steamRootPath;
                            std::thread([handler, strAppId, appId, localSteamRootPath]() {
                                std::this_thread::sleep_for(std::chrono::milliseconds(100));

                                fs::create_directories("images_cache");
                                fs::create_directories("images_banner");

                                fs::path cacheFolder = localSteamRootPath / "appcache" / "librarycache" / appId;
                                fs::path iconDest = L"images_cache/" + strAppId.ToStdWstring() + L".jpg";
                                fs::path bannerDest = L"images_banner/" + strAppId.ToStdWstring() + L".jpg";

                                bool iconFound = false;
                                bool bannerFound = false;
                                fs::path heroFile;

                                try {
                                    if (fs::exists(cacheFolder) && fs::is_directory(cacheFolder)) {
                                        for (const auto& entry : fs::directory_iterator(cacheFolder)) {
                                            std::string fn = entry.path().filename().string();

                                            if (!iconFound && entry.path().extension() == ".jpg" &&
                                                fn.length() > 30 && fn.find("_") == std::string::npos) {
                                                fs::copy_file(entry.path(), iconDest, fs::copy_options::overwrite_existing);
                                                iconFound = true;
                                            }

                                            if (!bannerFound) {
                                                if (fn == "library_header.jpg" || fn == "header.jpg") {
                                                    fs::copy_file(entry.path(), bannerDest, fs::copy_options::overwrite_existing);
                                                    bannerFound = true;
                                                }
                                                else if (fn == "library_hero.jpg") {
                                                    heroFile = entry.path();
                                                }
                                            }
                                        }
                                    }

                                    if (!bannerFound && !heroFile.empty()) {
                                        fs::copy_file(heroFile, bannerDest, fs::copy_options::overwrite_existing);
                                        bannerFound = true;
                                    }
                                }
                                catch (...) {}

                                if (!bannerFound) {
                                    std::wstring savePathStr = bannerDest.wstring();
                                    std::wstring baseId = strAppId.ToStdWstring();
                                    const wchar_t* urls[] = {
                                        (L"https://shared.fastly.steamstatic.com/store_item_assets/steam/apps/" + baseId + L"/header.jpg").c_str(),
                                        (L"https://cdn.cloudflare.steamstatic.com/steam/apps/" + baseId + L"/header.jpg").c_str(),
                                        (L"https://steamcdn-a.akamaihd.net/steam/apps/" + baseId + L"/header.jpg").c_str()
                                    };
                                    for (const wchar_t* u : urls) {
                                        if (DownloadImageWinHTTP(u, savePathStr.c_str())) {
                                            break;
                                        }
                                    }
                                }

                                wxThreadEvent* evt = new wxThreadEvent(EVT_IMAGE_LOADED);
                                evt->SetString(strAppId);
                                wxQueueEvent(handler, evt);
                                }).detach();
                        }
                    }
                }
            }
        }
        catch (...) { continue; }
    }

    std::thread([steamRootPath]() {
        fs::create_directories("images_cache");
        fs::path loginUsers = steamRootPath / "config" / "loginusers.vdf";
        if (!fs::exists(loginUsers)) return;

        std::ifstream file(loginUsers);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        size_t pos = 0;
        long long maxTs = -1;
        std::string avatarHash = "";

        while ((pos = content.find("\"7656119", pos)) != std::string::npos) {
            size_t idStart = pos + 1;
            size_t idEnd = content.find("\"", idStart);
            size_t blockEnd = content.find("}", pos);
            if (blockEnd == std::string::npos) blockEnd = content.length();

            std::string block = content.substr(pos, blockEnd - pos);

            long long ts = 0;
            size_t tsPos = block.find("\"Timestamp\"");
            if (tsPos != std::string::npos) {
                size_t q1 = block.find('"', tsPos + 11);
                size_t q2 = block.find('"', q1 + 1);
                if (q1 != std::string::npos && q2 != std::string::npos) {
                    try { ts = std::stoll(block.substr(q1 + 1, q2 - q1 - 1)); }
                    catch (...) {}
                }
            }

            if (ts > maxTs) {
                maxTs = ts;
                size_t avPos = block.find("\"Avatar\"");
                if (avPos != std::string::npos) {
                    size_t q1 = block.find('"', avPos + 8);
                    size_t q2 = block.find('"', q1 + 1);
                    if (q1 != std::string::npos && q2 != std::string::npos) {
                        std::string fullPath = block.substr(q1 + 1, q2 - q1 - 1);
                        avatarHash = fs::path(fullPath).filename().string();
                    }
                }
            }
            pos = idEnd;
        }

        if (!avatarHash.empty()) {
            try {
                fs::path avatarsDir = steamRootPath / "config" / "avatars";
                if (fs::exists(avatarsDir)) {
                    for (const auto& entry : fs::directory_iterator(avatarsDir)) {
                        std::string fn = entry.path().filename().string();
                        if (fn.find(avatarHash) != std::string::npos) {
                            fs::copy_file(entry.path(), "images_cache/avatar.png", fs::copy_options::overwrite_existing);
                            break;
                        }
                    }
                }
            }
            catch (...) {}
        }
        }).detach();
}

void LoadSteamProfile(fs::path steamRootPath, wxString& outName, wxString& outId) {
    fs::path loginUsersPath = steamRootPath / "config" / "loginusers.vdf";
    if (!fs::exists(loginUsersPath)) return;

    std::ifstream file(loginUsersPath);
    std::stringstream buffer;
    buffer << file.rdbuf();
    std::string content = buffer.str();

    size_t pos = 0;
    std::string bestId = "";
    std::string bestName = "";
    long long maxTimestamp = -1;

    while ((pos = content.find("\"7656119", pos)) != std::string::npos) {
        size_t idStart = pos + 1;
        size_t idEnd = content.find("\"", idStart);
        std::string currentId = content.substr(idStart, idEnd - idStart);

        size_t blockEnd = content.find("}", pos);
        if (blockEnd == std::string::npos) blockEnd = content.length();

        std::string block = content.substr(pos, blockEnd - pos);

        long long currentTimestamp = 0;
        size_t tsPos = block.find("\"Timestamp\"");
        if (tsPos != std::string::npos) {
            size_t q1 = block.find('"', tsPos + 11);
            size_t q2 = block.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                std::string tsStr = block.substr(q1 + 1, q2 - q1 - 1);
                try { currentTimestamp = std::stoll(tsStr); }
                catch (...) {}
            }
        }

        std::string currentName = "";
        size_t namePos = block.find("\"PersonaName\"");
        if (namePos != std::string::npos) {
            size_t q1 = block.find('"', namePos + 13);
            size_t q2 = block.find('"', q1 + 1);
            if (q1 != std::string::npos && q2 != std::string::npos) {
                currentName = block.substr(q1 + 1, q2 - q1 - 1);
            }
        }

        if (currentTimestamp > maxTimestamp) {
            maxTimestamp = currentTimestamp;
            bestId = currentId;
            bestName = currentName;
        }

        pos = idEnd;
    }

    if (!bestId.empty()) {
        outName = wxString::FromUTF8(bestName);
        outId = "ID: " + wxString(bestId);
    }
    else {
        outName = "Steam User";
        outId = "ID: Unknown";
    }
}