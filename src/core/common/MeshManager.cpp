#include "MeshManager.h"
#include <QCoreApplication>
#include <QFile>
#include <QDebug>
#include <QDir>

MeshManager::MeshManager(QObject* parent) : QObject(parent) {
    m_process = new QProcess(this);
    // 连接进程结束信号
    connect(m_process, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished),
        this, &MeshManager::onGmshFinished);
}

void MeshManager::buildAndLoad(const MeshGenerator& generator, const QString& entityName) {
    // 1. 获取程序运行目录，确定临时文件路径
    QString appPath = QCoreApplication::applicationDirPath();
    
    QString geoPath = appPath + "/" + entityName + ".geo";
    m_tempMshPath = appPath + "/" + entityName + ".msh";

    // 2. 从生成器获取脚本字符串并写入文件
    QString script = generator.generateGeoScript();
    QFile file(geoPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        emit errorOccurred("无法创建临时 .geo 文件");
        return;
    }
    file.write(script.toUtf8());
    file.close();

    // 3. 配置 QProcess 调用 Gmsh.exe
    // 参数说明: -3(生成3D网格), -o(输出路径), -format msh2(兼容目前的解析器)
    QString program = appPath + "/gmsh.exe";
    QStringList arguments;
    arguments << geoPath << "-3" << "-o" << m_tempMshPath << "-format" << "msh2";

    if (m_process->state() != QProcess::NotRunning) {
        m_process->kill(); // 如果上一次任务没完，先停掉
    }

    m_process->start(program, arguments);
    qDebug() << "Gmsh 已启动，正在生成: " << generator.getTypeName();
}

// MeshManager.cpp 修复后的逻辑

void MeshManager::onGmshFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    if (exitCode == 0 && exitStatus == QProcess::NormalExit) {
        qDebug() << "Gmsh 执行成功";
        emit meshReady(m_tempMshPath);
    }
    else {
        // 读取标准错误输出
        QString errorDetail = QString::fromUtf8(m_process->readAllStandardError());

        // 报错位置修复：使用 QString 的拼接方式，而不是 <<
        QString errorMsg = "Gmsh 运行出错: " + errorDetail;

        qDebug() << errorMsg; // 在调试流中使用 << 是没问题的
        emit errorOccurred(errorMsg); // 信号发射直接传 QString
    }
}