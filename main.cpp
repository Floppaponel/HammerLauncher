/*
 * Hammer Launcher
 * Copyright (c) 2026 Floppaponel
 * Licensed under the MIT License.
 */

#include "app.h"

wxIMPLEMENT_APP(MyApp);
wxDEFINE_EVENT(EVT_IMAGE_LOADED, wxThreadEvent);

bool MyApp::OnInit() {
    wxLog::EnableLogging(false);
    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}