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

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent) {

    
        glWidget = new GLWidget(this);
        setCentralWidget(glWidget);
        glWidget->setRepository(&m_repository); // ptr to the entities repo

        startMeshing();
        // 连接绘图完成信号
        connect(glWidget, &GLWidget::drawingComplete, this, &MainWindow::onDrawingComplete);

        // 初始化菜单栏
        createMenuBar();

        // 初始化工具栏
        createToolBars();

        // 初始化状态栏
        createStatusBar();

        // 初始化停靠窗口
        createDockWidgets();

        // 初始化命令行
        createCommandLine();

        createSimulationSetupDock();

        setWindowTitle("SafetyMunitionTestPlatform");
        resize(1024, 768);

        //clear()
        clean();
        
        m_meshManager = new MeshManager(this);

        // 核心：当网格文件准备好后，自动触发 parseMeshFile 进行解析和渲染
        connect(m_meshManager, &MeshManager::meshReady, this, &MainWindow::parseMeshFile);

        // 错误处理：如果 Gmsh 报错，打印到日志
        connect(m_meshManager, &MeshManager::errorOccurred, this, [this](QString msg) {
            logCommand("Error", msg);
            });
        //初始化默认工作目录为当前程序运行的目录
        m_workingDirectory = QDir::currentPath();
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
    // 文件工具栏
    fileToolBar = addToolBar("File");
    fileToolBar->addAction("New");
    fileToolBar->addAction("Open");
    fileToolBar->addAction("Save");

    // 编辑工具栏
    editToolBar = addToolBar("Edit");
    editToolBar->addAction("Cut");
    editToolBar->addAction("Copy");
    editToolBar->addAction("Paste");

    // 视图工具栏
    viewToolBar = addToolBar("View");
    viewToolBar->addAction("Zoom In");
    viewToolBar->addAction("Zoom Out");
    viewToolBar->addAction("Reset View");
}

void MainWindow::createStatusBar() {
    statusBar = new QStatusBar(this);
    setStatusBar(statusBar);
    QLabel *coordLabel = new QLabel("X: 0, Y: 0, Z: 0", this);
    statusBar->addPermanentWidget(coordLabel);
    statusBar->showMessage("Ready");
}

void MainWindow::createDockWidgets() {
    // 属性停靠窗口
    propertiesDock = new QDockWidget("Properties", this);
    propertiesDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    propertiesEditor = new QTextEdit();
    propertiesEditor->setPlainText("Object Properties:\n- Color: Red\n- Size: 1x1x1");
    propertiesDock->setWidget(propertiesEditor);
    propertiesDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::RightDockWidgetArea, propertiesDock);

    // 图层停靠窗口
    layersDock = new QDockWidget("Layers", this);
    layersDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    layersTree = new QTreeWidget();
    layersTree->setHeaderLabel("Layer Name");
    QTreeWidgetItem *layer1 = new QTreeWidgetItem(layersTree);
    layer1->setText(0, "Layer 1");
    QTreeWidgetItem *layer2 = new QTreeWidgetItem(layersTree);
    layer2->setText(0, "Layer 2");
    layersDock->setWidget(layersTree);
    addDockWidget(Qt::RightDockWidgetArea, layersDock);

    // 命令停靠窗口
    commandDock = new QDockWidget("Command History", this);
    commandDock->setAllowedAreas(Qt::BottomDockWidgetArea);
    commandHistoryEdit = new QTextEdit();  // 初始化 commandHistoryEdit
    commandHistoryEdit->setReadOnly(true);
    //QListWidget *commandHistoryList = new QListWidget();
    commandDock->setWidget(commandHistoryEdit);
    addDockWidget(Qt::BottomDockWidgetArea, commandDock);

    // substanceDock
    substanceDock = new QDockWidget("Substances", this);
    substanceDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    substanceTree = new QTreeWidget();
    substanceTree->setHeaderLabel("substanceTree");
    substanceDock->setWidget(substanceTree);
    substanceDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    substanceTree->setContextMenuPolicy(Qt::CustomContextMenu);
    addDockWidget(Qt::LeftDockWidgetArea, substanceDock);

    //boundaryDock
    boundaryDock = new QDockWidget("Boundaries", this);
    boundaryDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    boundaryTree = new QTreeWidget();
    boundaryTree->setHeaderLabel("boundaryTree");
    boundaryDock->setWidget(boundaryTree);
    boundaryDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, boundaryDock);

    //interactionDock
    interactionDock = new QDockWidget("Interactions", this);
    interactionDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    interactionTree = new QTreeWidget();
    interactionTree->setHeaderLabel("interactionTree");
    interactionDock->setWidget(interactionTree);
    interactionDock->setFeatures(QDockWidget::DockWidgetMovable | QDockWidget::DockWidgetFloatable);
    addDockWidget(Qt::LeftDockWidgetArea, interactionDock);

    if (propertiesDock) propertiesDock->hide();   // 隐藏属性面板
    if (layersDock) layersDock->hide();           // 隐藏图层面板
    if (boundaryDock) boundaryDock->hide();       // 隐藏边界树面板
    if (interactionDock) interactionDock->hide(); // 隐藏相互作用面板

    //connect slot funcs
    connect(substanceTree, &QTreeWidget::customContextMenuRequested,
        this, &MainWindow::onSubstanceTreeContextMenu);

    QDockWidget* generatorDock = new QDockWidget(tr("Entity Generator"), this);
    generatorDock->setAllowedAreas(Qt::LeftDockWidgetArea | Qt::RightDockWidgetArea);
    // 只允许移动和浮动，不可关闭
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
        // 如果需要平移，可以在 .geo 脚本中加入偏移量，这里先预留解析)
        double cx = args[7].toDouble();
        double cy = args[8].toDouble();
        double cz = args[9].toDouble();

        // 1. 实例化圆柱壳体生成器
        CylindricalShellGenerator shellGen;

        // 2. 设置参数
        // 注意：这里调用的参数顺序需对应类中 setParameters 的定义
        shellGen.setParameters(r, h, lid, wall, ms);

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

    qDebug() << "网格划分完成！文件已保存在 output.msh";
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
    int partId = 1;        // Part 计数器

    for (auto it = allEntities.begin(); it != allEntities.end(); ++it) {
        const MeshEntity& entity = it->second;

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

            globalNodeId++;
        }

        // 2. 导出六面体单元
        out << "*ELEMENT_SOLID\n";
        for (const auto& hex : entity.hexes) {
            // 解决报错的关键：直接使用 hex[j] 访问 std::array 元素
            out << globalElemId << ", " << partId;

            for (int j = 0; j < 8; ++j) {
                int localIdx = hex[j]; // 获取存储在 array 中的局部节点索引
                out << ", " << localToGlobal[localIdx]; // 映射为全局 ID
            }
            out << "\n";

            globalElemId++;
        }

        // 3. 定义 PART 关键字
        out << "*PART\n";
        out << entity.name << "\n";
        out << partId << ", 1, 1\n"; // 默认分配 SectionID=1, MaterialID=1

        partId++;
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
void MainWindow::addNumParamToUI(GeneratorUI& ui, const QString& labelText, const QString& key, double defaultValue) {
    QHBoxLayout* row = new QHBoxLayout();
    row->addWidget(new QLabel(labelText));
    QDoubleSpinBox* sb = new QDoubleSpinBox(this);
    sb->setRange(-9999.0, 9999.0);
    sb->setValue(defaultValue);
    row->addWidget(sb);
    ui.paramLayout->addLayout(row);
    ui.paramInputs[key] = sb; // 记录到对应的 ui 结构体中
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
        addNumParamToUI(ui, "Length (LX):", "lx", 10.0);
        addNumParamToUI(ui, "Width (LY):", "ly", 10.0);
        addNumParamToUI(ui, "Height (LZ):", "lz", 10.0);
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0);
        addNumParamToUI(ui, "Center X:", "cx", 0.0);
        addNumParamToUI(ui, "Center Y:", "cy", 0.0);
        addNumParamToUI(ui, "Center Z:", "cz", 0.0);
    }
    else if (text == "Cylinder") {
        addNumParamToUI(ui, "Radius (R):", "r", 5.0);
        addNumParamToUI(ui, "Height (H):", "h", 15.0);
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0);
        addNumParamToUI(ui, "Center X:", "cx", 0.0);
        addNumParamToUI(ui, "Center Y:", "cy", 0.0);
        addNumParamToUI(ui, "Center Z:", "cz", 0.0);
    }
    else if (text == "CylindricalShell") {
        addNumParamToUI(ui, "Inner Radius:", "r", 5.0);
        addNumParamToUI(ui, "Total Height:", "h", 15.0);
        addNumParamToUI(ui, "Lid Thick:", "lid", 1.0);
        addNumParamToUI(ui, "Wall Thick:", "wall", 1.0);
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0);
    }
    else if (text == "OpenCylindricalShell") {
        addNumParamToUI(ui, "Radius:", "r", 5.0);
        addNumParamToUI(ui, "Wall Thick:", "wall", 1.0);
        addNumParamToUI(ui, "Base Height:", "h_base", 1.0);
        addNumParamToUI(ui, "Wall Height:", "h_wall", 10.0);
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0);
        addNumParamToUI(ui, "Center X:", "cx", 0.0);
        addNumParamToUI(ui, "Center Y:", "cy", 0.0);
        addNumParamToUI(ui, "Center Z:", "cz", 0.0);
    }
    else if (text == "Sphere" || text == "Hemisphere") {
        addNumParamToUI(ui, "Radius:", "r", 5.0);
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0);
        addNumParamToUI(ui, "Center X:", "cx", 0.0);
        addNumParamToUI(ui, "Center Y:", "cy", 0.0);
        addNumParamToUI(ui, "Center Z:", "cz", 0.0);
    }
    else if (text == "Frustum") {
        addNumParamToUI(ui, "Base R:", "rb", 6.0);
        addNumParamToUI(ui, "Top R:", "rt", 3.0);
        addNumParamToUI(ui, "Height:", "h", 10.0);
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0);
        addNumParamToUI(ui, "Center X:", "cx", 0.0);
        addNumParamToUI(ui, "Center Y:", "cy", 0.0);
        addNumParamToUI(ui, "Center Z:", "cz", 0.0);
    }
    else if (text == "HalfCylinder") {
        addNumParamToUI(ui, "Radius:", "r", 5.0);
        addNumParamToUI(ui, "Height:", "h", 15.0);
        addNumParamToUI(ui, "Mesh Size:", "ms", 1.0);
        addNumParamToUI(ui, "Center X:", "cx", 0.0);
        addNumParamToUI(ui, "Center Y:", "cy", 0.0);
        addNumParamToUI(ui, "Center Z:", "cz", 0.0);
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
        gen.setParameters(val("r"), val("h"), val("lid"), val("wall"), val("ms"));
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
    m_simSetupUI.materialSelector->addItems({ "*MAT_PIECEWISE_LINEAR_PLASTICITY", "*MAT_JOHNSON_COOK", "*MAT_HIGH_EXPLOSIVE_BURN", "*MAT_NULL" });
    matLayout->addWidget(m_simSetupUI.materialSelector);

    // EOS 库选择 (初始隐藏)
    QLabel* eosLabel = new QLabel("状态方程 (Equation of State):");
    m_simSetupUI.eosSelector = new QComboBox();
    m_simSetupUI.eosSelector->addItems({ "None", "*EOS_JWL", "*EOS_GRUNEISEN", "*EOS_LINEAR_POLYNOMIAL" });
    matLayout->addWidget(eosLabel);
    matLayout->addWidget(m_simSetupUI.eosSelector);

    // 动态参数容器
    m_simSetupUI.matParamContainer = new QWidget();
    m_simSetupUI.matParamLayout = new QFormLayout(m_simSetupUI.matParamContainer);
    QScrollArea* scrollArea = new QScrollArea(); // 参数可能很多，加上滚动条
    scrollArea->setWidgetResizable(true);
    scrollArea->setWidget(m_simSetupUI.matParamContainer);
    matLayout->addWidget(scrollArea);

    m_simSetupUI.mainTab->addTab(matTab, "材料(Material)");

    m_simSetupUI.btnAddMaterial = new QPushButton("添加材料与侵蚀设定 (Add Material)");
    matLayout->addWidget(m_simSetupUI.btnAddMaterial);

	// 新增材料预设下拉菜单
    m_simSetupUI.presetSelector = new QComboBox();
    m_simSetupUI.presetSelector->addItems({ "Tungsten_Alloy", "Steel_4340", "Aluminum_6061" }); // 填入你需要的材料选项
    matLayout->addWidget(new QLabel("选择材料预设:"));
    matLayout->addWidget(m_simSetupUI.presetSelector);
    // ==========================================
    // Tab 2: 求解控制 (Control)
    // ==========================================
    QWidget* ctrlTab = new QWidget();
    QFormLayout* ctrlLayout = new QFormLayout(ctrlTab);

    m_simSetupUI.endtimeInput = new QDoubleSpinBox();
    m_simSetupUI.endtimeInput->setRange(0, 99999); m_simSetupUI.endtimeInput->setValue(1.0);
    ctrlLayout->addRow("结束时间 (ENDTIM):", m_simSetupUI.endtimeInput);

    m_simSetupUI.d3plotFreqInput = new QDoubleSpinBox();
    m_simSetupUI.d3plotFreqInput->setRange(0, 9999); m_simSetupUI.d3plotFreqInput->setValue(0.01);
    ctrlLayout->addRow("D3PLOT 步长 (DT):", m_simSetupUI.d3plotFreqInput);

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
    m_simSetupUI.setupSummaryList->setMaximumHeight(150); // 限制一下高度，别占满屏幕
    mainVLayout->addWidget(m_simSetupUI.setupSummaryList);

    // 3. 底部操作按钮
    QHBoxLayout* bottomBtnLayout = new QHBoxLayout();
    m_simSetupUI.btnClearSummary = new QPushButton("清空 (Clear)");
    m_simSetupUI.btnExportKFile = new QPushButton("打包导出 K 文件 (Export K File)");
    m_simSetupUI.btnExportKFile->setStyleSheet("background-color: #4CAF50; color: white; font-weight: bold;"); // 导出按钮搞个醒目的绿色

    bottomBtnLayout->addWidget(m_simSetupUI.btnClearSummary);
    bottomBtnLayout->addWidget(m_simSetupUI.btnExportKFile);
    mainVLayout->addLayout(bottomBtnLayout);

    setupDock->setWidget(mainContainer);

    setupDock->setWidget(mainContainer);
    addDockWidget(Qt::LeftDockWidgetArea, setupDock);

    // 触发一次初始化
    handleMaterialTypeChanged(m_simSetupUI.materialSelector->currentText());

    // ==========================================
    // [补充] Tab 1: 材料标签页的底部加入“侵蚀(Erosion)设置”
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
    // [新增] Tab: 初始条件 (Initial Conditions)
    // ==========================================
    QWidget* icTab = new QWidget();
    QFormLayout* icLayout = new QFormLayout(icTab);

    m_simSetupUI.icEntitySelector = new QComboBox();
    icLayout->addRow("目标实体 (Part):", m_simSetupUI.icEntitySelector);

    m_simSetupUI.icVx = new QDoubleSpinBox(); m_simSetupUI.icVx->setRange(-99999, 99999);
    m_simSetupUI.icVy = new QDoubleSpinBox(); m_simSetupUI.icVy->setRange(-99999, 99999);
    m_simSetupUI.icVz = new QDoubleSpinBox(); m_simSetupUI.icVz->setRange(-99999, 99999);
    icLayout->addRow("X 向初始速度 (Vx):", m_simSetupUI.icVx);
    icLayout->addRow("Y 向初始速度 (Vy):", m_simSetupUI.icVy);
    icLayout->addRow("Z 向初始速度 (Vz):", m_simSetupUI.icVz);

    m_simSetupUI.mainTab->addTab(icTab, "初始条件(IC)");

    m_simSetupUI.btnAddIC = new QPushButton("添加初始速度 (Add IC)");
    icLayout->addWidget(m_simSetupUI.btnAddIC);
    // ==========================================
    // [新增] Tab: 接触定义 (Contact)
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
    // [新增] Tab: 截面与算法 (Section)
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
    // [修改] Tab: 控制面板 (Control) 增加高级选项
    // ==========================================

    m_simSetupUI.jobTitleInput = new QLineEdit("Fragment_ignition");
    ctrlLayout->insertRow(0, "项目名称 (TITLE):", m_simSetupUI.jobTitleInput);

    m_simSetupUI.tssfacInput = new QDoubleSpinBox();
    m_simSetupUI.tssfacInput->setRange(0.1, 1.0); m_simSetupUI.tssfacInput->setValue(0.9); m_simSetupUI.tssfacInput->setSingleStep(0.1);
    ctrlLayout->insertRow(1, "时间步缩放 (TSSFAC):", m_simSetupUI.tssfacInput);

    // 连接所有信号
    connect(m_simSetupUI.materialSelector, &QComboBox::currentTextChanged, this, &MainWindow::handleMaterialTypeChanged);

    connect(m_simSetupUI.btnAddMaterial, &QPushButton::clicked, this, &MainWindow::handleAddMaterial);
    connect(m_simSetupUI.btnAddContact, &QPushButton::clicked, this, &MainWindow::handleAddContact);
    connect(m_simSetupUI.btnAddIC, &QPushButton::clicked, this, &MainWindow::handleAddIC);
    connect(m_simSetupUI.btnAddSection, &QPushButton::clicked, this, &MainWindow::handleAddSection);
    connect(m_simSetupUI.btnClearSummary, &QPushButton::clicked, this, &MainWindow::handleClearSummary);
    // 这里的 btnExportKFile 连接到你之前写的 handleApplySimulationSettings
    connect(m_simSetupUI.btnExportKFile, &QPushButton::clicked, this, &MainWindow::handleApplySimulationSettings);
}

void MainWindow::handleMaterialTypeChanged(const QString& matType) {
    // 1. 清空旧表单
    QLayoutItem* item;
    while ((item = m_simSetupUI.matParamLayout->takeAt(0)) != nullptr) {
        delete item->widget();
        delete item;
    }
    m_simSetupUI.currentMatInputs.clear();

    // 辅助 Lambda 表达式：快速添加一行参数
    auto addParam = [&](const QString& label, const QString& key, double defaultVal) {
        QDoubleSpinBox* box = new QDoubleSpinBox();
        box->setRange(-999999, 999999);
        box->setDecimals(5);
        box->setValue(defaultVal);
        m_simSetupUI.matParamLayout->addRow(label, box);
        m_simSetupUI.currentMatInputs[key] = box;
        };

    // 2. 根据材料类型动态构建 UI
    if (matType == "*MAT_JOHNSON_COOK") {
        m_simSetupUI.eosSelector->setCurrentText("*EOS_GRUNEISEN"); // 默认关联
        addParam("密度 (RO):", "ro", 7.83e-6); // 例：钢铁 kg/mm^3
        addParam("剪切模量 (G):", "g", 77.0);
        addParam("屈服强度 (A):", "a", 0.792);
        addParam("硬化常数 (B):", "b", 0.510);
        addParam("硬化指数 (N):", "n", 0.26);
        addParam("应变率常数 (C):", "c", 0.014);
        addParam("软化指数 (M):", "m", 1.03);
        addParam("熔点 (TMELT):", "tmelt", 1793);
    }
    else if (matType == "*MAT_HIGH_EXPLOSIVE_BURN") {
        m_simSetupUI.eosSelector->setCurrentText("*EOS_JWL"); // 炸药必须配 JWL
        addParam("密度 (RO):", "ro", 1.63e-6); // 例：TNT
        addParam("爆速 (D):", "d", 6.93);
        addParam("CJ 压力 (PCJ):", "pcj", 21.0);
        // -- 自动追加展示 JWL 参数 --
        addParam("[JWL] A:", "jwl_a", 373.77);
        addParam("[JWL] B:", "jwl_b", 3.747);
        addParam("[JWL] R1:", "jwl_r1", 4.15);
        addParam("[JWL] R2:", "jwl_r2", 0.90);
        addParam("[JWL] OMEGA:", "jwl_omega", 0.35);
        addParam("[JWL] E0:", "jwl_e0", 6.0);
    }
}

void MainWindow::handleApplySimulationSettings() {
    // 假设你有一个全局或成员变量 LSDynaDeck m_deck; 专门用来生成 .k 文件

    // 1. 获取控制参数
    double endtim = m_simSetupUI.endtimeInput->value();
    // 伪代码： m_deck.controlCard.setTerminationTime(endtim);

    // 2. 获取当前实体和材料
    QString targetEntity = m_simSetupUI.entitySelector->currentText();
    QString matType = m_simSetupUI.materialSelector->currentText();

    // 使用我们保存的输入框 QMap 一键取值
    auto val = [&](QString key) {
        return m_simSetupUI.currentMatInputs.contains(key) ? m_simSetupUI.currentMatInputs[key]->value() : 0.0;
        };

    if (matType == "*MAT_JOHNSON_COOK") {
        // 伪代码：调用你刚写的类
        // MAT_JohnsonCookCard jcCard;
        // jcCard.RO = val("ro");
        // jcCard.A = val("a");
        // m_deck.addMaterial(jcCard);
    }
    else if (matType == "*MAT_HIGH_EXPLOSIVE_BURN") {
        // 伪代码
        // MAT_HighExplosiveBurn heb;
        // heb.RO = val("ro");
        // heb.D = val("d");
        // EOS_JWL jwl;
        // jwl.A = val("jwl_a"); ...
        // heb.attachEOS(jwl);
        // m_deck.addMaterial(heb);
    }
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

    // 加载所有实体
    const auto& allEntities = m_repository.getAllEntities();
    for (const auto& pair : allEntities) {
        QString name = pair.first;
        m_simSetupUI.entitySelector->addItem(name);
        m_simSetupUI.icEntitySelector->addItem(name);
        m_simSetupUI.contactMasterSelector->addItem(name);
        m_simSetupUI.contactSlaveSelector->addItem(name);
        m_simSetupUI.sectionEntitySelector->addItem(name);
    }

    // 尝试恢复之前的选择
    m_simSetupUI.entitySelector->setCurrentText(curMat);
    m_simSetupUI.icEntitySelector->setCurrentText(curIc);
    m_simSetupUI.contactMasterSelector->setCurrentText(curMaster);
    m_simSetupUI.contactSlaveSelector->setCurrentText(curSlave);
    m_simSetupUI.sectionEntitySelector->setCurrentText(curSec);
}

void MainWindow::handleAddMaterial() {
    QString target = m_simSetupUI.entitySelector->currentText();
    QString preset = m_simSetupUI.presetSelector->currentText();
    bool useErosion = m_simSetupUI.erosionGroup->isChecked();

    QString summary = QString("[材料] 实体: %1 | 预设: %2").arg(target).arg(preset);
    if (useErosion) {
        summary += QString(" | 侵蚀开启 (MXEPS=%1)").arg(m_simSetupUI.erosionMxeps->value());
    }

    m_simSetupUI.setupSummaryList->addItem(summary);
    // 这里可以同时调用底层 C++ 库将参数真正存入你的 LSDynaDeck 中
}

void MainWindow::handleAddContact() {
    QString type = m_simSetupUI.contactTypeSelector->currentText().remove("*CONTACT_"); // 简化显示名称
    QString master = m_simSetupUI.contactMasterSelector->currentText();
    QString slave = m_simSetupUI.contactSlaveSelector->currentText();

    QString summary = QString("[接触] %1 | 主面: %2 | 从面: %3").arg(type).arg(master).arg(slave);
    m_simSetupUI.setupSummaryList->addItem(summary);
}

void MainWindow::handleAddIC() {
    QString target = m_simSetupUI.icEntitySelector->currentText();
    double vx = m_simSetupUI.icVx->value();
    double vy = m_simSetupUI.icVy->value();
    double vz = m_simSetupUI.icVz->value();

    QString summary = QString("[初始速度] 实体: %1 | V=(%2, %3, %4)")
        .arg(target).arg(vx).arg(vy).arg(vz);
    m_simSetupUI.setupSummaryList->addItem(summary);
}

void MainWindow::handleAddSection() {
    QString target = m_simSetupUI.sectionEntitySelector->currentText();
    QString type = m_simSetupUI.sectionTypeSelector->currentText().remove("*SECTION_");

    QString summary = QString("[截面] 实体: %1 | 类型: %2").arg(target).arg(type);
    m_simSetupUI.setupSummaryList->addItem(summary);
}

void MainWindow::handleClearSummary() {
    m_simSetupUI.setupSummaryList->clear();
    // 伪代码: m_deck.clearAllParams(); // 清空底层的卡片数据，防止旧数据残留
}