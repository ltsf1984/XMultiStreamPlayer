#pragma once

#include <memory>
#include <QtWidgets/QWidget>
#include "ui_xmulti_stream_player.h"
#include "xmulti_play_engine.h"

class XMultiStreamPlayer : public QWidget
{
    Q_OBJECT

public:
    XMultiStreamPlayer(QWidget *parent = nullptr);
    ~XMultiStreamPlayer();

public slots:
    void AddSourcePushButton();
    void MultiPlayPushButton();

private:
    Ui::XMultiStreamPlayerClass ui;

private:
    std::unique_ptr<XMultiPlayEngine> play_engine_;
};
