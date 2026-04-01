#include "xmulti_stream_player.h"

XMultiStreamPlayer::XMultiStreamPlayer(QWidget *parent)
    : QWidget(parent)
{
    ui.setupUi(this);
    
}

XMultiStreamPlayer::~XMultiStreamPlayer()
{}

void XMultiStreamPlayer::AddSourcePushButton()
{
}

void XMultiStreamPlayer::MultiPlayPushButton()
{
    PlayEngineConfig config;
    if (!play_engine_)
    {
        play_engine_ = std::make_unique<XMultiPlayEngine>(config);
    }
    WId win_id = ui.label_Video->winId();
    auto d3d_ctx_ = std::make_shared<D3D11HardwareContext>(reinterpret_cast<HWND>(win_id));
    play_engine_->SetD3DContext(d3d_ctx_);

    for (int i = 1; i < 32; ++i)
    {
        std::string filename = "camera_videos\\test_video_" +
            (i < 10 ? "0" + std::to_string(i) : std::to_string(i)) +
            ".mp4";
        play_engine_->AddStream(i, filename);
    }

    play_engine_->StartAllStreams();
}
