#include <wx/wx.h>
#include <hiredis/hiredis.h>
#include <thread>
#include <iostream>

class MyApp : public wxApp
{
public:
    virtual bool OnInit();
};

/**
 * 一个用于显示聊天相关信息的 UI 控件类
 * 
 * 包含一个文本控件，用于显示聊天信息，一个文本控件，用于显示输入消息，一个发送按钮
 * 发送按钮的点击事件和输入框回车事件会触发发送消息的操作
 * 包含接收来自 Redis 的消息并显示的功能    
*/
class MyFrame : public wxFrame
{
public:
    MyFrame();
    ~MyFrame();

private:
    // 处理发送按钮点击事件和输入框回车事件
    void OnSend(wxCommandEvent& event);
    // 接收来自 Redis 的消息并显示
    void OnReceive();
    // 连接到 Redis 服务器，IP:port : 127.0.0.1:6379
    void ConnectToRedis();

    // 文本控件，用于显示聊天信息
    wxTextCtrl* m_display;
    // 文本控件，用于显示输入消息
    wxTextCtrl* m_input;
    // 发送按钮
    wxButton* m_sendButton;

    // Redis 连接上下文 (context)
    redisContext* m_redisContext;
    // 接收消息的线程
    std::thread m_receiveThread;
};

wxIMPLEMENT_APP(MyApp);

bool MyApp::OnInit()
{
    // 创建应用程序窗口
    MyFrame* frame = new MyFrame();
    // 类似 javascript 的 document.getElementById("app").style.display = "block"
    frame->Show(true);
    // 返回代表启动成功的 true
    return true;
}

MyFrame::MyFrame()
    : wxFrame(NULL, wxID_ANY, "Chat Application")
{
    wxBoxSizer* sizer = new wxBoxSizer(wxVERTICAL);
    // wxFlexGridSizer 的四个构造参数：行数、列数、水平间隔、垂直间隔
    wxFlexGridSizer* gridSizer = new wxFlexGridSizer(2, 2, 5, 50);

    // wxTE_MULTILINE 表示多行文本控件，用于显示聊天信息，同时 wxTE_READONLY 表示这个文本控件是只读的
    m_display = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
    gridSizer->Add(m_display, 1, wxEXPAND | wxALL, 5);

    // wxTE_PROCESS_ENTER 表示这个文本控件会在用户按下回车键时触发一个事件，也就是，光标在这个文本控件中时用户按下回车键，就会触发一个事件
    m_input = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxDefaultSize, wxTE_PROCESS_ENTER);
    gridSizer->Add(m_input, 0, wxEXPAND | wxALL, 5);

    m_sendButton = new wxButton(this, wxID_ANY, "Send");
    gridSizer->Add(m_sendButton, 0, wxALIGN_CENTER | wxALL, 5);

    SetSizer(gridSizer);

    // 给控件绑定时间对应的处理函数
    Connect(m_sendButton->GetId(), wxEVT_COMMAND_BUTTON_CLICKED, wxCommandEventHandler(MyFrame::OnSend));
    Connect(m_input->GetId(), wxEVT_COMMAND_TEXT_ENTER, wxCommandEventHandler(MyFrame::OnSend));

    // 把窗口连接到 Redis 服务器，让前端能够与后端数据库进行通信
    ConnectToRedis();
    // 启动一个新的线程，用于监听来自 Redis 的消息并显示
    m_receiveThread = std::thread(&MyFrame::OnReceive, this);
}

MyFrame::~MyFrame()
{
    if (m_receiveThread.joinable())
    {
        m_receiveThread.join();
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
                CallAfter([this, message]() {
                    m_display->AppendText(message + "\n");
                });
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