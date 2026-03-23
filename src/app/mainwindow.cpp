#include <QMainWindow>
#include "mainwindow.h"
#include "gmsh.h"
#include "glwidget.h"
#include <QPushButton>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QDebug>
#include <QFileInfo>
#include "MeshUtils.h"

#include <QFile>
#include <QTextStream>
#include <QDir>
#include <QMessageBox>

//JSON file processors
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QCoreApplication>
//

#include <QMenu>
#include <QDialog>
#include <QTextEdit>

MainWindow::MainWindow(QWidget* parent) : QMainWindow(parent) {
    // ==========================================
    // 引入多工作区
    // ==========================================
    mainModeTab = new QTabWidget(this);
    setCentralWidget(mainModeTab); // 让 TabWidget 成为真正的中心件

    mainModeTab->setStyleSheet("QTabBar::tab { height: 0px; width: 0px; padding: 0px; margin: 0px; border: none; }");

    // --- Tab 1: 前处理工作区 ---
    glWidget = new GLWidget(this);
    glWidget->setRepository(&m_repository); // ptr to the entities repo
    mainModeTab->addTab(glWidget, "1. 前处理与建模 (Pre-Processing)");

    // --- Tab 2: 后处理工作区 ---
    postProcessWidget = new QWidget();
    setupPostProcessUI(); // 在这里面把后处理控件都加到 postProcessWidget 上
    mainModeTab->addTab(postProcessWidget, "2. 求解与后处理 (Solve & Post)");


    // ==========================================
    // 2. 原有的初始化逻辑 (保持不变)
    // ==========================================
    startMeshing();
    // 连接绘图完成信号
    connect(glWidget, &GLWidget::drawingComplete, this, &MainWindow::onDrawingComplete);

    // 初始化菜单栏、工具栏、状态栏、停靠窗口
    createMenuBar();
    createToolBars();
    createStatusBar();
    createDockWidgets();
    createCommandLine();
    createSimulationSetupDock();

    setWindowTitle("SafetyMunitionTestPlatform");
    resize(1024, 768);

    // clear()
    clean();

    m_meshManager = new MeshManager(this);

    // 核心：当网格文件准备好后，自动触发 parseMeshFile 进行解析和渲染
    connect(m_meshManager, &MeshManager::meshReady, this, &MainWindow::parseMeshFile);

    // 错误处理：如果 Gmsh 报错，打印到日志
    connect(m_meshManager, &MeshManager::errorOccurred, this, [this](QString msg) {
        logCommand("Error", msg);
        });

    // 初始化默认工作目录为当前程序运行的目录
    m_workingDirectory = QDir::currentPath();


    // ==========================================
    // 3. 新增：后处理进程与工作区智能联动
    // ==========================================

    // 初始化求解器后台进程
    m_solverProcess = new QProcess(this);
    connect(m_solverProcess, &QProcess::readyReadStandardOutput, this, &MainWindow::readSolverOutput);
    connect(m_solverProcess, &QProcess::readyReadStandardError, this, &MainWindow::readSolverOutput);
    connect(m_solverProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this, &MainWindow::handleSolverFinished);

    // ✨ 杀手锏功能：工作区智能切换 (类似 ABAQUS 切换 Module)
    // 只要检测到用户切换了 Tab 页面，自动隐藏/显示外围的 Dock 和工具栏
    connect(mainModeTab, &QTabWidget::currentChanged, this, [this](int index) {
        bool isPreProcess = (index == 0); // 只有在第一页时，才显示前处理面板

        // 自动遍历并控制所有停靠窗口 (Docks) 的显示状态
        for (QDockWidget* dock : this->findChildren<QDockWidget*>()) {
            dock->setVisible(isPreProcess);
        }
        });
}

MainWindow::~MainWindow() {}

void MainWindow::createMenuBar() {
    menuBar = new QMenuBar(this);
    setMenuBar(menuBar);

    // 文件菜单
    QMenu *fileMenu = menuBar->addMenu("File");
    QAction *newAction = fileMenu->addAction("New");
    QAction *openAction = fileMenu->addAction("Open");
    QAction *saveAction = fileMenu->addAction("Save");
    QAction *exitAction = fileMenu->addAction("Exit");

    // 编辑菜单
    QMenu *editMenu = menuBar->addMenu("Edit");
    QAction *cutAction = editMenu->addAction("Cut");
    QAction *copyAction = editMenu->addAction("Copy");
    QAction *pasteAction = editMenu->addAction("Paste");

    // 视图菜单
    QMenu *viewMenu = menuBar->addMenu("View");
    QAction *zoomInAction = viewMenu->addAction("Zoom In");
    QAction *zoomOutAction = viewMenu->addAction("Zoom Out");
    QAction *resetViewAction = viewMenu->addAction("Reset View");

    // 连接信号和槽
    connect(exitAction, &QAction::triggered, this, &QApplication::quit);
    connect(zoomInAction, &QAction::triggered, glWidget, [this]() { glWidget->scaleFactor *= 1.1; glWidget->update(); });
    connect(zoomOutAction, &QAction::triggered, glWidget, [this]() { glWidget->scaleFactor /= 1.1; glWidget->update(); });
    connect(resetViewAction, &QAction::triggered, glWidget, [this]() {
        glWidget->rotationX = glWidget->rotationY = glWidget->rotationZ = 0;
        glWidget->scaleFactor = 1.0;
        glWidget->translation = QVector3D(0, 0, 0);
        glWidget->update();
    });

    QAction* setWorkDirAct = new QAction(tr("设置工作目录 (Set Working Directory)..."), this);
    setWorkDirAct->setStatusTip(tr("设置所有导出文件和仿真数据的保存目录"));
    connect(setWorkDirAct, &QAction::triggered, this, &MainWindow::onSetWorkingDirectory);

    fileMenu->addAction(setWorkDirAct);
    fileMenu->addSeparator();
}

void MainWindow::createToolBars() {
    // 1. 创建全新的全局模式切换工具栏
    QToolBar* modeToolBar = addToolBar("工作模式 (Mode)");
    modeToolBar->setMovable(false); // 固定在顶部，不让用户乱拖破坏布局

    // 2. 创建两个切换动作 (Action)
    QAction* preModeAct = new QAction("1. 前处理与建模", this);
    preModeAct->setCheckable(true);
    preModeAct->setChecked(true); // 默认启动时选中前处理

    QAction* postModeAct = new QAction("2. 求解与后处理", this);
    postModeAct->setCheckable(true);

    // 3. 把它们加入互斥组 (ActionGroup)，保证一次只能按下一个
    QActionGroup* modeGroup = new QActionGroup(this);
    modeGroup->addAction(preModeAct);
    modeGroup->addAction(postModeAct);
    modeGroup->setExclusive(true);

    // 4. 将动作添加到工具栏
    modeToolBar->addAction(preModeAct);
    modeToolBar->addSeparator();
    modeToolBar->addAction(postModeAct);

    // 5. 绑定点击事件，通过点击工具栏按钮，在底层悄悄切换 Tab 页面
    connect(preModeAct, &QAction::triggered, this, [this]() {
        mainModeTab->setCurrentIndex(0);
        });
    connect(postModeAct, &QAction::triggered, this, [this]() {
        mainModeTab->setCurrentIndex(1);
        });

    // 6. UI 美化：把工具栏变成极具现代工业软件感的设计
    modeToolBar->setStyleSheet(
        "QToolBar {"
        "   background-color: #F8F9FA;"
        "   border-bottom: 1px solid #D0D0D0;"
        "   padding: 4px;"
        "}"
        "QToolButton {"
        "   font-size: 11pt;"
        "   font-weight: bold;"
        "   padding: 6px 20px;"
        "   margin: 0px 5px;"
        "   border-radius: 4px;"
        "   color: #555555;"
        "   background-color: transparent;"
        "}"
        "QToolButton:checked {"
        "   background-color: #0055A4;"   /* 选中时变为醒目的品牌蓝 */
        "   color: white;"                /* 选中时字体变白 */
        "}"
        "QToolButton:hover:!checked {"
        "   background-color: #E2E6EA;"   /* 鼠标悬浮时的交互浅灰色 */
        "}"
    );
}

void MainWindow::createStatusBar() {
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    QLabel *coordLabel = new QLabel("X: 0, Y: 0, Z: 0", this);
    statusBar->addPermanentWidget(coordLabel);
    statusBar->showMessage("Ready");
}

void MainWindow::createDockWidgets() {
    // ==========================================
    // 1. 命令历史停靠窗口 (保留在底部不变)
    // ==========================================
    commandDock = new QDockWidget("Command History", this);
    commandDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    commandHistoryEdit = new QTextEdit();  // 初始化 commandHistoryEdit
    commandHistoryEdit->setReadOnly(true);
    commandDock->setWidget(commandHistoryEdit);
    addDockWidget(Qt::BottomDockWidgetArea, commandDock);

    // ==========================================
    // 2. 实体树停靠窗口 (Substances)
    // ==========================================
    substanceDock = new QDockWidget("Substances", this);
    substanceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    substanceTree = new QTreeWidget();
    substanceTree->setHeaderLabel("substanceTree");
    substanceDock->setWidget(substanceTree);
    substanceDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    substanceTree->setContextMenuPolicy(Qt::CustomContextMenu);

    // 🌟 核心修改 1：原本是 LeftDockWidgetArea，现在移到【右侧】
    addDockWidget(Qt::RightDockWidgetArea, substanceDock);

    // 绑定右键菜单信号
    connect(substanceTree, &QTreeWidget::customContextMenuRequested,
        this, &MainWindow::onSubstanceTreeContextMenu);

    // ==========================================
    // 3. 实体生成器停靠窗口 (Entity Generator)
    // ==========================================
    QDockWidget* generatorDock = new QDockWidget(tr("Entity Generator"), this);
    generatorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    generatorDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    // 创建标签页控件作为 Dock 的核心组件
    QTabWidget* tabWidget = new QTabWidget(generatorDock);

    // 将三个分类作为不同的 Tab 添加进去
    setupGeneratorTab(tabWidget, tr("破片 (Fragment)"),
        { "Cube", "Sphere", "Hemisphere" }, m_fragmentUI);

    setupGeneratorTab(tabWidget, tr("壳体 (Shell)"),
        { "CylindricalShell", "OpenCylindricalShell", "Frustum" }, m_shellUI);

    setupGeneratorTab(tabWidget, tr("装药 (Charge)"),
        { "Cylinder", "HalfCylinder", "Cube", "Sphere" }, m_chargeUI);

    // 将 TabWidget 设置为 Dock 的主体，并添加到右侧
    generatorDock->setWidget(tabWidget);
    addDockWidget(Qt::RightDockWidgetArea, generatorDock);

    // 🌟 核心修改 2：把右侧的 Substance Tree 和 Entity Generator 合并成标签页组！
    tabifyDockWidget(substanceDock, generatorDock);
    substanceDock->raise(); // 默认让 Substance Tree 显示在前面
}

void MainWindow::createCommandLine() {
    QDockWidget *commandLineDock = new QDockWidget("Command Line", this);
    commandLineDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    commandLine = new CommandLine();
    connect(commandLine, &CommandLine::commandEntered, this, &MainWindow::onCommandEntered);
    commandLineDock->setWidget(commandLine);
    addDockWidget(Qt::BottomDockWidgetArea, commandLineDock);
}

void MainWindow::onCommandEntered(const QString &command) {
    
    logCommand(command);

    QStringList args = command.split(" ", Qt::SkipEmptyParts);
    if (args.isEmpty()) return;

    QString cmd = args[0].toLower();
    
    if (cmd == "cube") {
     
        if (args.size() < 9) {
            logCommand(command, "用法: box [name] [lx] [ly] [lz] [ms] [cx] [cy] [cz]");
            return;
        }

        QString name = args[1];
        double lx = args[2].toDouble();
        double ly = args[3].toDouble();
        double lz = args[4].toDouble();
        double ms = args[5].toDouble();
        double cx = args[6].toDouble();
        double cy = args[7].toDouble();
        double cz = args[8].toDouble();

        // 1. 实例化立方体生成器
        CubeGenerator cubeGen;

        // 2. 设置参数（长度、宽度、高度、网格大小、中心点坐标）
        cubeGen.setParameters(lx, ly, lz, ms, cx, cy, cz);

        // 3. 调用管理器进行构建与加载
        // MeshManager 会获取 boxGen 的 .geo 脚本并交给 Gmsh 处理
        m_meshManager->buildAndLoad(cubeGen, name);

        logCommand(command, QString("Box (%1x%2x%3) generation task sent to manager...").arg(lx).arg(ly).arg(lz));
     }
    
    else if (cmd == "zoom") {
        if (args.size() > 1 && args[1] == "in") {
            glWidget->m_zoom *= 1.5f;
            logCommand(command, "Zoomed in.");
        } else if (args.size() > 1 && args[1] == "out") {
            glWidget->m_zoom /= 1.5f;
            logCommand(command, "Zoomed out.");
        } else {
            logCommand(command, "Error: Invalid zoom argument. Use 'zoom in' or 'zoom out'.");
        }
    } else if (cmd == "reset") {
        //glWidget->resetView();
		startMeshing();
        logCommand(command, "View reset to default.");
    } else if (cmd == "clear") {

        // 1. 清空数据仓库 (核心：源头清理)
            m_repository.clearAll();

        // 2. 清理 UI 界面上的对象树
        substanceTree->clear();

        // 3. 通知 GLWidget 重新绘图
        // 由于 GLWidget 持有仓库指针，当它 update 时发现仓库为空，自然就什么都不画了
        glWidget->update();
    } 
    else if (cmd == "help") {
        showHelp();
    } 
    else if (cmd == "cylinder") {
    
		//generateCylindricalMesh(3.8, 10.0, 0.1);
        if (args.size() < 8) {
            logCommand(command, "Argument: cylinder [name] [radius] [ms] [height] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();
        double ms = args[3].toDouble();
		double height = args[4].toDouble();
        double cx = args[5].toDouble();
        double cy = args[6].toDouble();
        double cz = args[7].toDouble();

        // 实例化并设置参数
        CylinderGenerator cylinderGen;
        cylinderGen.setParameters(r, ms, height, cx, cy, cz);

        // 调用管理器（MeshManager 会处理 buildAndLoad 逻辑）
        m_meshManager->buildAndLoad(cylinderGen, name);

        logCommand(command, "Sphere generation task sent to manager...");
    
        //glWidget->update();
    }

    else if (cmd == "half_cylinder") {
        // 预期命令格式: half_cylinder [名称] [半径] [网格尺寸] [高度] [x] [y] [z]
        if (args.size() < 8) {
            logCommand(command, "Argument: half_cylinder [name] [radius] [ms] [height] [x] [y] [z]");
            return;
        }

        // 1. 解析参数
        QString name = args[1];
        double r = args[2].toDouble();
        double ms = args[3].toDouble();
        double height = args[4].toDouble();
        double cx = args[5].toDouble();
        double cy = args[6].toDouble();
        double cz = args[7].toDouble();

        // 2. 实例化生成器并设置参数
        // 确保你已经在 mainwindow.h 中包含了 #include "src/core/generation/HalfCylinderGenerator.h"
        HalfCylinderGenerator halfCylinderGen;
        halfCylinderGen.setParameters(r, height, ms, cx, cy, cz);

        // 3. 调用管理器执行生成、保存和解析流程
        // MeshManager 会调用 generator.generateGeoScript() 并通过 Gmsh 产生 .msh 文件
        m_meshManager->buildAndLoad(halfCylinderGen, name);

        logCommand(command, QString("Half-Cylinder '%1' generation task sent to manager...").arg(name));
    }

    else if (cmd == "cylindrical_shell") {
        
        // args[0] 是 "cylindrical_shell"，所以 args.size() 至少需要 10
        if (args.size() < 10) {
            logCommand(command, "用法: cylindrical_shell [name] [radius] [height] [lid] [wall] [ms] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();      // 内径
        double h = args[3].toDouble();      // 总高度
        double lid = args[4].toDouble();    // 端盖厚度
        double wall = args[5].toDouble();   // 侧壁厚度
        double ms = args[6].toDouble();     // 网格大小

        // 中心点坐标 (目前 Generator 内部逻辑以 0,0,0 为底面中心，
        double cx = args[7].toDouble();
        double cy = args[8].toDouble();
        double cz = args[9].toDouble();

        // 1. 实例化圆柱壳体生成器
        CylindricalShellGenerator shellGen;

        // 2. 设置参数
        // 注意：这里调用的参数顺序需对应类中 setParameters 的定义
        shellGen.setParameters(r, h, lid, wall, ms, cx, cy, cz);

        // 3. 调用管理器进行构建与加载
        m_meshManager->buildAndLoad(shellGen, name);

        logCommand(command, QString("Cylindrical Shell (%1) generation task sent to manager...").arg(name));
    }

    else if (cmd == "open_cylindrical_shell") {

        // args[0] 是 "open_cylindrical_shell"
        // 参数顺序: name, radius, wall, h_base, h_wall, ms, x, y, z
        if (args.size() < 10) {
            logCommand(command, "用法: open_cylindrical_shell [name] [radius] [wall] [h_base] [h_wall] [ms] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();      // 内径
        double wall = args[3].toDouble();   // 侧壁厚度
        double h_base = args[4].toDouble(); // 底部厚度
        double h_wall = args[5].toDouble(); // 侧壁高度
        double ms = args[6].toDouble();     // 网格大小

        // 中心点坐标预留（如需平移，需在Generator脚本中添加Translate逻辑）
        double cx = args[7].toDouble();
        double cy = args[8].toDouble();
        double cz = args[9].toDouble();

        // 1. 实例化开口圆柱壳体生成器 (有底无盖) [cite: 86, 107]
        OpenCylindricalShellGenerator openShellGen;

        // 2. 设置参数 
        // 对应封装类中的: setParameters(radius, wall, h_base, h_wall, meshSize) 
        openShellGen.setParameters(r, wall, h_base, h_wall, ms, cx, cy, cz);

        // 3. 调用管理器进行构建与加载
        m_meshManager->buildAndLoad(openShellGen, name);

        logCommand(command, QString("Open Cylindrical Shell (有底无盖: %1) generation task sent to manager...").arg(name));
        }

    else if (cmd == "sphere") {
    
        if (args.size() < 7) {
            logCommand(command, "用法: sphere [name] [radius] [ms] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();
        double ms = args[3].toDouble();
        double cx = args[4].toDouble();
        double cy = args[5].toDouble();
        double cz = args[6].toDouble();

        // 实例化并设置参数
        SphereGenerator sphereGen;
        sphereGen.setParameters(r, ms, cx, cy, cz);

        // 调用管理器（MeshManager 会处理 buildAndLoad 逻辑）
        m_meshManager->buildAndLoad(sphereGen, name);

        logCommand(command, "Sphere generation task sent to manager...");
    }

    else if (cmd == "hemisphere") {
        // 参数检查: hemisphere [name] [radius] [ms] [cx] [cy] [cz]
        // args[0] 是 "hemisphere"，所以 args.size() 至少需要 7
        if (args.size() < 7) {
            logCommand(command, "用法: hemisphere [name] [radius] [ms] [cx] [cy] [cz]");
            return;
        }

        QString name = args[1];
        double r = args[2].toDouble();  // 半径
        double ms = args[3].toDouble();  // 网格大小
        double cx = args[4].toDouble();  // 中心 X (底面圆心)
        double cy = args[5].toDouble();  // 中心 Y
        double cz = args[6].toDouble();  // 中心 Z

        // 1. 实例化半球形生成器 (使用 O-Grid 结构化算法)
        HemisphereGenerator hemiGen;

        // 2. 设置参数
        hemiGen.setParameters(r, ms, cx, cy, cz);

        // 3. 调用管理器进行构建与加载
        // MeshManager 会调用 hemiGen.generateGeoScript() 并驱动 Gmsh 渲染
        m_meshManager->buildAndLoad(hemiGen, name);

        logCommand(command, QString("Hemisphere (R=%1, MS=%2) task sent to manager...").arg(r).arg(ms));

        // 如果需要立即刷新界面
        // glWidget->update();
        }

    else if (cmd == "frustum") {
        // frustum [name] [rBase] [rTop] [height] [ms] [x] [y] [z]
        if (args.size() < 9) {
            logCommand(command, "用法: frustum [name] [rBase] [rTop] [height] [ms] [x] [y] [z]");
            return;
        }

        QString name = args[1];
        double rb = args[2].toDouble();
        double rt = args[3].toDouble();
        double h = args[4].toDouble();
        double ms = args[5].toDouble();
        double cx = args[6].toDouble();
        double cy = args[7].toDouble();
        double cz = args[8].toDouble();

        FrustumGenerator frustumGen;
        frustumGen.setParameters(rb, rt, h, ms, cx, cy, cz);

        m_meshManager->buildAndLoad(frustumGen, name);
        logCommand(command, QString("Frustum (%1) task sent to manager...").arg(name));
        }

    else if (cmd == "save" || cmd == "export") {
        if (args.size() < 2) {
            logCommand(command, "用法: save [filename.k]");
            return;
        }
        QString fileName = args[1];
        if (!fileName.endsWith(".k")) fileName += ".k";

        exportToKFile(fileName);
        }

      else {
        logCommand("Error: Unknown command. Type 'help' for a list of commands.");
    }
    glWidget->update();
}

void MainWindow::onDrawingComplete() {
    statusBar->showMessage("Drawing completed.");
}

void MainWindow::logCommand(const QString &command, const QString &response) {
    commandHistoryEdit->append("> " + command);
    if (!response.isEmpty()) {
        commandHistoryEdit->append(" " + response);
    }
    commandHistoryEdit->ensureCursorVisible();
    statusBar->showMessage("Executed: " + command);
}


void MainWindow::showHelp() {
    if (commandLine) {  // 检查是否为空指针
        QString helpText = commandLine->getHelpText();
        commandHistoryEdit->append(helpText);
        logCommand("help", helpText);
    }
}

void MainWindow::DrawLine(QStringList args) {

    if (args.size() == 8) {
        
        QString line_name = args[1];
        float x_start = args[2].toFloat();
        float y_start = args[3].toFloat();
        float z_start = args[4].toFloat();
        float x_end = args[5].toFloat();
        float y_end = args[6].toFloat();
        float z_end = args[7].toFloat();

        Vector3 start = { x_start, y_start, z_start };
        Vector3 end = { x_end, y_end, z_end };

        drawables_lines.push_back(GeoLine(start, end));
        glWidget->setDrawableLines(drawables_lines);

        QTreeWidgetItem* instance_generate = new QTreeWidgetItem(substanceTree);
        instance_generate->setText(0, line_name);

        glWidget->update();
    }
    else {
        logCommand(QString("Error: Missing argument for 'line' command."));
    }
}

void MainWindow::clean() {

}

void MainWindow::startMeshing() {

    QString geoContent = "Point(1) = {0, 0, 0, 0.1}; Point(2) = {1, 0, 0, 0.1}; Line(1) = {1, 2};";

    QFile file("input.geo");
    if (file.open(QIODevice::WriteOnly)) {
        file.write(geoContent.toUtf8());
        file.close();
    }

    // 2. 像在命令行里输入命令一样启动 gmsh.exe
    QProcess gmsh;
    QStringList args;
    args << "input.geo" << "-1" << "-o" << "output.msh"; // -1 代表生成1D网格，-o 输出文件名
    gmsh.start("gmsh.exe", args);
    if ( !gmsh.waitForFinished() ) {
		qDebug() << "Gmsh 进程启动失败或超时！";
    }

    //qDebug() << "网格划分完成！文件已保存在 output.msh";
}

void MainWindow::parseMeshFile(QString fileName) {

    QFile file(fileName);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) return;

    QTextStream in(&file);
    QString line;
    std::vector<MeshPoint> nodes; nodes.push_back(MeshPoint());//占位
	std::vector<Hexahedron> hexes;

    QString entityName = QFileInfo(fileName).baseName();

    while (!in.atEnd()) {
        line = in.readLine().trimmed();

        // 找节点数据块
        if (line == "$Nodes") {
            int numNodes = in.readLine().trimmed().toInt();
            for (int i = 0; i < numNodes; ++i) {
                QStringList data = in.readLine().split(" ");
                if (data.size() >= 4) {
                    MeshPoint node;
                   
                    node.pos.setX( data[1].toDouble() );
                    node.pos.setY(data[2].toDouble());
                    node.pos.setZ(data[3].toDouble());
					nodes.push_back(node);
                }
            }
        }

        if (line == "$Elements") {
        
            int numElements = in.readLine().trimmed().toInt();
            for (int i = 0; i < numElements; i++) {
                QStringList data = in.readLine().split(" ", QString::SkipEmptyParts);
                if (data.size() < 3) continue;

                int elementType = data[1].toInt();

                if (elementType == 5) {
                
                    Hexahedron hex;
					//hex.id = data[0].toInt();
					int numTags = data[2].toInt();
                    for (int j = 0; j < 8; j++) {
                    
                        hex[j] = data[3 +numTags+ j].toInt();
                    }
					hexes.push_back(hex);
                }
            }
        }
    }

    std::vector<MeshPoint> points;
    for (auto& n : nodes)
        points.push_back(n);

    /*DrawCommand cmd;
    cmd.type = DrawCommand::Points;
    cmd.pointsCmd.points = points;
    cmd.pointsCmd.size = 2.5f;*/

    //glWidget->clearDrawCommands();
    /*glWidget->submitDrawCommand(cmd);*/

    std::vector<Vector3> wireLines = buildHexWireframe(nodes, hexes);
   // auto wireLines = buildHexWireframe( nodes, hexes );

    MeshEntity newEntity;
    newEntity.name = entityName;
    newEntity.type = "Empty"; // 或者是从某个地方传过来的类型
    newEntity.nodes = points;
    newEntity.hexes = hexes;
    newEntity.wireLines = wireLines;

    m_repository.addEntity(entityName, newEntity);

    /*DrawCommand lineCmd;
    lineCmd.type = DrawCommand::Lines;
    lineCmd.linesCmd.points = wireLines;
    lineCmd.linesCmd.width = 2.0f;*/

    //glWidget->submitDrawCommand(lineCmd);
    redrawAllEntities();

    /*qDebug() << "done parsing and wireframe generated!";
    qDebug() << "Wireframe submitted with" << wireLines.size() / 2 << "lines.";*/
    file.close();
}

void MainWindow::redrawAllEntities() {
    // 1. 清空 GLWidget 里的旧命令
    glWidget->clearDrawCommands();

    //// 2. 遍历仓库里的所有实体
    //const auto& allEntities = m_repository.getAllEntities();
    //for (const auto& pair : allEntities) {
    //    const MeshEntity& entity = pair.second;

    //    // 提交点云渲染命令 (如果你需要)
    //    DrawCommand ptCmd;
    //    ptCmd.type = DrawCommand::Points;
    //    ptCmd.pointsCmd.points = entity.nodes;
    //    ptCmd.pointsCmd.size = 2.5f;
    //    glWidget->submitDrawCommand(ptCmd);

    //    // 提交线框渲染命令
    //    DrawCommand lineCmd;
    //    lineCmd.type = DrawCommand::Lines;
    //    lineCmd.linesCmd.points = entity.wireLines;
    //    lineCmd.linesCmd.width = 2.0f;
    //    glWidget->submitDrawCommand(lineCmd);
    //}

    // 3. 触发显卡刷新
    glWidget->update();
    // 4. Update Substance Tree
    updateSubstanceTree();
}

void MainWindow::clearAllEntities() {
    m_repository.clearAll();      // 清空账本
    glWidget->clearDrawCommands(); // 清空显卡指令
    glWidget->update();           // 刷新画面
    qDebug() << "Cleared all entities";
}

void MainWindow::updateSubstanceTree() {
    if (!substanceTree) return;

    substanceTree->clear();
    // 确保有两列，一列显示名称，一列显示信息
    substanceTree->setHeaderLabels(QStringList() << "Entity / Boundary" << "Info");

    const auto& allEntities = m_repository.getAllEntities();

    // 遍历仓库中的所有实体
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it) {
        QString entityName = it->first;
        const MeshEntity& entity = it->second;

        // 1. 创建实体项 (父节点)，挂载在 substanceTree 上
        QTreeWidgetItem* entityItem = new QTreeWidgetItem(substanceTree);
        entityItem->setText(0, entityName);
        entityItem->setText(1, QString("Nodes: %1").arg(entity.nodes.size()));

        // --- [新增边界子节点逻辑开始] ---
        // 2. 遍历该实体所拥有的边界，作为小项挂载到 entityItem 下面
        for (const auto& boundary : entity.boundaries) {
            // 关键：传入 entityItem 作为父级，这样它就会变成可展开的子项
            QTreeWidgetItem* boundaryItem = new QTreeWidgetItem(entityItem);

            // 为了视觉上区分，可以加个前缀或图标
            boundaryItem->setText(0, QString("[Boundary] %1").arg(QString::fromStdString(boundary.name)));
            boundaryItem->setText(1, QString("Size: %1").arg(boundary.nodeIndices.size()));

            // (可选) 给边界小项换个颜色，比如暗灰色或蓝色，以便和实体区分
            boundaryItem->setForeground(0, QBrush(QColor(80, 120, 200)));
        }
        // --- [新增边界子节点逻辑结束] ---

        // (可选) 默认展开包含边界的实体节点
        if (!entity.boundaries.empty()) {
            entityItem->setExpanded(true);
        }
    }

    // 每次新建实体，更新一次下拉菜单
    m_simSetupUI.entitySelector->clear();
    for (const auto& pair : m_repository.getAllEntities()) {
        m_simSetupUI.entitySelector->addItem(pair.first); // pair.first 是实体名字，如 Cube_1
    }

    if (m_simSetupUI.entitySelector != nullptr) {
        updateAllEntitySelectors();
    }
}

void MainWindow::onSubstanceTreeContextMenu(const QPoint& pos) {
    QTreeWidgetItem* item = substanceTree->itemAt(pos);
    // 假设只有顶层实体可以操作 (没有父节点的 Item)
    if (!item || item->parent() != nullptr) return;

    QString entityName = item->text(0);
    QMenu menu(this);

    // 1. 删除
    QAction* delAct = menu.addAction("删除实体 (Delete)");
    connect(delAct, &QAction::triggered, this, [this, entityName]() { handleDeleteEntity(entityName); });

    menu.addSeparator(); // 加条分割线

    // 2. 平移
    QAction* transAct = menu.addAction("平移 (Translate)");
    connect(transAct, &QAction::triggered, this, [this, entityName]() { handleTranslateEntity(entityName); });

    // 3. 缩放
    QAction* scaleAct = menu.addAction("缩放 (Scale)");
    connect(scaleAct, &QAction::triggered, this, [this, entityName]() { handleScaleEntity(entityName); });

    // 4. 旋转
    QAction* rotateAct = menu.addAction("旋转 (Rotate)");
    connect(rotateAct, &QAction::triggered, this, [this, entityName]() { handleRotateEntity(entityName); });

    menu.exec(substanceTree->viewport()->mapToGlobal(pos));
}

void MainWindow::handleDeleteEntity(QString name) {
    // 1. 从实体仓库(Repository)中移除数据
    if (m_repository.deleteEntity(name)) {

        // 2. 刷新 3D 画面 (清空指令 -> 重新遍历剩余实体提交给 GLWidget)
        redrawAllEntities();

        // 3. 刷新左侧列表 (清空树节点 -> 重新遍历剩余实体插入 Tree)
        updateSubstanceTree();

        qDebug() << "Entity deleted and UI refreshed: " << name;
    }
}

void MainWindow::exportToKFile(const QString& fileName) {
    QFile file(fileName);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        logCommand("Export", "Failed to open file: " + fileName);
        return;
    }

    QTextStream out(&file);
    out << "*KEYWORD\n";

    const auto& allEntities = m_repository.getAllEntities(); // 获取仓库中所有实体

    int globalNodeId = 1;  // 全局节点计数器
    int globalElemId = 1;  // 全局单元计数器
    std::vector<int> globalSensorIds; // 收集所有传感器的全局 ID

    for (auto it = allEntities.begin(); it != allEntities.end(); ++it) {
        const MeshEntity& entity = it->second;
        QString entityName = it->first; // 获取实体名字

        int realPartId = getOrCreatePart(entityName)->pid;

        // 映射表：局部向量索引 -> 全局文件节点 ID
        std::unordered_map<int, int> localToGlobal;

        out << "$ #################################################\n";
        out << "$ Entity: " << entity.name << "\n";
        out << "$ #################################################\n";

        // 1. 导出节点 (从索引 1 开始，跳过占位的 nodes[0])
        out << "*NODE\n";
        for (size_t i = 1; i < entity.nodes.size(); ++i) {
            localToGlobal[static_cast<int>(i)] = globalNodeId;

            // 格式：ID, X, Y, Z
            out << globalNodeId << ", "
                << entity.nodes[i].pos.x() << ", "
                << entity.nodes[i].pos.y() << ", "
                << entity.nodes[i].pos.z() << "\n";

            if (m_sensorNodes.contains(entityName) && m_sensorNodes[entityName].contains(i)) {
                globalSensorIds.push_back(globalNodeId);
            }

            globalNodeId++;
        }

        // 2. 导出六面体单元
        out << "*ELEMENT_SOLID\n";
        for (const auto& hex : entity.hexes) {
            // 解决报错的关键：直接使用 hex[j] 访问 std::array 元素
            out << globalElemId << ", " << realPartId;

            for (int j = 0; j < 8; ++j) {
                int localIdx = hex[j]; // 获取存储在 array 中的局部节点索引
                out << ", " << localToGlobal[localIdx]; // 映射为全局 ID
            }
            out << "\n";

            globalElemId++;
        }

    }

    if (!globalSensorIds.empty()) {
        // 1. 指定观测点全局 ID
        out << "*DATABASE_HISTORY_NODE\n";
        out << "$#    id1       id2       id3       id4       id5       id6       id7       id8\n";

        // LS-DYNA 要求每行最多 8 个 ID，宽度为 10，自动换行
        for (size_t i = 0; i < globalSensorIds.size(); ++i) {
            out << QString("%1").arg(globalSensorIds[i], 10, 10, QChar(' '));
            if ((i + 1) % 8 == 0) out << "\n";
        }
        if (globalSensorIds.size() % 8 != 0) out << "\n";

        // 2. 指定节点数据的输出时间步长 (这里默认0.005，越小数据点越密，曲线越平滑)
        out << "*DATABASE_NODOUT\n";
        out << "$#      dt      lcdt      beam     npltc    psetid\n";
        out << "     0.005         0         0         0         0\n";
    }

    out << "*END\n";
    file.close();
    logCommand("Export", "Saved entities to " + fileName);
}


// 辅助函数：创建独立且不可关闭的 Dock 窗口
void MainWindow::setupGeneratorTab(QTabWidget* tabWidget, const QString& title, const QStringList& shapes, GeneratorUI& ui) {
    // 1. 创建一个普通的 QWidget 作为单独的标签页
    QWidget* tab = new QWidget(tabWidget);
    QVBoxLayout* mainLayout = new QVBoxLayout(tab);

    // 2. 形状选择下拉框
    mainLayout->addWidget(new QLabel("Select Shape:"));
    ui.shapeComboBox = new QComboBox(tab);
    ui.shapeComboBox->addItems(shapes);
    mainLayout->addWidget(ui.shapeComboBox);

    // 3. 动态参数区域
    mainLayout->addSpacing(10);
    mainLayout->addWidget(new QLabel("Parameters:"));
    ui.paramContainer = new QWidget(tab);
    ui.paramLayout = new QVBoxLayout(ui.paramContainer);
    ui.paramLayout->setContentsMargins(0, 0, 0, 0);
    mainLayout->addWidget(ui.paramContainer);

    mainLayout->addStretch(1);

    // 4. 生成按钮
    QPushButton* generateBtn = new QPushButton("Generate " + title, tab);
    mainLayout->addWidget(generateBtn);

    // 5. 将配置好的页面添加到 TabWidget 中
    tabWidget->addTab(tab, title);

    // 6. 信号槽连接
    connect(ui.shapeComboBox, &QComboBox::currentTextChanged, this, [this, &ui](const QString& text) {
        handleShapeTypeChanged(ui, text);
        });
    connect(generateBtn, &QPushButton::clicked, this, [this, &ui]() {
        handleGenerateButtonClicked(ui);
        });

    // 初始化显示
    handleShapeTypeChanged(ui, ui.shapeComboBox->currentText());
}

// 辅助函数：向特定的 UI 结构体中添加输入框
void MainWindow::addNumParamToUI(GeneratorUI& ui, const QString& labelText, const QString& key, double defaultValue, const QString& unit) {
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(labelText));
    QDoubleSpinBox* sb = new QDoubleSpinBox(this);
    sb->setRange(-9999.0, 9999.0);
    sb->setValue(defaultValue);

    // 🌟 新增：如果传入了单位，就把它作为后缀显示在数字后面
    if (!unit.isEmpty()) {
        sb->setSuffix(unit);
    }

    row->addWidget(sb);
    ui.paramLayout->addLayout(row);
    ui.paramInputs[key] = sb;
}

// 槽函数重构：处理任意一个窗口的下拉菜单变化
void MainWindow::handleShapeTypeChanged(GeneratorUI& ui, const QString& text) {
    if (!ui.paramLayout || !ui.paramContainer) return;

    // 清理该窗口旧的参数输入框
    QList<QWidget*> children = ui.paramContainer->findChildren<QWidget*>(QString(), Qt::FindDirectChildrenOnly);
    for (QWidget* child : children) {
        child->hide();
        child->deleteLater();
    }
    ui.paramInputs.clear();

    // 重新添加实体名称输入框
    QHBoxLayout* nameRow = new QHBoxLayout();
    nameRow->addWidget(new QLabel("Entity Name:"));
    ui.nameInput = new QLineEdit(this);
    ui.nameInput->setText(text.toLower() + "_1");
    nameRow->addWidget(ui.nameInput);
    ui.paramLayout->addLayout(nameRow);

    // 根据选中的形状生成参数
    if (text == "Cube") {
        addNumParamToUI(ui, "Length (LX):", "lx", 10.0, " cm");
        addNumParamToUI(ui, "Width (LY):", "ly", 10.0, " cm");
        addNumParamToUI(ui, "Height (LZ):", "lz", 10.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "Cylinder") {
        addNumParamToUI(ui, "Radius (R):", "r", 5.0, " cm");
        addNumParamToUI(ui, "Height (H):", "h", 15.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "CylindricalShell") {
        addNumParamToUI(ui, "Inner Radius:", "r", 5.0, " cm");
        addNumParamToUI(ui, "Total Height:", "h", 15.0, " cm");
        addNumParamToUI(ui, "Lid Thick:", "lid", 1.0, " cm");
        addNumParamToUI(ui, "Wall Thick:", "wall", 1.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "OpenCylindricalShell") {
        addNumParamToUI(ui, "Radius:", "r", 5.0, " cm");
        addNumParamToUI(ui, "Wall Thick:", "wall", 1.0, " cm");
        addNumParamToUI(ui, "Base Height:", "h_base", 1.0, " cm");
        addNumParamToUI(ui, "Wall Height:", "h_wall", 10.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "Sphere" || text == "Hemisphere") {
        addNumParamToUI(ui, "Radius:", "r", 5.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "Frustum") {
        addNumParamToUI(ui, "Base R:", "rb", 6.0, " cm");
        addNumParamToUI(ui, "Top R:", "rt", 3.0, " cm");
        addNumParamToUI(ui, "Height:", "h", 10.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
    else if (text == "HalfCylinder") {
        addNumParamToUI(ui, "Radius:", "r", 5.0, " cm");
        addNumParamToUI(ui, "Height:", "h", 15.0, " cm");
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0, " cm");
        addNumParamToUI(ui, "Center X:", "cx", 0.0, " cm");
        addNumParamToUI(ui, "Center Y:", "cy", 0.0, " cm");
        addNumParamToUI(ui, "Center Z:", "cz", 0.0, " cm");
    }
}

// 槽函数重构：处理任意一个窗口的生成按钮点击
void MainWindow::handleGenerateButtonClicked(GeneratorUI& ui) {
    if (!ui.nameInput || ui.shapeComboBox->currentText().isEmpty()) return;

    QString type = ui.shapeComboBox->currentText();
    QString name = ui.nameInput->text();

    // 从触发生成的特定窗口中获取数值
    auto val = [&](QString key) {
        return ui.paramInputs.contains(key) ? ui.paramInputs[key]->value() : 0.0;
        };

    if (type == "Cube") {
        CubeGenerator gen;
        gen.setParameters(val("lx"), val("ly"), val("lz"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Cylinder") {
        CylinderGenerator gen;
        gen.setParameters(val("r"), val("ms"), val("h"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "CylindricalShell") {
        CylindricalShellGenerator gen;
        gen.setParameters(val("r"), val("h"), val("lid"), val("wall"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "OpenCylindricalShell") {
        OpenCylindricalShellGenerator gen;
        gen.setParameters(val("r"), val("wall"), val("h_base"), val("h_wall"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Sphere") {
        SphereGenerator gen;
        gen.setParameters(val("r"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Hemisphere") {
        HemisphereGenerator gen;
        gen.setParameters(val("r"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "Frustum") {
        FrustumGenerator gen;
        gen.setParameters(val("rb"), val("rt"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }
    else if (type == "HalfCylinder") {
        HalfCylinderGenerator gen;
        gen.setParameters(val("r"), val("h"), val("ms"), val("cx"), val("cy"), val("cz"));
        m_meshManager->buildAndLoad(gen, name);
    }

    logCommand("GUI Generate (" + type + "): " + name);
    glWidget->update();
}

// ==========================================
// 1. 平移操作 (Translate)
// ==========================================
void MainWindow::handleTranslateEntity(const QString& entityName) {
    QDialog dialog(this);
    dialog.setWindowTitle("平移实体 - " + entityName);
    QFormLayout form(&dialog);

    QDoubleSpinBox* dx = new QDoubleSpinBox(&dialog); dx->setRange(-99999, 99999); dx->setDecimals(3);
    QDoubleSpinBox* dy = new QDoubleSpinBox(&dialog); dy->setRange(-99999, 99999); dy->setDecimals(3);
    QDoubleSpinBox* dz = new QDoubleSpinBox(&dialog); dz->setRange(-99999, 99999); dz->setDecimals(3);

    form.addRow("X位移 (dx):", dx);
    form.addRow("Y位移 (dy):", dy);
    form.addRow("Z位移 (dz):", dz);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(dx->value(), dy->value(), dz->value());

        applyTransformation(entityName, mat);
        logCommand("Transformation", "Translated " + entityName);
    }
}

// ==========================================
// 2. 缩放操作 (Scale)
// ==========================================
void MainWindow::handleScaleEntity(const QString& entityName) {
    QDialog dialog(this);
    dialog.setWindowTitle("缩放实体 - " + entityName);
    QFormLayout form(&dialog);

    // 缩放因子默认值为 1.0
    QDoubleSpinBox* sx = new QDoubleSpinBox(&dialog); sx->setRange(0.001, 9999); sx->setValue(1.0); sx->setDecimals(3);
    QDoubleSpinBox* sy = new QDoubleSpinBox(&dialog); sy->setRange(0.001, 9999); sy->setValue(1.0); sy->setDecimals(3);
    QDoubleSpinBox* sz = new QDoubleSpinBox(&dialog); sz->setRange(0.001, 9999); sz->setValue(1.0); sz->setDecimals(3);

    form.addRow("X向缩放:", sx);
    form.addRow("Y向缩放:", sy);
    form.addRow("Z向缩放:", sz);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        MeshEntity* entity = m_repository.getMutableEntity(entityName);
        if (!entity || entity->nodes.empty()) return;

        // 【关键】必须以实体的几何中心为基准进行缩放
        QVector3D center(0, 0, 0);
        for (const auto& node : entity->nodes) {
            center += QVector3D(node.pos.x(), node.pos.y(), node.pos.z());
        }
        center /= float(entity->nodes.size());

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(center); // 3. 将原点移回其实际位置
        mat.scale(sx->value(), sy->value(), sz->value()); // 2. 缩放
        mat.translate(-center); // 1. 将实体中心平移到坐标原点

        applyTransformation(entityName, mat);
        logCommand("Transformation", "Scaled " + entityName);
    }
}

// ==========================================
// 3. 旋转操作 (Rotate)
// ==========================================
void MainWindow::handleRotateEntity(const QString& entityName) {
    QDialog dialog(this);
    dialog.setWindowTitle("旋转实体 - " + entityName);
    QFormLayout form(&dialog);

    QDoubleSpinBox* cx = new QDoubleSpinBox(&dialog); cx->setRange(-9999, 9999); cx->setDecimals(3);
    QDoubleSpinBox* cy = new QDoubleSpinBox(&dialog); cy->setRange(-9999, 9999); cy->setDecimals(3);
    QDoubleSpinBox* cz = new QDoubleSpinBox(&dialog); cz->setRange(-9999, 9999); cz->setDecimals(3);

    QDoubleSpinBox* ax = new QDoubleSpinBox(&dialog); ax->setRange(-1.0, 1.0); ax->setValue(0.0);
    QDoubleSpinBox* ay = new QDoubleSpinBox(&dialog); ay->setRange(-1.0, 1.0); ay->setValue(0.0);
    QDoubleSpinBox* az = new QDoubleSpinBox(&dialog); az->setRange(-1.0, 1.0); az->setValue(1.0);

    QDoubleSpinBox* angle = new QDoubleSpinBox(&dialog); angle->setRange(-360.0, 360.0); angle->setValue(90.0);

    form.addRow("旋转中心 X:", cx);
    form.addRow("旋转中心 Y:", cy);
    form.addRow("旋转中心 Z:", cz);
    form.addRow("旋转轴向量 X:", ax);
    form.addRow("旋转轴向量 Y:", ay);
    form.addRow("旋转轴向量 Z:", az);
    form.addRow("旋转角度 (度):", angle);

    QDialogButtonBox buttonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    form.addRow(&buttonBox);
    connect(&buttonBox, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    connect(&buttonBox, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);

    if (dialog.exec() == QDialog::Accepted) {
        QVector3D center(cx->value(), cy->value(), cz->value());
        QVector3D axis(ax->value(), ay->value(), az->value());

        QMatrix4x4 mat;
        mat.setToIdentity();
        mat.translate(center);  // 3. 移回旋转中心点
        mat.rotate(angle->value(), axis); // 2. 绕原点的对应轴旋转
        mat.translate(-center); // 1. 将旋转中心点移到原点

        applyTransformation(entityName, mat);
        logCommand("Transformation", "Rotated " + entityName);
    }
}

// ==========================================
// 4. 矩阵变换底层执行 (Apply)
// ==========================================
void MainWindow::applyTransformation(const QString& entityName, const QMatrix4x4& mat) {
    MeshEntity* entity = m_repository.getMutableEntity(entityName);
    if (!entity) return;

    // 1. 变换所有节点的坐标
    for (auto& node : entity->nodes) {
        QVector3D oldPos(node.pos.x(), node.pos.y(), node.pos.z());
        QVector3D newPos = mat * oldPos; // 矩阵乘法直接完成变换

        // 重新赋值（请根据你自定义 Vector 的方法名修改此处，比如 setX 或者直接赋值）
        node.pos = QVector3D(newPos.x(), newPos.y(), newPos.z());
    }

    // 2. 变换实体线框/边界线（如果在渲染层你用 wireLines 保存了骨架线）
    for (auto& linePos : entity->wireLines) {
        QVector3D oldPos(linePos.x(), linePos.y(), linePos.z());
        QVector3D newPos = mat * oldPos;

        linePos = QVector3D(newPos.x(), newPos.y(), newPos.z());
    }

    // 3. 强制重绘 3D 界面
    glWidget->update();
}

void MainWindow::createSimulationSetupDock() {

    QDockWidget* setupDock = new QDockWidget(tr("Simulation Setup (物理与求解设置)"), this);
    setupDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    setupDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);

    m_simSetupUI.mainTab = new QTabWidget(setupDock);

    // ==========================================
    // Tab 1: 材料与状态方程 (Materials & EOS)
    // ==========================================
    QWidget* matTab = new QWidget();
    QVBoxLayout* matLayout = new QVBoxLayout(matTab);

    // 实体选择
    matLayout->addWidget(new QLabel("目标实体 (Target Entity):"));
    m_simSetupUI.entitySelector = new QComboBox();
    matLayout->addWidget(m_simSetupUI.entitySelector);

    // 材料库选择
    matLayout->addWidget(new QLabel("材料本构 (Material Model):"));
    m_simSetupUI.materialSelector = new QComboBox();
    m_simSetupUI.materialSelector->addItems({ "*MAT_PIECEWISE_LINEAR_PLASTICITY", "*MAT_JOHNSON_COOK", "*MAT_HIGH_EXPLOSIVE_BURN", "*MAT_ELASTIC_PLASTIC_HYDRO","*MAT_NULL" });
    matLayout->addWidget(m_simSetupUI.materialSelector);

    // EOS 库选择 (初始隐藏)
    QLabel* eosLabel = new QLabel("状态方程 (Equation of State):");
    m_simSetupUI.eosSelector = new QComboBox();

    m_simSetupUI.eosSelector->addItems({
            "None",
            "*EOS_GRUNEISEN",
            "*EOS_JWL",
            "*EOS_LINEAR_POLYNOMIAL",
            "*EOS_TILLOTSON",
            "*EOS_IGNITION_AND_GROWTH_OF_REACTION_IN_HE"
        });    

    matLayout->addWidget(eosLabel);
    matLayout->addWidget(m_simSetupUI.eosSelector);

    // 动态参数容器
    m_simSetupUI.matParamContainer = new QWidget();
    m_simSetupUI.matParamLayout = new QFormLayout(m_simSetupUI.matParamContainer);
    QScrollArea* scrollArea = new QScrollArea(); //滚动条
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(m_simSetupUI.matParamContainer);
    matLayout->addWidget(scrollArea);

    m_simSetupUI.mainTab->addTab(matTab, "材料(Material)");

    m_simSetupUI.btnAddMaterial = new QPushButton("添加材料与侵蚀设定 (Add Material)");
    matLayout->addWidget(m_simSetupUI.btnAddMaterial);

	// 材料预设下拉菜单
    m_simSetupUI.presetSelector = new QComboBox();
    
    matLayout->addWidget(new QLabel("选择材料预设 (自动填充参数):"));
    matLayout->addWidget(m_simSetupUI.presetSelector);

    loadMaterialPresetsFromJson();

    connect(m_simSetupUI.presetSelector, &QComboBox::currentTextChanged, this, &MainWindow::handlePresetChanged);
    // ==========================================
    // Tab 2: 求解控制 (Control)
    // ==========================================
    QWidget* ctrlTab = new QWidget();
    QFormLayout* ctrlLayout = new QFormLayout(ctrlTab);

    m_simSetupUI.endtimeInput = new QDoubleSpinBox();
    m_simSetupUI.endtimeInput->setRange(0, 99999); m_simSetupUI.endtimeInput->setValue(1.0);
    ctrlLayout->addRow("结束时间 (ENDTIM):", m_simSetupUI.endtimeInput);
    m_simSetupUI.endtimeInput->setSuffix(" μs");

    m_simSetupUI.d3plotFreqInput = new QDoubleSpinBox();
    m_simSetupUI.d3plotFreqInput->setRange(0, 9999); m_simSetupUI.d3plotFreqInput->setValue(0.01);
    ctrlLayout->addRow("D3PLOT 步长 (DT):", m_simSetupUI.d3plotFreqInput);
    m_simSetupUI.d3plotFreqInput->setSuffix(" μs");

    m_simSetupUI.mainTab->addTab(ctrlTab, "控制(Control)");

    // ==========================================
    // 底部应用按钮
    // ==========================================
    QWidget* mainContainer = new QWidget();
    QVBoxLayout* mainVLayout = new QVBoxLayout(mainContainer);
    mainVLayout->addWidget(m_simSetupUI.mainTab);

    // ==========================================
    // 底部：实时观察面板与导出区域
    // ==========================================

    // 1. 上半部分放入你的 TabWidget
    mainVLayout->addWidget(m_simSetupUI.mainTab);

    // 2. 下半部分放入实时观察列表
    mainVLayout->addWidget(new QLabel("已添加的仿真参数 (Active Cards):"));
    m_simSetupUI.setupSummaryList = new QListWidget();
    m_simSetupUI.setupSummaryList->setMaximumHeight(150);
    mainVLayout->addWidget(m_simSetupUI.setupSummaryList);

    m_simSetupUI.setupSummaryList->setContextMenuPolicy(Qt::CustomContextMenu);
    connect(m_simSetupUI.setupSummaryList, &QListWidget::customContextMenuRequested, this, &MainWindow::showSummaryContextMenu);

    // 3. 底部操作按钮
    QHBoxLayout* bottomBtnLayout = new QHBoxLayout();
    m_simSetupUI.btnClearSummary = new QPushButton("清空 (Clear)");
    m_simSetupUI.btnExportKFile = new QPushButton("导出工况.k文件(Export .k File)");
    m_simSetupUI.btnExportKFile->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;"); // 导出按钮搞个醒目的绿色

    bottomBtnLayout->addWidget(m_simSetupUI.btnClearSummary);
    bottomBtnLayout->addWidget(m_simSetupUI.btnExportKFile);
    mainVLayout->addLayout(bottomBtnLayout);


    // ==========================================
    // Tab 材料标签页的底部加入“侵蚀(Erosion)设置”
    // ==========================================
    m_simSetupUI.erosionGroup = new QGroupBox("单元侵蚀准则 (*MAT_ADD_EROSION)");
    m_simSetupUI.erosionGroup->setCheckable(true);
    m_simSetupUI.erosionGroup->setChecked(false); // 默认不开启
    QFormLayout* erosionLayout = new QFormLayout(m_simSetupUI.erosionGroup);
    m_simSetupUI.erosionMxeps = new QDoubleSpinBox();
    m_simSetupUI.erosionMxeps->setRange(0.0, 10.0);
    m_simSetupUI.erosionMxeps->setValue(2.0); // 默认失效应变 2.0
    erosionLayout->addRow("最大失效应变 (MXEPS):", m_simSetupUI.erosionMxeps);
    matLayout->addWidget(m_simSetupUI.erosionGroup); // 加到原来的 matLayout 底部

    // ==========================================
    // Tab: 初始条件 (Initial Conditions)
    // ==========================================
    QWidget* icTab = new QWidget();
    QFormLayout* icLayout = new QFormLayout(icTab);

    m_simSetupUI.icEntitySelector = new QComboBox();
    icLayout->addRow("目标实体 (Part):", m_simSetupUI.icEntitySelector);

    m_simSetupUI.icVx = new QDoubleSpinBox(); m_simSetupUI.icVx->setRange(-99999, 99999);
    m_simSetupUI.icVx->setSuffix(" cm/μs");
    m_simSetupUI.icVy = new QDoubleSpinBox(); m_simSetupUI.icVy->setRange(-99999, 99999);
    m_simSetupUI.icVy->setSuffix(" cm/μs");
    m_simSetupUI.icVz = new QDoubleSpinBox(); m_simSetupUI.icVz->setRange(-99999, 99999);
    m_simSetupUI.icVz->setSuffix(" cm/μs");
    icLayout->addRow("X 向初始速度 (Vx):", m_simSetupUI.icVx);
    icLayout->addRow("Y 向初始速度 (Vy):", m_simSetupUI.icVy);
    icLayout->addRow("Z 向初始速度 (Vz):", m_simSetupUI.icVz);

    m_simSetupUI.mainTab->addTab(icTab, "初始条件(IC)");

    m_simSetupUI.btnAddIC = new QPushButton("添加初始速度 (Add IC)");
    icLayout->addWidget(m_simSetupUI.btnAddIC);
    // ==========================================
    // Tab: 接触定义 (Contact)
    // ==========================================
    QWidget* contactTab = new QWidget();
    QFormLayout* contactLayout = new QFormLayout(contactTab);

    m_simSetupUI.contactTypeSelector = new QComboBox();
    m_simSetupUI.contactTypeSelector->addItems({ "*CONTACT_ERODING_SURFACE_TO_SURFACE", "*CONTACT_AUTOMATIC_SINGLE_SURFACE", "*CONTACT_TIED_SURFACE_TO_SURFACE" });
    contactLayout->addRow("接触类型:", m_simSetupUI.contactTypeSelector);

    m_simSetupUI.contactMasterSelector = new QComboBox();
    m_simSetupUI.contactSlaveSelector = new QComboBox();
    contactLayout->addRow("主面实体 (Master):", m_simSetupUI.contactMasterSelector);
    contactLayout->addRow("从面实体 (Slave):", m_simSetupUI.contactSlaveSelector);

    m_simSetupUI.contactFs = new QDoubleSpinBox(); m_simSetupUI.contactFs->setRange(0, 1); m_simSetupUI.contactFs->setValue(0.0);
    m_simSetupUI.contactFd = new QDoubleSpinBox(); m_simSetupUI.contactFd->setRange(0, 1); m_simSetupUI.contactFd->setValue(0.0);
    contactLayout->addRow("静摩擦系数 (FS):", m_simSetupUI.contactFs);
    contactLayout->addRow("动摩擦系数 (FD):", m_simSetupUI.contactFd);

    m_simSetupUI.mainTab->addTab(contactTab, "接触(Contact)");

    m_simSetupUI.btnAddContact = new QPushButton("添加接触定义 (Add Contact)");
    contactLayout->addWidget(m_simSetupUI.btnAddContact);
    // ==========================================
    // Tab: 截面与算法 (Section)
    // ==========================================
    QWidget* sectionTab = new QWidget();
    QFormLayout* sectionLayout = new QFormLayout(sectionTab);

    m_simSetupUI.sectionEntitySelector = new QComboBox();
    sectionLayout->addRow("目标实体 (Part):", m_simSetupUI.sectionEntitySelector);

    m_simSetupUI.sectionTypeSelector = new QComboBox();
    m_simSetupUI.sectionTypeSelector->addItems({ "*SECTION_SOLID", "*SECTION_SHELL" });
    sectionLayout->addRow("单元类型:", m_simSetupUI.sectionTypeSelector);

    m_simSetupUI.sectionElformSelector = new QComboBox();
    m_simSetupUI.sectionElformSelector->addItems({ "1 - 单点积分 (快, 需控制沙漏)", "2 - 全积分 (慢, 精确)", "0 - 默认" });
    m_simSetupUI.sectionElformSelector->setCurrentIndex(0);
    sectionLayout->addRow("单元算法 (ELFORM):", m_simSetupUI.sectionElformSelector);

    m_simSetupUI.mainTab->addTab(sectionTab, "截面(Section)");

    m_simSetupUI.btnAddSection = new QPushButton("添加截面属性 (Add Section)");
    sectionLayout->addWidget(m_simSetupUI.btnAddSection);
    // ==========================================
    // Tab: 控制面板 (Control) 增加高级选项
    // ==========================================

    m_simSetupUI.jobTitleInput = new QLineEdit("Fragment_ignition");
    ctrlLayout->insertRow(0, "项目名称 (TITLE):", m_simSetupUI.jobTitleInput);

    m_simSetupUI.tssfacInput = new QDoubleSpinBox();
    m_simSetupUI.tssfacInput->setRange(0.1, 1.0); m_simSetupUI.tssfacInput->setValue(0.9); m_simSetupUI.tssfacInput->setSingleStep(0.1);
    ctrlLayout->insertRow(1, "时间步缩放 (TSSFAC):", m_simSetupUI.tssfacInput);

    // ==========================================
    // [重写] Tab: 传感器与观测点 (Sensors) - 统一界面
    // ==========================================
    QWidget* sensorTab = new QWidget();
    QVBoxLayout* sensorMainLayout = new QVBoxLayout(sensorTab);

    QFormLayout* sensorLayout = new QFormLayout();

    // 1. 实体选择
    m_simSetupUI.sensorEntitySelector = new QComboBox();
    sensorLayout->addRow("目标实体 (Target):", m_simSetupUI.sensorEntitySelector);

    // 2. 模式切换
    m_simSetupUI.sensorModeSelector = new QComboBox();
    m_simSetupUI.sensorModeSelector->addItems({ "单点观测 (Single Point)", "线性阵列观测 (Line Array)" });
    sensorLayout->addRow("观测模式 (Mode):", m_simSetupUI.sensorModeSelector);

    // 3. 起点 / 单点坐标
    m_simSetupUI.sensorStartX = new QDoubleSpinBox(); m_simSetupUI.sensorStartX->setRange(-99999, 99999);
    m_simSetupUI.sensorStartY = new QDoubleSpinBox(); m_simSetupUI.sensorStartY->setRange(-99999, 99999);
    m_simSetupUI.sensorStartZ = new QDoubleSpinBox(); m_simSetupUI.sensorStartZ->setRange(-99999, 99999);

    QHBoxLayout* startLayout = new QHBoxLayout();
    startLayout->addWidget(m_simSetupUI.sensorStartX); startLayout->addWidget(m_simSetupUI.sensorStartY); startLayout->addWidget(m_simSetupUI.sensorStartZ);
    sensorLayout->addRow("位置/起点 (X, Y, Z):", startLayout);

    // 4. 阵列专属折叠面板 (默认隐藏)
    m_simSetupUI.sensorArrayContainer = new QWidget();
    QFormLayout* arrayLayout = new QFormLayout(m_simSetupUI.sensorArrayContainer);
    arrayLayout->setContentsMargins(0, 0, 0, 0); // 取消边距让它看起来无缝衔接

    m_simSetupUI.sensorEndX = new QDoubleSpinBox(); m_simSetupUI.sensorEndX->setRange(-99999, 99999);
    m_simSetupUI.sensorEndY = new QDoubleSpinBox(); m_simSetupUI.sensorEndY->setRange(-99999, 99999);
    m_simSetupUI.sensorEndZ = new QDoubleSpinBox(); m_simSetupUI.sensorEndZ->setRange(-99999, 99999);

    QHBoxLayout* endLayout = new QHBoxLayout();
    endLayout->addWidget(m_simSetupUI.sensorEndX); endLayout->addWidget(m_simSetupUI.sensorEndY); endLayout->addWidget(m_simSetupUI.sensorEndZ);
    arrayLayout->addRow("终点坐标 (X, Y, Z):", endLayout);

    m_simSetupUI.sensorNumPoints = new QSpinBox();
    m_simSetupUI.sensorNumPoints->setRange(2, 1000);
    m_simSetupUI.sensorNumPoints->setValue(10);
    arrayLayout->addRow("测点数量 (N):", m_simSetupUI.sensorNumPoints);

    sensorLayout->addRow("", m_simSetupUI.sensorArrayContainer);
    m_simSetupUI.sensorArrayContainer->setVisible(false); // 初始状态为单点，隐藏该容器

    // 🌟 核心折叠逻辑：根据下拉框的值，动态显示/隐藏下半部分
    connect(m_simSetupUI.sensorModeSelector, &QComboBox::currentTextChanged, this, [this](const QString& text) {
        m_simSetupUI.sensorArrayContainer->setVisible(text.contains("Line Array"));
        });

    sensorMainLayout->addLayout(sensorLayout);

    // 5. 统一提交按钮
    m_simSetupUI.btnAddSensor = new QPushButton("添加观测点 (Add Sensor)");
    sensorMainLayout->addWidget(m_simSetupUI.btnAddSensor);
    sensorMainLayout->addStretch();

    m_simSetupUI.mainTab->addTab(sensorTab, "测点(Sensors)");

    // 绑定信号
    connect(m_simSetupUI.btnAddSensor, &QPushButton::clicked, this, &MainWindow::handleAddSensor);

    // 连接所有信号
    connect(m_simSetupUI.materialSelector, &QComboBox::currentTextChanged, this, [this](const QString& matType) {
        m_simSetupUI.eosSelector->blockSignals(true); // 防止自动匹配时重复触发绘制
        if (matType == "*MAT_JOHNSON_COOK") m_simSetupUI.eosSelector->setCurrentText("*EOS_GRUNEISEN");
        else if (matType == "*MAT_HIGH_EXPLOSIVE_BURN") m_simSetupUI.eosSelector->setCurrentText("*EOS_JWL");
        else if (matType == "*MAT_NULL") m_simSetupUI.eosSelector->setCurrentText("*EOS_LINEAR_POLYNOMIAL");
        else if (matType == "*MAT_ELASTIC_PLASTIC_HYDRO") m_simSetupUI.eosSelector->setCurrentText("*EOS_IGNITION_AND_GROWTH_OF_REACTION_IN_HE");
        else m_simSetupUI.eosSelector->setCurrentText("None");
        m_simSetupUI.eosSelector->blockSignals(false);

        handleMaterialTypeChanged(matType); // 绘制输入框
        });

    connect(m_simSetupUI.btnAddMaterial, &QPushButton::clicked, this, &MainWindow::handleAddMaterial);
    connect(m_simSetupUI.btnAddContact, &QPushButton::clicked, this, &MainWindow::handleAddContact);
    connect(m_simSetupUI.btnAddIC, &QPushButton::clicked, this, &MainWindow::handleAddIC);
    connect(m_simSetupUI.btnAddSection, &QPushButton::clicked, this, &MainWindow::handleAddSection);
    connect(m_simSetupUI.btnClearSummary, &QPushButton::clicked, this, &MainWindow::handleClearSummary);
    // 这里的 btnExportKFile 连接到你之前写的 handleApplySimulationSettings
    connect(m_simSetupUI.btnExportKFile, &QPushButton::clicked, this, &MainWindow::handleApplySimulationSettings);

    // ==========================================
    //全局控件随窗口自适应缩放 (Fluid Layout)
    // ==========================================

    // 1. 批量解除【所有】输入控件和按钮的宽度锁定
    QList<QWidget*> allWidgets = mainContainer->findChildren<QWidget*>();
    for (QWidget* w : allWidgets) {
        // 利用 qobject_cast 过滤出所有的下拉框、数字框、文本框和按钮
        if (qobject_cast<QComboBox*>(w) ||
            qobject_cast<QDoubleSpinBox*>(w) ||
            qobject_cast<QSpinBox*>(w) ||
            qobject_cast<QLineEdit*>(w) ||
            qobject_cast<QPushButton*>(w)) {

            w->setMinimumWidth(40); // 允许压缩到底线 40 像素
            // 🌟 核心：横向策略设为 Expanding (自动跟随窗口拉伸/压缩)
            // 纵向策略设为 Fixed (高度保持原生尺寸不变)
            w->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Fixed);
        }
    }

    // 2. 加入透明滚动区 (QScrollArea) 
    QScrollArea* mainScrollArea = new QScrollArea();
    mainScrollArea->setWidget(mainContainer);
    mainScrollArea->setWidgetResizable(true);       // 必须开启：内部面板跟随外框灵活伸缩
    mainScrollArea->setFrameShape(QFrame::NoFrame); // 去除自带的凹陷边框

    mainScrollArea->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);

    // 3. 将被滚动区保护的面板放入 Dock 中
    setupDock->setWidget(mainScrollArea);

    addDockWidget(Qt::LeftDockWidgetArea, setupDock);

    // 触发一次初始化
    handleMaterialTypeChanged(m_simSetupUI.materialSelector->currentText());
}

void MainWindow::handleMaterialTypeChanged(const QString& matType) {
    // 1. 清空旧表单
    QLayoutItem* item;
    while ((item = m_simSetupUI.matParamLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_simSetupUI.currentMatInputs.clear();

    // 辅助 Lambda
    auto addParam = [&](const QString& label, const QString& key, double defaultVal, const QString& unit = "") {
        QDoubleSpinBox* box = new QDoubleSpinBox();
        box->setRange(-999999, 999999);
        box->setDecimals(5);
        box->setValue(defaultVal);

        if (!unit.isEmpty()) {
            box->setSuffix(unit);
        }

        m_simSetupUI.matParamLayout->addRow(label, box);
        m_simSetupUI.currentMatInputs[key] = box;
        };

    // 2. 先画出纯材料 (MAT) 的参数
    if (matType == "*MAT_JOHNSON_COOK") {
        addParam("密度 (RO):", "ro", 7.83e-6, " g/cm³");
        addParam("剪切模量 (G):", "g", 77.0, " Mbar");
        addParam("屈服强度 (A):", "a", 0.792, " Mbar");
        addParam("硬化常数 (B):", "b", 0.510, " Mbar");
        addParam("硬化指数 (N):", "n", 0.26);
        addParam("应变率常数 (C):", "c", 0.014);
        addParam("软化指数 (M):", "m", 1.03);
        addParam("熔点 (TMELT):", "tmelt", 1793, " K");
    }
    else if (matType == "*MAT_HIGH_EXPLOSIVE_BURN") {
        addParam("密度 (RO):", "ro", 1.63e-6);
        addParam("爆速 (D):", "d", 6.93);
        addParam("CJ 压力 (PCJ):", "pcj", 21.0);
    }
    else if (matType == "*MAT_NULL") {
        addParam("密度 (RO):", "ro", 1.0e-6);
        addParam("压力截断 (PC):", "pc", -1.0e-6);
        addParam("动力粘度 (MU):", "mu", 0.0);
    }
    else if (matType == "*MAT_ELASTIC_PLASTIC_HYDRO") {
        addParam("密度 (RO):", "ro", 1.712);
        addParam("剪切模量 (G):", "g", 0.0354);
        addParam("屈服强度 (SIGY):", "sigy", 2.0e-4);
        addParam("压力截断 (PC):", "pc", -9.0);
    }

    // 3. 接着读取当前 EOS 选了什么，并在下方画出具体的 EOS 参数！
    QString eosType = m_simSetupUI.eosSelector->currentText();

    if (eosType == "*EOS_GRUNEISEN") {
        addParam("[GRUNEISEN] C (截距):", "gr_c", 0.0);
        addParam("[GRUNEISEN] S1 (斜率):", "gr_s1", 0.0);
        addParam("[GRUNEISEN] GAMAO:", "gr_gamao", 0.0);
    }
    else if (eosType == "*EOS_JWL") {
        addParam("[JWL] A:", "jwl_a", 373.77, " Mbar");
        addParam("[JWL] B:", "jwl_b", 3.747, " Mbar");
        addParam("[JWL] R1:", "jwl_r1", 4.15);
        addParam("[JWL] R2:", "jwl_r2", 0.90);
        addParam("[JWL] OMEGA:", "jwl_omega", 0.35);
        addParam("[JWL] E0:", "jwl_e0", 0.06, " Mbar");
    }
    else if (eosType == "*EOS_LINEAR_POLYNOMIAL") {
        addParam("[LINEAR] C0:", "lp_c0", 0.0);
        addParam("[LINEAR] C1:", "lp_c1", 0.0);
        addParam("[LINEAR] C2:", "lp_c2", 0.0);
        addParam("[LINEAR] C3:", "lp_c3", 0.0);
        addParam("[LINEAR] C4:", "lp_c4", 0.4);
        addParam("[LINEAR] C5:", "lp_c5", 0.4);
        addParam("[LINEAR] C6:", "lp_c6", 0.0);
        addParam("[LINEAR] E0:", "lp_e0", 2.5e-6);
        addParam("[LINEAR] V0:", "lp_v0", 1.0);
    }
    else if (eosType == "*EOS_TILLOTSON") {
        addParam("[TILLOTSON] A:", "til_a", 0.0);
        addParam("[TILLOTSON] B:", "til_b", 0.0);
        addParam("[TILLOTSON] OMEGA:", "til_omega", 0.0);
        addParam("[TILLOTSON] E0:", "til_e0", 0.0);
        addParam("[TILLOTSON] V0:", "til_v0", 1.0);
        addParam("[TILLOTSON] ALPHA:", "til_alpha", 0.0);
        addParam("[TILLOTSON] BETA:", "til_beta", 0.0);
    }
    else if (eosType == "*EOS_IGNITION_AND_GROWTH_OF_REACTION_IN_HE") {
        // --- 1. 未反应固体炸药的 JWL 参数 ---
        addParam("[I&G] 未反应 A:", "ig_a", 5.242);
        addParam("[I&G] 未反应 B:", "ig_b", 0.07678);
        addParam("[I&G] 未反应 R1:", "ig_r1", 778.1);
        addParam("[I&G] 未反应 R2:", "ig_r2", -0.05031);
        addParam("[I&G] 未反应 OMEGA (G):", "ig_g", 5.0e-6);

        // --- 2. 完全反应后爆炸产物的 JWL 参数 ---
        addParam("[I&G] 产物 XP1 (A):", "ig_xp1", 2.84999);
        addParam("[I&G] 产物 XP2 (B):", "ig_xp2", 0.0);
        addParam("[I&G] 产物 R3 (R1):", "ig_r3", 2.223e-5);
        addParam("[I&G] 产物 R5 (R2):", "ig_r5", 11.3);
        addParam("[I&G] 产物 R6 (OMEGA):", "ig_r6", 1.13);

        // --- 3. 反应速率：点火项 (Ignition) ---
        addParam("[I&G] 点火频率 (FREQ):", "ig_freq", 4.0);
        addParam("[I&G] 最大点火份额 (FMXIG):", "ig_fmxig", 0.022);
        addParam("[I&G] 临界压缩度 (CCRIT):", "ig_ccrit", 0.0367);
        addParam("[I&G] 压缩指数 (EETAL):", "ig_eetal", 7.0);

        // --- 4. 反应速率：缓慢生长项 (Growth 1) ---
        addParam("[I&G] 生长系数1 (GROW1):", "ig_grow1", 120.0);
        addParam("[I&G] 压力指数1 (EM):", "ig_em", 2.0);
        addParam("[I&G] 反应份额指数 (AR1):", "ig_ar1", 0.333);
        addParam("[I&G] 未反应份额指数 (ES1):", "ig_es1", 0.667);
        addParam("[I&G] 最大生长份额 (FMXGR):", "ig_fmxgr", 0.7);

        // --- 5. 反应速率：快速爆轰项 (Growth 2) ---
        addParam("[I&G] 生长系数2 (GROW2):", "ig_grow2", 1000.0);
        addParam("[I&G] 反应份额指数 (AR2):", "ig_ar2", 1.0);
        addParam("[I&G] 未反应份额指数 (ES2):", "ig_es2", 0.222);
        addParam("[I&G] 压力指数2 (EN):", "ig_en", 3.0);
        addParam("[I&G] 最小爆轰份额 (FMNGR):", "ig_fmngr", 0.0);

        // --- 6. 热力学与能量守恒 ---
        addParam("[I&G] 反应热/爆炸能 (ENQ):", "ig_enq", 0.085);
        addParam("[I&G] 初始温度 (TMP0):", "ig_tmp0", 298.0);
        addParam("[I&G] 产物比热容 (CVP):", "ig_cvp", 10.0);
        addParam("[I&G] 反应物比热容 (CVR):", "ig_cvr", 24.78);
        addParam("[I&G] 比例系数 (FRER):", "ig_frer", 1.1);
    }
}

void MainWindow::handleApplySimulationSettings() {
    if (m_workingDirectory.isEmpty()) {
        QMessageBox::warning(this, "警告", "请先设置工作目录！");
        return;
    }

    for (int i = 0; i < m_simSetupUI.entitySelector->count(); ++i) {
        getOrCreatePart(m_simSetupUI.entitySelector->itemText(i));
    }

    QString jobTitle = m_simSetupUI.jobTitleInput->text();
    if (jobTitle.isEmpty()) jobTitle = "Simulation_Job";

    QString meshFileName = jobTitle + "_mesh.k";
    QString controlFileName = jobTitle + "_control.k";

    // 1. 导出几何网格 (保留你的原有代码)
    exportToKFile(QDir(m_workingDirectory).filePath(meshFileName));

    // 2. 导出控制文件
    QFile controlFile(QDir(m_workingDirectory).filePath(controlFileName));
    if (controlFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
        QTextStream out(&controlFile);

        // 写入固定头部
        out << "*KEYWORD MEMORY=399999999\n";
        out << "*TITLE\n" << jobTitle << "\n";
        out << "*INCLUDE\n" << meshFileName << "\n"; // Include网格

        // 🌟 核心点 1：把全局控制卡丢给管家（读取时间步、结束时间等）
        m_deck.addCard(std::make_shared<GlobalControlCard>(
            m_simSetupUI.endtimeInput->value(),
            m_simSetupUI.tssfacInput->value(),
            m_simSetupUI.d3plotFreqInput->value()
        ));

        // 🌟 核心点 2：一键多态序列化！
        // m_deck 会遍历内部所有的 shared_ptr<KeywordCard>，挨个调用它们自己的 to_string()
        out << QString::fromStdString(m_deck.generateDeck());

        out << "*END\n";
        controlFile.close();
    }

    QMessageBox::information(this, "导出成功", "仿真文件打包成功！控制文件与网格文件已分离。");
}

// ==========================================
// 设置工作目录
// ==========================================
void MainWindow::onSetWorkingDirectory() {
    // 弹出文件夹选择对话框
    QString dir = QFileDialog::getExistingDirectory(this,
        tr("选择工作目录 (Select Working Directory)"),
        m_workingDirectory,
        QFileDialog::ShowDirsOnly | QFileDialog::DontResolveSymlinks);

    // 如果用户没有点击取消
    if (!dir.isEmpty()) {
        m_workingDirectory = dir; // 更新路径

        // 在底部的命令行/日志窗口打印提示
        logCommand("System", "工作目录已设置为: " + m_workingDirectory);

     }
}

void MainWindow::updateAllEntitySelectors() {
    // 提前保存当前选择的文本，防止刷新后用户的选择丢失
    QString curMat = m_simSetupUI.entitySelector->currentText();
    QString curIc = m_simSetupUI.icEntitySelector->currentText();
    QString curMaster = m_simSetupUI.contactMasterSelector->currentText();
    QString curSlave = m_simSetupUI.contactSlaveSelector->currentText();
    QString curSec = m_simSetupUI.sectionEntitySelector->currentText();

    // 清空所有框
    m_simSetupUI.entitySelector->clear();
    m_simSetupUI.icEntitySelector->clear();
    m_simSetupUI.contactMasterSelector->clear();
    m_simSetupUI.contactSlaveSelector->clear();
    m_simSetupUI.sectionEntitySelector->clear();
    m_simSetupUI.sensorEntitySelector->clear();

    // 加载所有实体
    const auto& allEntities = m_repository.getAllEntities();
    for (const auto& pair : allEntities) {
        QString name = pair.first;

        m_simSetupUI.entitySelector->addItem(name);
        m_simSetupUI.icEntitySelector->addItem(name);
        m_simSetupUI.contactMasterSelector->addItem(name);
        m_simSetupUI.contactSlaveSelector->addItem(name);
        m_simSetupUI.sectionEntitySelector->addItem(name);
        m_simSetupUI.sensorEntitySelector->addItem(name);
    }

    // 尝试恢复之前的选择
    m_simSetupUI.entitySelector->setCurrentText(curMat);
    m_simSetupUI.icEntitySelector->setCurrentText(curIc);
    m_simSetupUI.contactMasterSelector->setCurrentText(curMaster);
    m_simSetupUI.contactSlaveSelector->setCurrentText(curSlave);
    m_simSetupUI.sectionEntitySelector->setCurrentText(curSec);

    QString curSensor = m_simSetupUI.sensorEntitySelector->currentText();
    m_simSetupUI.sensorEntitySelector->setCurrentText(curSensor);
}

void MainWindow::handleAddMaterial() {
    QString target = m_simSetupUI.entitySelector->currentText();
    if (target.isEmpty()) return;

    auto part = getOrCreatePart(target);
    QString matType = m_simSetupUI.materialSelector->currentText().remove("*MAT_");
    QString eosTypeStr = m_simSetupUI.eosSelector->currentText();

    // 1. 将前端动态输入全部抓取为字典
    std::map<std::string, double> paramDict;
    for (auto it = m_simSetupUI.currentMatInputs.begin(); it != m_simSetupUI.currentMatInputs.end(); ++it) {
        paramDict[it.key().toStdString()] = it.value()->value();
    }
    if (m_simSetupUI.erosionGroup->isChecked()) {
        paramDict["mxeps"] = m_simSetupUI.erosionMxeps->value();
    }

    // 2. 实例化材料卡片并存入 Deck
    auto matCard = std::make_shared<MaterialCard>(part->pid, matType.toStdString(), paramDict);
    m_deck.addCard(matCard);

    // 3. 动态解析并挂载 EOS 卡片
    std::shared_ptr<EOSCard> eosCard = nullptr;
    if (eosTypeStr != "None") {
        QString cleanEos = eosTypeStr;
        cleanEos.remove("*EOS_"); // 变成 "GRUNEISEN", "JWL" 等

        eosCard = std::make_shared<EOSCard>(part->pid, cleanEos.toStdString(), paramDict);
        m_deck.addCard(eosCard);
        part->eosid = part->pid; // 绑定到实体
    }
    else {
        part->eosid = 0; // 无 EOS
    }

    // 4. 生成右侧列表文字
    QString summary = eosCard ? QString("[材料+EOS] 实体:%1 | %2 + %3").arg(target).arg(matType).arg(eosTypeStr.remove("*EOS_"))
        : QString("[材料] 实体:%1 | %2").arg(target).arg(matType);

    // 5. 【核心修改】将卡片指针地址悄悄绑定到列表项 (UserRole隐藏域)
    QListWidgetItem* item = new QListWidgetItem(summary);

    // 绑定 MAT 卡片地址
    item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(matCard.get())));

    // 如果有 EOS，把 EOS 卡片地址绑在备用域 UserRole + 2
    if (eosCard) {
        item->setData(Qt::UserRole + 2, QVariant::fromValue(reinterpret_cast<quintptr>(eosCard.get())));
    }
    item->setData(Qt::UserRole + 1, "CARD"); // 标记类型为实体卡片

    m_simSetupUI.setupSummaryList->addItem(item);
}

void MainWindow::handleAddContact() {
    QString type = m_simSetupUI.contactTypeSelector->currentText().remove("*CONTACT_"); // 简化显示名称
    QString master = m_simSetupUI.contactMasterSelector->currentText();
    QString slave = m_simSetupUI.contactSlaveSelector->currentText();

    QString summary = QString("[接触] %1 | 主面: %2 | 从面: %3").arg(type).arg(master).arg(slave);

    // 【修改】仅作显示，标记为 OTHER
    QListWidgetItem* item = new QListWidgetItem(summary);
    item->setData(Qt::UserRole + 1, "OTHER");

    m_simSetupUI.setupSummaryList->addItem(item);
}

void MainWindow::handleAddIC() {
    QString target = m_simSetupUI.icEntitySelector->currentText();
    if (target.isEmpty()) return;

    auto part = getOrCreatePart(target); // 获取实体对应的 Part 指针

    // 1. 实例化初始速度卡片
    auto icCard = std::make_shared<InitialVelocityGenerationCard>(
        part->pid,
        m_simSetupUI.icVx->value(),
        m_simSetupUI.icVy->value(),
        m_simSetupUI.icVz->value()
    );
    m_deck.addCard(icCard);

    // 2. 生成右侧列表文字
    QString summary = QString("[初始速度] 实体: %1 | V=(%2, %3, %4)")
        .arg(target).arg(m_simSetupUI.icVx->value()).arg(m_simSetupUI.icVy->value()).arg(m_simSetupUI.icVz->value());

    // 3. 【核心修改】将卡片指针地址绑定到列表项
    QListWidgetItem* item = new QListWidgetItem(summary);
    item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(icCard.get())));
    item->setData(Qt::UserRole + 1, "CARD");

    m_simSetupUI.setupSummaryList->addItem(item);
}

void MainWindow::handleAddSection() {
    QString target = m_simSetupUI.sectionEntitySelector->currentText();
    if (target.isEmpty()) return;

    // 1. 获取目标实体的底层指针
    auto part = getOrCreatePart(target);

    // 2. 获取截面类型和算法
    QString type = m_simSetupUI.sectionTypeSelector->currentText().remove("*SECTION_");
    QString elformStr = m_simSetupUI.sectionElformSelector->currentText();
    int elform = elformStr.split(" ").first().toInt();

    // 3. 实例化截面卡片
    auto secCard = std::make_shared<SectionCard>(part->pid, type.toStdString(), elform);
    m_deck.addCard(secCard);
    part->secid = part->pid; // 绑定实体

    // 4. 生成右侧列表文字
    QString summary = QString("[截面] 实体: %1 | 类型: %2 | 算法: ELFORM=%3").arg(target).arg(type).arg(elform);

    // 5. 【核心修改】将卡片指针地址绑定到列表项
    QListWidgetItem* item = new QListWidgetItem(summary);
    item->setData(Qt::UserRole, QVariant::fromValue(reinterpret_cast<quintptr>(secCard.get())));
    item->setData(Qt::UserRole + 1, "CARD");

    m_simSetupUI.setupSummaryList->addItem(item);
}

void MainWindow::handleClearSummary() {
    // 1. 清空前端显示列表
    m_simSetupUI.setupSummaryList->clear();

    // 2. 清空实体指针映射字典
    m_entityParts.clear();

    m_deck.clear();
}

// 在 mainwindow.cpp 空白处添加
std::shared_ptr<PartCard> MainWindow::getOrCreatePart(const QString& entityName) {
    if (!m_entityParts.contains(entityName)) {
        
        // 直接使用当前已分配实体的总数 + 1 作为全局唯一 PID。
        
        int pid = m_entityParts.size() + 1;

        auto part = std::make_shared<PartCard>(pid, entityName.toStdString());
        m_deck.addCard(part);
        m_entityParts[entityName] = part;
    }
    return m_entityParts[entityName];
}

void MainWindow::loadMaterialPresetsFromJson() {
    m_simSetupUI.presetSelector->clear();
    m_simSetupUI.presetSelector->addItem("None (自定义)");

    // 🌟 智能路径查找逻辑：适配不同的运行环境和编译目录
    QString jsonPath;
    QString relativePath = "/src/core/materials/material_repo.json";

    // 1. 尝试直接在当前工作目录下寻找 (适用于在工程根目录直接运行)
    QFile file("." + relativePath);

    if (!file.exists()) {
        // 2. 尝试在 exe 所在目录往上一层找 (适用于常见的 build/ 目录)
        file.setFileName(QCoreApplication::applicationDirPath() + "/.." + relativePath);
    }
    if (!file.exists()) {
        // 3. 尝试在 exe 所在目录往上两层找 (适用于 build/debug/ 目录)
        file.setFileName(QCoreApplication::applicationDirPath() + "/../.." + relativePath);
    }

    // 如果还是打不开，报错并在命令行提示
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        logCommand("System Error", "未找到 material_repo.json，请检查 src/core/materials/ 路径！材料预设功能暂时禁用。");
        return;
    }

    // 读取并解析 JSON 文件
    QByteArray data = file.readAll();
    file.close();

    QJsonDocument doc = QJsonDocument::fromJson(data);
    if (doc.isNull() || !doc.isObject()) {
        logCommand("System Error", "material_repo.json 格式错误，无法解析！");
        return;
    }

    QJsonArray presetsArray = doc.object()["presets"].toArray();

    // 遍历 JSON 数组，将其存入字典并添加到下拉框
    for (int i = 0; i < presetsArray.size(); ++i) {
        QJsonObject obj = presetsArray[i].toObject();
        QString name = obj["name"].toString();

        MaterialPreset preset;
        preset.matType = obj["matType"].toString();
        preset.eosType = obj["eosType"].toString();
        preset.source = obj["source"].toString();

        QJsonObject paramsObj = obj["params"].toObject();
        for (auto it = paramsObj.begin(); it != paramsObj.end(); ++it) {
            preset.params[it.key()] = it.value().toDouble();
        }

        m_materialPresets[name] = preset;
        m_simSetupUI.presetSelector->addItem(name);
    }

    logCommand("System", QString("成功从 material_repo.json 加载 %1 种材料预设。").arg(m_materialPresets.size()));
}

void MainWindow::handlePresetChanged(const QString& presetName) {
    if (presetName == "None (自定义)" || !m_materialPresets.contains(presetName)) {
        return;
    }

    // 从我们解析好的内存字典中拿出预设数据
    const MaterialPreset& preset = m_materialPresets[presetName];

    // 1. 自动切换材料与EOS下拉框（屏蔽信号防死循环）
    m_simSetupUI.materialSelector->blockSignals(true);
    m_simSetupUI.eosSelector->blockSignals(true);

    m_simSetupUI.materialSelector->setCurrentText(preset.matType);
    m_simSetupUI.eosSelector->setCurrentText(preset.eosType);

    m_simSetupUI.materialSelector->blockSignals(false);
    m_simSetupUI.eosSelector->blockSignals(false);

    // 2. 强制触发一次界面重绘，生成对应材料的几十个空白输入框
    handleMaterialTypeChanged(preset.matType);

    // 3. ✨ 核心：无脑遍历匹配！根据 JSON 里的 Key，直接把值塞进对应 UI 输入框
    for (auto it = preset.params.begin(); it != preset.params.end(); ++it) {
        if (m_simSetupUI.currentMatInputs.contains(it.key())) {
            m_simSetupUI.currentMatInputs[it.key()]->setValue(it.value());
        }
    }

    logCommand("Material Preset", QString("已加载预设 [%1]. 数据来源文献: %2").arg(presetName).arg(preset.source));
}

void MainWindow::handleAddSensor() {
    QString target = m_simSetupUI.sensorEntitySelector->currentText();
    if (target.isEmpty()) return;

    MeshEntity* entity = m_repository.getMutableEntity(target);
    if (!entity || entity->nodes.size() < 2) return;

    // 获取当前模式是否为阵列
    bool isArray = m_simSetupUI.sensorModeSelector->currentText().contains("Line Array");

    // 提取通用起点
    double sx = m_simSetupUI.sensorStartX->value();
    double sy = m_simSetupUI.sensorStartY->value();
    double sz = m_simSetupUI.sensorStartZ->value();

    if (!isArray) {
        // -----------------------------
        // 分支 A: 单点测点逻辑
        // -----------------------------
        int closestIdx = -1;
        double min_dist = 1e9;

        for (size_t i = 1; i < entity->nodes.size(); ++i) {
            double dx = entity->nodes[i].pos.x() - sx;
            double dy = entity->nodes[i].pos.y() - sy;
            double dz = entity->nodes[i].pos.z() - sz;
            double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
            if (dist < min_dist) { min_dist = dist; closestIdx = i; }
        }

        if (closestIdx != -1 && !m_sensorNodes[target].contains(closestIdx)) {
            m_sensorNodes[target].append(closestIdx);

            QString summary = QString("[测点] 实体:%1 | 单点:(%2,%3,%4) -> 吸附误差:%5")
                .arg(target).arg(sx).arg(sy).arg(sz).arg(min_dist, 0, 'f', 4);
            if (m_simSetupUI.setupSummaryList) m_simSetupUI.setupSummaryList->addItem(summary);
            logCommand("Sensor", summary);
        }
    }
    else {
        // -----------------------------
        // 分支 B: 线性阵列测点逻辑
        // -----------------------------
        double ex = m_simSetupUI.sensorEndX->value();
        double ey = m_simSetupUI.sensorEndY->value();
        double ez = m_simSetupUI.sensorEndZ->value();
        int count = m_simSetupUI.sensorNumPoints->value();
        int successCount = 0;

        for (int k = 0; k < count; ++k) {
            double t = static_cast<double>(k) / (count - 1);
            double tx = sx + t * (ex - sx);
            double ty = sy + t * (ey - sy);
            double tz = sz + t * (ez - sz);

            int closestIdx = -1;
            double min_dist = 1e9;
            for (size_t i = 1; i < entity->nodes.size(); ++i) {
                double dx = entity->nodes[i].pos.x() - tx;
                double dy = entity->nodes[i].pos.y() - ty;
                double dz = entity->nodes[i].pos.z() - tz;
                double dist = std::sqrt(dx * dx + dy * dy + dz * dz);
                if (dist < min_dist) { min_dist = dist; closestIdx = i; }
            }

            if (closestIdx != -1 && !m_sensorNodes[target].contains(closestIdx)) {
                m_sensorNodes[target].append(closestIdx);
                successCount++;
            }
        }

        QString summary = QString("[阵列测点] 实体:%1 | %2个点 | 从(%3,%4,%5)到(%6,%7,%8)")
            .arg(target).arg(successCount).arg(sx).arg(sy).arg(sz).arg(ex).arg(ey).arg(ez);
        if (m_simSetupUI.setupSummaryList) m_simSetupUI.setupSummaryList->addItem(summary);
        logCommand("Sensor", summary);
    }
}

// ==========================================
// 界面搭建：后处理与监控模块
// ==========================================
void MainWindow::setupPostProcessUI() {
    QVBoxLayout* mainLayout = new QVBoxLayout(postProcessWidget);

    // --- 模块 A: 工况提交区 ---
    QGroupBox* submitGroup = new QGroupBox("工况提交 (Job Submission)");
    QFormLayout* submitLayout = new QFormLayout(submitGroup);

    kFilePathEdit = new QLineEdit();
    QPushButton* btnBrowseK = new QPushButton("浏览...");
    QHBoxLayout* kLayout = new QHBoxLayout();
    kLayout->addWidget(kFilePathEdit); kLayout->addWidget(btnBrowseK);
    submitLayout->addRow("控制文件 (.k):", kLayout);

    solverPathEdit = new QLineEdit();
    solverPathEdit->setPlaceholderText("例如: C:/LSDYNA/ls-dyna_smp_d_R11_0_winx64.exe");
    QPushButton* btnBrowseSolver = new QPushButton("浏览...");
    QHBoxLayout* solverLayout = new QHBoxLayout();
    solverLayout->addWidget(solverPathEdit); solverLayout->addWidget(btnBrowseSolver);
    submitLayout->addRow("求解器路径 (EXE):", solverLayout);

    cpuCoresSpin = new QSpinBox();
    cpuCoresSpin->setRange(1, 128);
    cpuCoresSpin->setValue(4); // 默认 4 核计算
    submitLayout->addRow("计算核心数 (NCPU):", cpuCoresSpin);

    btnRunSolver = new QPushButton("▶ 开始求解 (Run LS-DYNA)");
    btnRunSolver->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;");
    btnStopSolver = new QPushButton("■ 终止计算 (Kill)");
    btnStopSolver->setEnabled(false); // 默认不可点，运行后解锁

    QHBoxLayout* btnLayout = new QHBoxLayout();
    btnLayout->addWidget(btnRunSolver);
    btnLayout->addWidget(btnStopSolver);
    submitLayout->addRow("", btnLayout);

    mainLayout->addWidget(submitGroup);

    // --- 模块 B: 计算监控区 ---
    QGroupBox* monitorGroup = new QGroupBox("计算监控 (Solver Console)");
    QVBoxLayout* monitorLayout = new QVBoxLayout(monitorGroup);

    solverConsole = new QTextEdit();
    solverConsole->setReadOnly(true);
    solverConsole->setStyleSheet("background-color: #1E1E1E; color: #00FF00; font-family: Consolas;"); // 黑底绿字，黑客风
    monitorLayout->addWidget(solverConsole);

    mainLayout->addWidget(monitorGroup, 1); // 1表示让控制台占据主要拉伸空间

    // --- 模块 C: d3plot 后处理接口区 ---
    QGroupBox* postGroup = new QGroupBox("结果处理 (d3plot Visualization)");
    QHBoxLayout* postLayout = new QHBoxLayout(postGroup);

    btnOpenFolder = new QPushButton("打开结果所在目录");
    btnLaunchD3plot = new QPushButton("加载 d3plot 进行分析 (开发中...)");
    postLayout->addWidget(btnOpenFolder);
    postLayout->addWidget(btnLaunchD3plot);

    mainLayout->addWidget(postGroup);

    // 绑定按钮信号
    connect(btnBrowseK, &QPushButton::clicked, this, &MainWindow::browseKFile);
    connect(btnBrowseSolver, &QPushButton::clicked, this, &MainWindow::browseSolver);
    connect(btnRunSolver, &QPushButton::clicked, this, &MainWindow::startCalculation);
    connect(btnStopSolver, &QPushButton::clicked, this, &MainWindow::stopCalculation);
    // 预留的后处理信号
    connect(btnOpenFolder, &QPushButton::clicked, this, &MainWindow::openResultFolder);
    connect(btnLaunchD3plot, &QPushButton::clicked, this, &MainWindow::launchPostProcessor);
}

// ==========================================
// 逻辑实现：求解器控制与日志读取
// ==========================================
void MainWindow::startCalculation() {
    QString kFile = kFilePathEdit->text();
    QString solver = solverPathEdit->text();

    if (kFile.isEmpty() || solver.isEmpty()) {
        solverConsole->append("<b><font color='red'>[错误] 请先选择 .k 文件和求解器路径！</font></b>");
        return;
    }

    // 拼接 LS-DYNA 命令行参数： i=xxx.k ncpu=4 memory=100m
    QStringList arguments;
    arguments << QString("i=%1").arg(kFile);
    arguments << QString("ncpu=%1").arg(cpuCoresSpin->value());
    arguments << "memory=200m"; // 预设内存，可根据需求提取为UI输入

    // 设置工作目录为 .k 文件所在的目录，这样 d3plot 就会生成在那里
    QFileInfo kFileInfo(kFile);
    m_solverProcess->setWorkingDirectory(kFileInfo.absolutePath());

    solverConsole->clear();
    solverConsole->append(QString("<b><font color='yellow'>[系统] 正在启动求解器...</font></b>"));
    solverConsole->append(QString("执行命令: %1 %2").arg(solver).arg(arguments.join(" ")));
    solverConsole->append("--------------------------------------------------");

    // 启动进程
    m_solverProcess->start(solver, arguments);

    // 更新界面状态
    btnRunSolver->setEnabled(false);
    btnStopSolver->setEnabled(true);
}

void MainWindow::readSolverOutput() {
    // 读取来自 LS-DYNA 的控制台输出
    QByteArray output = m_solverProcess->readAllStandardOutput();
    QByteArray error = m_solverProcess->readAllStandardError();

    if (!output.isEmpty()) {
        solverConsole->insertPlainText(QString::fromLocal8Bit(output));
    }
    if (!error.isEmpty()) {
        solverConsole->insertPlainText(QString::fromLocal8Bit(error));
    }

    // 自动滚动到最底部
    QScrollBar* sb = solverConsole->verticalScrollBar();
    sb->setValue(sb->maximum());
}

void MainWindow::stopCalculation() {
    if (m_solverProcess->state() == QProcess::Running) {
        m_solverProcess->kill(); // 强制终止进程
        solverConsole->append("<b><font color='red'>[系统] 收到用户指令，计算已强行终止！</font></b>");
    }
}

void MainWindow::handleSolverFinished(int exitCode, QProcess::ExitStatus exitStatus) {
    btnRunSolver->setEnabled(true);
    btnStopSolver->setEnabled(false);

    if (exitStatus == QProcess::NormalExit && exitCode == 0) {
        solverConsole->append("--------------------------------------------------");
        solverConsole->append("<b><font color='cyan'>[系统] LS-DYNA 求解正常完成 (Normal Termination)！</font></b>");
    }
    else {
        solverConsole->append("--------------------------------------------------");
        solverConsole->append("<b><font color='red'>[系统] 求解异常退出或被终止 (Error Termination)。</font></b>");
    }
}

// ==========================================
// 后处理界面：按钮槽函数实现
// ==========================================

// 1. 浏览选择 K 文件
void MainWindow::browseKFile() {
    // 弹出文件选择框，只过滤 .k 或 .key 文件
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择 LS-DYNA 控制文件",
        m_workingDirectory, // 默认打开之前的工作目录
        "LS-DYNA 文件 (*.k *.key);;所有文件 (*.*)"
    );

    if (!fileName.isEmpty()) {
        kFilePathEdit->setText(fileName);
        // 同步更新系统的工作目录为该文件所在的目录
        m_workingDirectory = QFileInfo(fileName).absolutePath();
        logCommand("System", "已加载控制文件: " + fileName);
    }
}

// 2. 浏览选择求解器
void MainWindow::browseSolver() {
    // 弹出文件选择框，只过滤 .exe 可执行文件
    QString fileName = QFileDialog::getOpenFileName(
        this,
        "选择 LS-DYNA 求解器程序",
        "C:/", // 默认从 C 盘开始找
        "可执行文件 (*.exe)"
    );

    if (!fileName.isEmpty()) {
        solverPathEdit->setText(fileName);
        logCommand("System", "已手动配置求解器路径: " + fileName);
    }
}

// 3. 一键打开结果文件夹
void MainWindow::openResultFolder() {
    QString kFile = kFilePathEdit->text();
    if (kFile.isEmpty()) {
        // 如果用户还没选择文件，弹出警告并记录日志
        logCommand("Warning", "请先选择一个 .k 文件！");
        return;
    }

    // 提取 .k 文件所在的纯目录路径
    QString dirPath = QFileInfo(kFile).absolutePath();

    // 调用操作系统的资源管理器打开这个路径
    QDesktopServices::openUrl(QUrl::fromLocalFile(dirPath));
    logCommand("System", "已打开结果所在目录: " + dirPath);
}

// 4. 加载 d3plot 后处理程序 (占位接口)
void MainWindow::launchPostProcessor() {
    // 目前处于开发阶段，使用信息弹窗占位
    QMessageBox::information(
        this,
        "后处理接口",
        "d3plot 可视化接口模块正在开发中...\n\n后续可以在这里唤起官方的 LS-PrePost 软件，或者集成我们自己的 OpenGL 后处理渲染器！"
    );
}

// ==========================================
// 右键菜单与预览功能 (全新增)
// ==========================================
void MainWindow::showSummaryContextMenu(const QPoint& pos) {
    QListWidgetItem* item = m_simSetupUI.setupSummaryList->itemAt(pos);
    if (!item) return;

    QMenu menu(this);
    QAction* viewAct = menu.addAction("查看关键字格式 (View K-File)");
    QAction* delAct = menu.addAction("删除该项 (Delete)");

    QAction* selected = menu.exec(m_simSetupUI.setupSummaryList->mapToGlobal(pos));
    if (!selected) return;

    // 提取隐藏在 item 中的卡片指针和类型
    QString type = item->data(Qt::UserRole + 1).toString();
    quintptr ptrVal = item->data(Qt::UserRole).value<quintptr>();
    KeywordCard* cardPtr = reinterpret_cast<KeywordCard*>(ptrVal);

    if (selected == viewAct) {
        if (type == "CARD" && cardPtr) {
            // 弹出一个黑客风代码预览框
            QDialog dialog(this);
            dialog.setWindowTitle("LS-DYNA 关键字预览");
            dialog.resize(600, 400);

            QVBoxLayout layout(&dialog);
            QTextEdit textEdit;
            textEdit.setReadOnly(true);
            textEdit.setStyleSheet("background-color: #1E1E1E; color: #D4D4D4; font-family: Consolas; font-size: 11pt;");

            // 解析主卡片
            QString kText = QString::fromStdString(cardPtr->to_string());

            // 如果有绑定的 EOS 附加卡片，拼接在一起显示
            if (item->data(Qt::UserRole + 2).isValid()) {
                quintptr eosPtrVal = item->data(Qt::UserRole + 2).value<quintptr>();
                KeywordCard* eosCardPtr = reinterpret_cast<KeywordCard*>(eosPtrVal);
                if (eosCardPtr) {
                    kText += "\n" + QString::fromStdString(eosCardPtr->to_string());
                }
            }

            textEdit.setPlainText(kText);
            layout.addWidget(&textEdit);
            dialog.exec();
        }
        else {
            QMessageBox::information(this, "提示", "该项暂未生成标准卡片(如测点、接触等将在导出时动态生成)。");
        }
    }
    else if (selected == delAct) {
        if (type == "CARD" && cardPtr) {
            // 从大管家中注销这张主卡片
            m_deck.removeCard(cardPtr);

            // 如果有绑定的 EOS 卡片，一并注销
            if (item->data(Qt::UserRole + 2).isValid()) {
                quintptr eosPtrVal = item->data(Qt::UserRole + 2).value<quintptr>();
                KeywordCard* eosCardPtr = reinterpret_cast<KeywordCard*>(eosPtrVal);
                if (eosCardPtr) {
                    m_deck.removeCard(eosCardPtr);
                }
            }
        }
        // 清理 UI 列表项
        delete item;
        logCommand("System", "已撤销指定的仿真参数配置。");
    }
}