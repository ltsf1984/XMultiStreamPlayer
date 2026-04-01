#include "xmulti_stream_player.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    XMultiStreamPlayer w;
    w.show();
    return a.exec();
}
