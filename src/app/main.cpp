#include "mainwindow.h"

#include <QApplication>
#include <iostream>

int main(int argc, char *argv[])
{
    // 强制输出位数
    if (sizeof(void*) == 8) {
        MessageBoxA(NULL, "这是 64 位程序 (x64)", "位数确认", MB_OK);
    }
    else {
        MessageBoxA(NULL, "这是 32 位程序 (x86)", "位数确认", MB_OK);
    }

    // 加上这句，程序运行到这会停住，等你按回车
    std::system("pause");    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
