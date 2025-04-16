#include <wx/wx.h>

class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};

class MyFrame : public wxFrame
{
public:
    MyFrame();
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}

MyFrame::MyFrame()
    : wxFrame(NULL, wxID_ANY, "Example Application")
{
    wxGridSizer* gridSizer = new wxGridSizer(2, 2, 5, 5);

    wxTextCtrl* m_display = new wxTextCtrl(this, wxID_ANY, "Discussion table", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

    gridSizer->Add(m_display, 1, wxEXPAND | wxALL, 5);

    wxTextCtrl* m_input = new wxTextCtro(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    gridSizer->Add(m_input, 0, wxEXPAND | wxALL, 5);

    wxButton* m_sendButton = new wxButton(this, wxID_ANY, "Send");
    gridSizer->Add(m_sendButton, 0, wxALIGN_CENTER | wxALL, 5);

    wxButton* m_cancelButton = new wxButton(this, wxID_ANY, "Cancel");
    gridSizer->Add(m_cancelButton, 0, wxALIGN_CENTER | wxALL, 5);

    SetSizer(gridSizer);
}