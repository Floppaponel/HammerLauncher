#pragma once
#include <wx/wx.h>
#include <wx/scrolwin.h>
#include <wx/wrapsizer.h>
#include <wx/statbmp.h>
#include <wx/stattext.h>
#include <wx/simplebook.h>
#include <wx/panel.h>
#include <wx/filename.h>
#include <wx/graphics.h>
#include <wx/dcbuffer.h>
#include <wx/grid.h>
#include <filesystem>
#include <map>
#include <vector>

struct GameData {
    wxString name;
    wxString appId;
    wxString gamePath;
    wxString hammerPath;
    wxString hppPath;
};

struct TileInfo {
    wxStaticBitmap* bannerBmp;
    wxStaticBitmap* iconBmp;
    wxStaticText* loadingText;
};

wxDECLARE_EVENT(EVT_IMAGE_LOADED, wxThreadEvent);

wxBitmap LoadAnyImage(const wxString& path);
wxImage FitImageToSize(wxImage img, int targetW, int targetH);

class MyFrame : public wxFrame {
public:
    MyFrame();
    void OnImageLoaded(wxThreadEvent& event);
    void ShowGamePage(const GameData& data);
    void ShowMainPage();

private:
    wxSimplebook* book;
    wxPanel* mainPage;
    wxScrolledWindow* scrollWin;
    wxGridSizer* gridSizer;
    wxPanel* profilePanel;

    wxStaticBitmap* avatarBmp;
    wxStaticText* profileName;
    wxStaticText* profileId;

    wxButton* btnDiscord;
    wxButton* btnOpenSource;

    wxPanel* gamePage;
    wxStaticBitmap* bigArtBmp;
    wxStaticText* gameTitleText;
    wxStaticText* gameIdText;
    wxButton* btnLaunch;
    wxButton* btnHpp;
    wxButton* btnFolder;
    GameData currentGameData;

    std::filesystem::path steamRootPath;
    std::map<wxString, TileInfo> tileMap;

    void ScanGames();
    void LoadProfile();
};

class MyApp : public wxApp {
public:
    virtual bool OnInit() override;
};