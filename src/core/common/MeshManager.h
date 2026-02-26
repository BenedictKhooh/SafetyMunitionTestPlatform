#ifndef MESHMANAGER_H
#define MESHMANAGER_H

#include <QObject>
#include <QProcess>
#include <QString>
#include "MeshGenerator.h"

/**
 * @brief MeshManager 负责管理从脚本生成到 Gmsh 调用的完整生命周期
 */
class MeshManager : public QObject {
    Q_OBJECT
public:
    explicit MeshManager(QObject* parent = nullptr);

    // 核心接口：传入任何一种生成器，开始构建流程
    void buildAndLoad(const MeshGenerator& generator, const QString& entityName);


signals:
    // 当 Gmsh 处理完成且输出文件准备好时发送此信号
    void meshReady(const QString& mshFilePath);
    // 如果执行出错时发送此信号
    void errorOccurred(const QString& errorMsg);

private slots:
    // 内部槽函数：处理 QProcess 结束
    void onGmshFinished(int exitCode, QProcess::ExitStatus exitStatus);

private:
    QProcess* m_process;
    QString m_tempMshPath;
};

#endif // MESHMANAGER_H