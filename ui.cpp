#include "app.h"
#include <windows.h>
#include <shellapi.h>
#include <functional>
#include <wrl/client.h>
#include <cstdlib>
#include <wx/hyperlink.h>
#include <wx/listctrl.h>
#include <wincodec.h>

#pragma comment(lib, "shell32.lib")
#pragma comment(lib, "windowscodecs.lib")

extern void LoadSteamGames(wxEvtHandler* handler, std::vector<GameData>& games, std::filesystem::path& steamRootPath);
extern void LoadSteamProfile(std::filesystem::path steamRootPath, wxString& outName, wxString& outId);

wxBitmap LoadAnyImage(const wxString& path) {
    using namespace Microsoft::WRL;
    ComPtr<IWICImagingFactory> factory;
    CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER, IID_PPV_ARGS(&factory));
    if (!factory) return wxNullBitmap;

    ComPtr<IWICBitmapDecoder> decoder;
    factory->CreateDecoderFromFilename(path.ToStdWstring().c_str(), nullptr, GENERIC_READ, WICDecodeMetadataCacheOnDemand, &decoder);
    if (!decoder) return wxNullBitmap;

    ComPtr<IWICBitmapFrameDecode> frame;
    decoder->GetFrame(0, &frame);
    if (!frame) return wxNullBitmap;

    ComPtr<IWICFormatConverter> converter;
    factory->CreateFormatConverter(&converter);
    converter->Initialize(frame.Get(), GUID_WICPixelFormat32bppRGBA, WICBitmapDitherTypeNone, nullptr, 0.0, WICBitmapPaletteTypeCustom);

    UINT width, height;
    converter->GetSize(&width, &height);
    std::vector<BYTE> buffer(width * height * 4);
    converter->CopyPixels(nullptr, width * 4, buffer.size(), buffer.data());

    wxImage img(width, height);
    unsigned char* rgb = img.GetData();
    unsigned char* alpha = img.GetAlpha();
    if (!alpha) { img.InitAlpha(); alpha = img.GetAlpha(); }

    for (UINT i = 0; i < width * height; ++i) {
        rgb[i * 3 + 0] = buffer[i * 4 + 0];
        rgb[i * 3 + 1] = buffer[i * 4 + 1];
        rgb[i * 3 + 2] = buffer[i * 4 + 2];
        alpha[i] = buffer[i * 4 + 3];
    }
    return wxBitmap(img);
}

wxImage FitImageToSize(wxImage img, int targetW, int targetH) {
    if (!img.IsOk()) return img;
    double imgRatio = (double)img.GetWidth() / img.GetHeight();
    double targetRatio = (double)targetW / targetH;

    int newW, newH;
    if (imgRatio > targetRatio) {
        newW = targetW;
        newH = (int)(targetW / imgRatio);
    }
    else {
        newH = targetH;
        newW = (int)(targetH * imgRatio);
    }

    img.Rescale(newW, newH, wxIMAGE_QUALITY_HIGH);

    wxImage canvas(targetW, targetH);
    canvas.SetRGB(wxRect(0, 0, targetW, targetH), 30, 30, 35);

    int x = (targetW - newW) / 2;
    int y = (targetH - newH) / 2;
    canvas.Paste(img, x, y);

    return canvas;
}

void StyleButton(wxButton* btn) {
    btn->SetBackgroundColour(wxColour(30, 30, 40));
    btn->SetForegroundColour(*wxWHITE);
    btn->SetFont(wxFont(10, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
    btn->Bind(wxEVT_ENTER_WINDOW, [btn](wxMouseEvent& e) { btn->SetBackgroundColour(wxColour(23, 105, 163)); btn->Refresh(); e.Skip(); });
    btn->Bind(wxEVT_LEAVE_WINDOW, [btn](wxMouseEvent& e) { btn->SetBackgroundColour(wxColour(30, 30, 40)); btn->Refresh(); e.Skip(); });
}

class HppInfoDialog : public wxDialog {
public:
    HppInfoDialog(wxWindow* parent, const GameData& data)
        : wxDialog(parent, wxID_ANY, "Hammer++ Installation Guide", wxDefaultPosition, wxSize(750, 700), wxDEFAULT_DIALOG_STYLE) {

        SetBackgroundColour(wxColour(25, 25, 25));
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

        wxStaticText* title = new wxStaticText(this, wxID_ANY, "Hammer++ Installation");
        wxFont titleFont = title->GetFont();
        titleFont.SetPointSize(20); titleFont.SetWeight(wxFONTWEIGHT_BOLD);
        title->SetFont(titleFont);
        title->SetForegroundColour(*wxWHITE);
        mainSizer->Add(title, 0, wxALL, 15);

        wxStaticText* authorText = new wxStaticText(this, wxID_ANY, "Author: ficool2");
        authorText->SetForegroundColour(wxColour(200, 200, 200));
        mainSizer->Add(authorText, 0, wxLEFT | wxRIGHT, 15);

        wxHyperlinkCtrl* link = new wxHyperlinkCtrl(this, wxID_ANY, "Open Official Download Page", "https://ficool2.github.io/HammerPlusPlus-Website/");
        link->SetNormalColour(wxColour(100, 180, 255));
        mainSizer->Add(link, 0, wxALL, 15);

        wxStaticText* tableTitle = new wxStaticText(this, wxID_ANY, "Supported Games:");
        tableTitle->SetForegroundColour(*wxWHITE);
        tableTitle->SetFont(wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        mainSizer->Add(tableTitle, 0, wxLEFT | wxRIGHT | wxTOP, 15);

        wxListCtrl* list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxSize(-1, 220), wxLC_REPORT | wxBORDER_NONE);
        list->SetBackgroundColour(wxColour(35, 35, 35));
        list->SetTextColour(*wxWHITE);
        list->InsertColumn(0, "Game", wxLIST_FORMAT_LEFT, 250);
        list->InsertColumn(1, "Install to Folder", wxLIST_FORMAT_LEFT, 150);
        list->InsertColumn(2, "Notes", wxLIST_FORMAT_LEFT, 300);

        auto addRow = [&](const wxString& game, const wxString& folder, const wxString& notes) {
            long idx = list->InsertItem(list->GetItemCount(), game);
            list->SetItem(idx, 1, folder);
            list->SetItem(idx, 2, notes);
            };

        addRow("Garry's Mod", "bin/x64", "Requires x86-64 beta branch");
        addRow("Team Fortress 2", "bin", "Standard 32-bit");
        addRow("Portal 2", "bin", "Standard 32-bit");
        addRow("Left 4 Dead 2", "bin", "Standard 32-bit");
        addRow("Counter-Strike: Source", "bin", "Standard 32-bit");
        addRow("Day of Defeat: Source", "bin", "Standard 32-bit");
        addRow("Half-Life 2", "bin", "Standard 32-bit");
        addRow("Source SDK 2013 Singleplayer", "bin", "Standard 32-bit");
        addRow("Source SDK 2013 Multiplayer", "bin", "Standard 32-bit");

        list->Bind(wxEVT_LIST_ITEM_SELECTED, [list](wxListEvent& e) {
            list->SetItemState(e.GetIndex(), 0, wxLIST_STATE_SELECTED);
            });

        mainSizer->Add(list, 1, wxEXPAND | wxALL, 15);

        wxString titleStr = "How to install for " + data.name + ":";
        wxStaticText* stepTitle = new wxStaticText(this, wxID_ANY, titleStr);
        stepTitle->SetForegroundColour(*wxWHITE);
        stepTitle->SetFont(wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        mainSizer->Add(stepTitle, 0, wxLEFT | wxRIGHT | wxTOP, 15);

        wxString instructions =
            "1. Download the specific version for your game from the website.\n"
            "2. Extract the .zip contents into this folder:\n\n" +
            data.gamePath + "\\bin\n\n" +
            "3. IMPORTANT: If the table says 'bin/x64' (like for GMod), put files in the 'bin/x64' folder!\n"
            "4. Launch Hammer++ using this launcher.";

        wxStaticText* text = new wxStaticText(this, wxID_ANY, instructions);
        text->SetForegroundColour(wxColour(200, 200, 200));
        mainSizer->Add(text, 0, wxALL, 15);

        wxBoxSizer* btnSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* btnOpenFolder = new wxButton(this, wxID_ANY, "Open Game Folder");
        StyleButton(btnOpenFolder);

        wxButton* btnClose = new wxButton(this, wxID_OK, "Close");
        StyleButton(btnClose);

        btnSizer->Add(btnOpenFolder, 0, wxALL, 10);
        btnSizer->AddStretchSpacer();
        btnSizer->Add(btnClose, 0, wxALL, 10);

        mainSizer->Add(btnSizer, 0, wxEXPAND | wxALL, 10);

        SetSizer(mainSizer);

        btnOpenFolder->Bind(wxEVT_BUTTON, [data](wxCommandEvent&) {
            std::wstring cmd = L"explorer.exe \"" + data.gamePath.ToStdWstring() + L"\\bin\"";
            _wsystem(cmd.c_str());
            });
    }
};

MyFrame::MyFrame() : wxFrame(nullptr, wxID_ANY, "Hammer Launcher", wxDefaultPosition, wxSize(1100, 750)) {
    SetBackgroundColour(wxColour(15, 15, 15));
    wxIcon appIcon;
    if (appIcon.LoadFile("app.ico", wxBITMAP_TYPE_ICO)) {
        SetIcon(appIcon);
    }
    book = new wxSimplebook(this);

    mainPage = new wxPanel(book);
    mainPage->SetBackgroundColour(wxColour(15, 15, 15));
    wxBoxSizer* mainPageSizer = new wxBoxSizer(wxVERTICAL);

    profilePanel = new wxPanel(mainPage, wxID_ANY, wxDefaultPosition, wxSize(-1, 80));
    profilePanel->SetBackgroundColour(wxColour(25, 25, 25));
    wxBoxSizer* profileSizer = new wxBoxSizer(wxHORIZONTAL);

    avatarBmp = new wxStaticBitmap(profilePanel, wxID_ANY, wxBitmap(64, 64), wxDefaultPosition, wxSize(64, 64));

    wxBoxSizer* textSizer = new wxBoxSizer(wxVERTICAL);
    profileName = new wxStaticText(profilePanel, wxID_ANY, "Loading...");
    profileName->SetForegroundColour(*wxWHITE);
    wxFont font = profileName->GetFont();
    font.SetPointSize(14); font.SetWeight(wxFONTWEIGHT_BOLD);
    profileName->SetFont(font);

    profileId = new wxStaticText(profilePanel, wxID_ANY, "ID: ...");
    profileId->SetForegroundColour(wxColour(150, 150, 150));

    textSizer->Add(profileName, 0, wxLEFT | wxTOP, 5);
    textSizer->Add(profileId, 0, wxLEFT, 5);

    profileSizer->Add(avatarBmp, 0, wxALL | wxALIGN_CENTER_VERTICAL, 8);
    profileSizer->Add(textSizer, 0, wxALIGN_CENTER_VERTICAL);

    profileSizer->AddStretchSpacer();

    btnDiscord = new wxButton(profilePanel, wxID_ANY, "Discord", wxDefaultPosition, wxSize(80, 30));
    btnOpenSource = new wxButton(profilePanel, wxID_ANY, "Source Code", wxDefaultPosition, wxSize(100, 30));

    auto styleLinkButton = [](wxButton* btn) {
        btn->SetBackgroundColour(wxColour(25, 25, 25));
        btn->SetForegroundColour(wxColour(150, 150, 150));
        btn->SetFont(wxFont(9, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_NORMAL));
        btn->Bind(wxEVT_ENTER_WINDOW, [btn](wxMouseEvent& e) { btn->SetForegroundColour(*wxWHITE); btn->Refresh(); e.Skip(); });
        btn->Bind(wxEVT_LEAVE_WINDOW, [btn](wxMouseEvent& e) { btn->SetForegroundColour(wxColour(150, 150, 150)); btn->Refresh(); e.Skip(); });
        };

    styleLinkButton(btnDiscord);
    styleLinkButton(btnOpenSource);

    btnDiscord->Bind(wxEVT_BUTTON, [](wxCommandEvent&) {
        ShellExecuteW(nullptr, L"open", L"https://discord.gg/your-invite", nullptr, nullptr, SW_SHOWNORMAL);
        });
    btnOpenSource->Bind(wxEVT_BUTTON, [](wxCommandEvent&) {
        ShellExecuteW(nullptr, L"open", L"https://github.com/Floppaponel/HammerLauncher", nullptr, nullptr, SW_SHOWNORMAL);
        });

    profileSizer->Add(btnDiscord, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
    profileSizer->Add(btnOpenSource, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 15);

    profilePanel->SetSizer(profileSizer);

    scrollWin = new wxScrolledWindow(mainPage, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxVSCROLL | wxALWAYS_SHOW_SB);
    scrollWin->SetBackgroundColour(wxColour(15, 15, 15));
    scrollWin->SetScrollRate(0, 20);

    gridSizer = new wxGridSizer(0, 3, 10, 10);
    scrollWin->SetSizer(gridSizer);

    mainPageSizer->Add(profilePanel, 0, wxEXPAND | wxALL, 10);
    mainPageSizer->Add(scrollWin, 1, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);
    mainPage->SetSizer(mainPageSizer);

    gamePage = new wxPanel(book);
    gamePage->SetBackgroundColour(wxColour(15, 15, 15));
    wxBoxSizer* gamePageSizer = new wxBoxSizer(wxVERTICAL);

    wxPanel* bannerPanel = new wxPanel(gamePage, wxID_ANY, wxDefaultPosition, wxSize(-1, 350));
    bannerPanel->SetBackgroundColour(wxColour(20, 20, 20));

    bigArtBmp = new wxStaticBitmap(bannerPanel, wxID_ANY, wxBitmap(1, 1), wxPoint(0, 0), wxSize(1, 1));

    wxButton* btnBack = new wxButton(bannerPanel, wxID_ANY, "< Back", wxPoint(20, 20), wxSize(100, 40));
    StyleButton(btnBack);
    btnBack->Raise();

    wxPanel* contentPanel = new wxPanel(gamePage, wxID_ANY);
    contentPanel->SetBackgroundColour(wxColour(15, 15, 15));
    wxBoxSizer* contentSizer = new wxBoxSizer(wxVERTICAL);

    gameTitleText = new wxStaticText(contentPanel, wxID_ANY, "Title");
    wxFont titleFont = gameTitleText->GetFont();
    titleFont.SetPointSize(36); titleFont.SetWeight(wxFONTWEIGHT_BOLD);
    gameTitleText->SetFont(titleFont);
    gameTitleText->SetForegroundColour(*wxWHITE);

    gameIdText = new wxStaticText(contentPanel, wxID_ANY, "AppID: 0");
    gameIdText->SetForegroundColour(wxColour(150, 150, 150));

    wxBoxSizer* btnRow = new wxBoxSizer(wxHORIZONTAL);

    btnLaunch = new wxButton(contentPanel, wxID_ANY, "Launch Hammer", wxDefaultPosition, wxSize(180, 45));
    btnHpp = new wxButton(contentPanel, wxID_ANY, "Download Hammer++", wxDefaultPosition, wxSize(180, 45));
    btnFolder = new wxButton(contentPanel, wxID_ANY, "Open Folder", wxDefaultPosition, wxSize(180, 45));

    StyleButton(btnLaunch);
    StyleButton(btnHpp);
    StyleButton(btnFolder);

    btnRow->Add(btnLaunch, 0, wxRIGHT, 15);
    btnRow->Add(btnHpp, 0, wxRIGHT, 15);
    btnRow->Add(btnFolder, 0);

    contentSizer->Add(gameTitleText, 0, wxLEFT | wxTOP, 20);
    contentSizer->Add(gameIdText, 0, wxLEFT | wxTOP | wxBOTTOM, 20);
    contentSizer->Add(btnRow, 0, wxLEFT | wxBOTTOM, 20);
    contentPanel->SetSizer(contentSizer);

    gamePageSizer->Add(bannerPanel, 0, wxEXPAND);
    gamePageSizer->Add(contentPanel, 1, wxEXPAND);
    gamePage->SetSizer(gamePageSizer);

    book->AddPage(mainPage, "Main");
    book->AddPage(gamePage, "Game");

    btnBack->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { ShowMainPage(); });
    btnFolder->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        std::wstring cmd = L"explorer.exe \"" + currentGameData.gamePath.ToStdWstring() + L"\"";
        _wsystem(cmd.c_str());
        });
    btnLaunch->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (currentGameData.hammerPath.IsEmpty()) {
            wxString url = "steam://run/" + currentGameData.appId + "//-tools";
            ShellExecuteW(nullptr, L"open", url.ToStdWstring().c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        }
        else {
            wxString exeDir = wxFileName(currentGameData.hammerPath).GetPath();
            ShellExecuteW(nullptr, L"open", currentGameData.hammerPath.ToStdWstring().c_str(), nullptr, exeDir.ToStdWstring().c_str(), SW_SHOWNORMAL);
        }
        });
    btnHpp->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) {
        if (!currentGameData.hppPath.IsEmpty()) {
            wxString exeDir = wxFileName(currentGameData.hppPath).GetPath();
            ShellExecuteW(nullptr, L"open", currentGameData.hppPath.ToStdWstring().c_str(), nullptr, exeDir.ToStdWstring().c_str(), SW_SHOWNORMAL);
        }
        else {
            HppInfoDialog dlg(this, currentGameData);
            dlg.ShowModal();
        }
        });

    gamePage->Bind(wxEVT_SIZE, [this, bannerPanel](wxSizeEvent& event) {
        int newWidth = gamePage->GetSize().GetWidth();
        int newHeight = 350;
        if (bigArtBmp->IsShown()) {
            wxString bannerPath = "images_banner/" + currentGameData.appId + ".jpg";
            wxBitmap bannerBmp = LoadAnyImage(bannerPath);
            if (bannerBmp.IsOk()) {
                wxImage img = FitImageToSize(bannerBmp.ConvertToImage(), newWidth, newHeight);
                bigArtBmp->SetBitmap(wxBitmap(img));
                bigArtBmp->SetSize(newWidth, newHeight);
            }
        }
        event.Skip();
        });

    Bind(EVT_IMAGE_LOADED, &MyFrame::OnImageLoaded, this);
    ScanGames();
}

void MyFrame::LoadProfile() {
    wxString steamName, steamId;
    LoadSteamProfile(steamRootPath, steamName, steamId);

    if (profileName) profileName->SetLabel(steamName.IsEmpty() ? wxString("Steam User") : steamName);
    if (profileId) profileId->SetLabel(steamId.IsEmpty() ? wxString("ID: Unknown") : steamId);

    if (std::filesystem::exists("images_cache/avatar.png")) {
        wxBitmap av = LoadAnyImage("images_cache/avatar.png");
        if (av.IsOk()) {
            wxImage img = av.ConvertToImage();
            img.Rescale(64, 64, wxIMAGE_QUALITY_HIGH);
            if (avatarBmp) avatarBmp->SetBitmap(wxBitmap(img));
        }
    }

    profilePanel->Layout();
}

void MyFrame::ScanGames() {
    CoInitializeEx(NULL, COINIT_APARTMENTTHREADED);

    std::vector<GameData> games;
    LoadSteamGames(this, games, steamRootPath);
    LoadProfile();

    gridSizer->Clear(true);

    for (const auto& data : games) {
        wxPanel* tilePanel = new wxPanel(scrollWin, wxID_ANY, wxDefaultPosition, wxSize(320, 200));
        tilePanel->SetBackgroundColour(wxColour(30, 30, 35));

        wxBoxSizer* mainTileSizer = new wxBoxSizer(wxVERTICAL);

        wxStaticBitmap* bannerBmp = new wxStaticBitmap(tilePanel, wxID_ANY, wxBitmap(320, 130));
        mainTileSizer->Add(bannerBmp, 0, wxEXPAND);

        wxPanel* bottomPanel = new wxPanel(tilePanel, wxID_ANY, wxDefaultPosition, wxSize(320, 70));
        bottomPanel->SetBackgroundColour(wxColour(20, 20, 20));
        wxBoxSizer* bottomSizer = new wxBoxSizer(wxHORIZONTAL);

        wxStaticBitmap* iconBmp = new wxStaticBitmap(bottomPanel, wxID_ANY, wxBitmap(32, 32), wxDefaultPosition, wxSize(32, 32));

        wxStaticText* nameText = new wxStaticText(bottomPanel, wxID_ANY, data.name);
        nameText->SetForegroundColour(*wxWHITE);
        wxFont nameFont = nameText->GetFont();
        nameFont.SetWeight(wxFONTWEIGHT_BOLD);
        nameFont.SetPointSize(11);
        nameText->SetFont(nameFont);
        nameText->Wrap(220);

        bottomSizer->Add(iconBmp, 0, wxALL | wxALIGN_CENTER_VERTICAL, 12);
        bottomSizer->Add(nameText, 1, wxALL | wxALIGN_CENTER_VERTICAL, 8);
        bottomPanel->SetSizer(bottomSizer);

        mainTileSizer->Add(bottomPanel, 1, wxEXPAND);
        tilePanel->SetSizer(mainTileSizer);

        GameData* gameDataPtr = new GameData(data);
        tilePanel->SetClientData((void*)gameDataPtr);

        auto bindClick = [this, tilePanel](wxWindow* win) {
            win->Bind(wxEVT_LEFT_DOWN, [this, tilePanel](wxMouseEvent&) {
                GameData* gd = (GameData*)tilePanel->GetClientData();
                ShowGamePage(*gd);
                });
            };
        bindClick(tilePanel);
        bindClick(bannerBmp);
        bindClick(bottomPanel);
        bindClick(iconBmp);
        bindClick(nameText);

        gridSizer->Add(tilePanel, 0, wxALL, 10);
        tileMap[data.appId] = { bannerBmp, iconBmp, nullptr };

        wxString bannerPath = "images_banner/" + data.appId + ".jpg";
        if (std::filesystem::exists(bannerPath.ToStdString())) {
            wxBitmap loaded = LoadAnyImage(bannerPath);
            if (loaded.IsOk()) {
                wxImage img = FitImageToSize(loaded.ConvertToImage(), 320, 130);
                bannerBmp->SetBitmap(wxBitmap(img));
            }
        }
        else {
            wxImage img(320, 130);
            img.SetRGB(wxRect(0, 0, 320, 130), 40, 40, 40);
            bannerBmp->SetBitmap(wxBitmap(img));
        }

        wxString iconPath = "images_cache/" + data.appId + ".jpg";
        if (std::filesystem::exists(iconPath.ToStdString())) {
            wxBitmap loaded = LoadAnyImage(iconPath);
            if (loaded.IsOk()) {
                wxImage img = loaded.ConvertToImage();
                img.Rescale(32, 32, wxIMAGE_QUALITY_HIGH);
                iconBmp->SetBitmap(wxBitmap(img));
            }
        }
    }

    gridSizer->Layout();
    wxSize minSize = gridSizer->ComputeFittingClientSize(scrollWin);
    scrollWin->SetVirtualSize(minSize.x, minSize.y);
    scrollWin->FitInside();
}

void MyFrame::ShowGamePage(const GameData& data) {
    currentGameData = data;
    gameTitleText->SetLabel(data.name);
    gameIdText->SetLabel("AppID: " + data.appId);

    if (!data.hammerPath.IsEmpty()) {
        btnLaunch->SetLabel("Launch Hammer");
        btnLaunch->Enable();
    }
    else if (data.appId == "730" || data.appId == "570" || data.appId == "546560") {
        btnLaunch->SetLabel("Launch Workshop Tools");
        btnLaunch->Enable();
    }
    else {
        btnLaunch->SetLabel("Hammer Not Found");
        btnLaunch->Disable();
    }

    if (!data.hppPath.IsEmpty()) {
        btnHpp->SetLabel("Open Hammer++");
    }
    else {
        btnHpp->SetLabel("Download Hammer++");
    }

    wxString bannerPath = "images_banner/" + data.appId + ".jpg";
    wxWindow* bannerPanel = bigArtBmp->GetParent();

    if (std::filesystem::exists(bannerPath.ToStdString())) {
        wxBitmap bannerBmp = LoadAnyImage(bannerPath);
        if (bannerBmp.IsOk()) {
            wxImage img = FitImageToSize(bannerBmp.ConvertToImage(), bannerPanel->GetSize().GetWidth(), 350);
            bigArtBmp->SetBitmap(wxBitmap(img));
            bigArtBmp->SetSize(bannerPanel->GetSize().GetWidth(), 350);
            bigArtBmp->Show();
        }
    }
    else {
        bigArtBmp->Hide();
    }

    gamePage->Layout();
    book->SetSelection(1);
}

void MyFrame::ShowMainPage() {
    book->SetSelection(0);
}

void MyFrame::OnImageLoaded(wxThreadEvent& event) {
    wxString appId = event.GetString();

    wxString iconPath = "images_cache/" + appId + ".jpg";
    wxBitmap loadedIcon = LoadAnyImage(iconPath);
    if (loadedIcon.IsOk()) {
        wxImage img = loadedIcon.ConvertToImage();
        img.Rescale(32, 32, wxIMAGE_QUALITY_HIGH);
        if (tileMap.count(appId)) {
            tileMap[appId].iconBmp->SetBitmap(wxBitmap(img));
        }
    }

    wxString bannerPath = "images_banner/" + appId + ".jpg";
    wxBitmap loadedBanner = LoadAnyImage(bannerPath);
    if (loadedBanner.IsOk()) {
        wxImage img = FitImageToSize(loadedBanner.ConvertToImage(), 320, 130);
        if (tileMap.count(appId)) {
            tileMap[appId].bannerBmp->SetBitmap(wxBitmap(img));
        }
    }

    scrollWin->Layout();
    wxSize minSize = scrollWin->GetSizer()->ComputeFittingClientSize(scrollWin);
    scrollWin->SetVirtualSize(minSize.x, minSize.y);
    scrollWin->FitInside();
}