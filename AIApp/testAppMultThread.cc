#include <wx/wx.h>
#include <hiredis/hiredis.h>
#include <thread>
#include <iostream>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>

class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};

class MyFrame : public wxFrame
{
public:
    MyFrame();
    ~MyFrame();

private:
    void OnSend(wxCommandEvent& event);
    void OnReceive();
    void ConnectToRedis();
    void ProcessMessages();

    wxTextCtrl* m_display;
    wxTextCtrl* m_input;
    wxButton* m_sendButton;

    redisContext* m_redisContext;
    std::vector<std::thread> m_threadPool;
    std::queue<wxString> m_messageQueue;
    std::mutex m_queueMutex;
    std::condition_variable m_queueCondVar;
    bool m_running;
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    MyFrame* frame = new MyFrame();
    frame->Show(true);
    return true;
}

MyFrame::MyFrame()
    : wxFrame(NULL, wxID_ANY, "Chat Application"), m_running(true)
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    wxFlexGridSizer* gridSizer = new wxFlexGridSizer(2, 2, 5, 50);

    m_display = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    gridSizer->Add(m_display, 1, wxEXPAND | wxALL, 5);

    m_input = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    gridSizer->Add(m_input, 0, wxEXPAND | wxALL, 5);

    m_sendButton = new wxButton(this, wxID_ANY, "Send");
    gridSizer->Add(m_sendButton, 0, wxALIGN_CENTER | wxALL, 5);

    SetSizer(gridSizer);

    Connect(m_sendButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::OnSend));
    Connect(m_input->GetId(), wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(MyFrame::OnSend));

    ConnectToRedis();

    // 创建线程池
    for (int i = 0; i < std::thread::hardware_concurrency(); ++i)
    {
        m_threadPool.emplace_back(&MyFrame::ProcessMessages, this);
    }

    // 启动一个新的线程，用于监听来自 Redis 的消息并显示
    std::thread(&MyFrame::OnReceive, this).detach();
}

MyFrame::~MyFrame()
{
    {
        std::lock_guard<std::mutex> lock(m_queueMutex);
        m_running = false;
    }
    m_queueCondVar.notify_all();

    for (auto& thread : m_threadPool)
    {
        if (thread.joinable())
        {
            thread.join();
        }
    }

    redisFree(m_redisContext);
}

void MyFrame::OnSend(wxCommandEvent& event)
{
    wxString message = m_input->GetValue();
    if (!message.IsEmpty())
    {
        redisCommand(m_redisContext, "PUBLISH chat %s", message.ToStdString().c_str());
        m_input->Clear();
    }
}

void MyFrame::OnReceive()
{
    redisContext* subContext = redisConnect("127.0.0.1", 6379);
    redisCommand(subContext, "SUBSCRIBE chat");

    while (true)
    {
        redisReply* reply;
        if (redisGetReply(subContext, (void**)&reply) == REDIS_OK)
        {
            if (reply->type == REDIS_REPLY_ARRAY && reply->elements == 3)
            {
                wxString message = wxString::FromUTF8(reply->element[2]->str);
                {
                    std::lock_guard<std::mutex> lock(m_queueMutex);
                    m_messageQueue.push(message);
                }
                m_queueCondVar.notify_one();
            }
            freeReplyObject(reply);
        }
    }
}

void MyFrame::ConnectToRedis()
{
    m_redisContext = redisConnect("127.0.0.1", 6379);
    if (m_redisContext == NULL || m_redisContext->err)
    {
        if (m_redisContext)
        {
            std::cerr << "Error: " << m_redisContext->errstr << std::endl;
            redisFree(m_redisContext);
        }
        else
        {
            std::cerr << "Can't allocate redis context" << std::endl;
        }
        exit(1);
    }
}

void MyFrame::ProcessMessages()
{
    while (m_running)
    {
        std::unique_lock<std::mutex> lock(m_queueMutex);
        m_queueCondVar.wait(lock, [this]() { return !m_messageQueue.empty() || !m_running; });

        while (!m_messageQueue.empty())
        {
            wxString message = m_messageQueue.front();
            m_messageQueue.pop();
            lock.unlock();

            CallAfter([this, message]() {
                m_display->AppendText(message + "\n");
            });

            lock.lock();
        }
    }
}