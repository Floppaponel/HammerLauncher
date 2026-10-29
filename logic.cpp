/*
 * Hammer Launcher
 * Copyright (c) 2026 Floppaponel
 * Licensed under the MIT License.
 */

#include "app.h"
#include <windows.h>
#include <fstream>
#include <thread>
#include <sstream>

namespace fs = std::filesystem;

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
            if (fs::exists(p) && fs::is_directory(p)) {
                bool alreadyExists = false;
                for (const auto& existing : steamappsPaths) {
                    if (fs::equivalent(p, existing)) { alreadyExists = true; break; }
                }
                if (!alreadyExists) steamappsPaths.push_back(p);
            }
        }
    }

    for (const auto& steamappsPath : steamappsPaths) {
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
                            else {
                                continue;
                            }
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
                            fs::create_directories("images_cache");
                            fs::create_directories("images_banner");

                            fs::path cacheFolder = localSteamRootPath / "appcache" / "librarycache" / appId;
                            fs::path iconDest = L"images_cache/" + strAppId.ToStdWstring() + L".jpg";
                            fs::path bannerDest = L"images_banner/" + strAppId.ToStdWstring() + L".jpg";

                            bool iconFound = false;
                            bool bannerFound = false;

                            if (fs::exists(cacheFolder) && fs::is_directory(cacheFolder)) {
                                for (const auto& entry : fs::directory_iterator(cacheFolder)) {
                                    std::string fn = entry.path().filename().string();
                                    if (!iconFound && entry.path().extension() == ".jpg" &&
                                        fn.length() > 30 && fn.find("_") == std::string::npos) {
                                        fs::copy_file(entry.path(), iconDest, fs::copy_options::overwrite_existing);
                                        iconFound = true;
                                    }
                                    if (!bannerFound) {
                                        if (fn == "library_header.jpg") {
                                            fs::copy_file(entry.path(), bannerDest, fs::copy_options::overwrite_existing);
                                            bannerFound = true;
                                        }
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

    std::thread([steamRootPath]() {
        fs::create_directories("images_cache");
        fs::path loginUsers = steamRootPath / "config" / "loginusers.vdf";
        if (!fs::exists(loginUsers)) return;

        std::ifstream file(loginUsers);
        std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());

        size_t pos = 0;
        long long maxTs = -1;
        std::string avatarPath = "";

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
                        avatarPath = block.substr(q1 + 1, q2 - q1 - 1);
                    }
                }
            }
            pos = idEnd;
        }

        if (!avatarPath.empty()) {
            fs::path fullAvatarPath = steamRootPath / avatarPath;
            if (fs::exists(fullAvatarPath)) {
                fs::copy_file(fullAvatarPath, "images_cache/avatar.png", fs::copy_options::overwrite_existing);
            }
            else {
                fs::path avatarsDir = steamRootPath / "config" / "avatars";
                if (fs::exists(avatarsDir)) {
                    std::string filename = fs::path(avatarPath).filename().string();
                    for (const auto& entry : fs::directory_iterator(avatarsDir)) {
                        if (entry.path().filename().string().find(filename) != std::string::npos) {
                            fs::copy_file(entry.path(), "images_cache/avatar.png", fs::copy_options::overwrite_existing);
                            break;
                        }
                    }
                }
            }
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