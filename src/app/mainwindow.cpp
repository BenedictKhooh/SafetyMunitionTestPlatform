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

    //connect slot funcs
    connect(substanceTree, &QTreeWidget::customContextMenuRequested,
        this, &MainWindow::onSubstanceTreeContextMenu);

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

        points.clear();

		drawables.clear();
        glWidget->setDrawableInstances(drawables);

        drawables_lines.clear();
        glWidget->setDrawableLines(drawables_lines);
        
        substanceTree->clear();

        logCommand(command, "Cleared all!");
    } else if (cmd == "help") {
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

    drawables.clear();
    drawables_lines.clear();
    points.clear();

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

    DrawCommand cmd;
    cmd.type = DrawCommand::Points;
    cmd.pointsCmd.points = points;
    cmd.pointsCmd.size = 2.5f;

    //glWidget->clearDrawCommands();
    glWidget->submitDrawCommand(cmd);

    std::vector<Vector3> wireLines = buildHexWireframe(nodes, hexes);
   // auto wireLines = buildHexWireframe( nodes, hexes );

    MeshEntity newEntity;
    newEntity.name = entityName;
    newEntity.type = "Empty"; // 或者是从某个地方传过来的类型
    newEntity.nodes = points;
    newEntity.hexes = hexes;
    newEntity.wireLines = wireLines;

    m_repository.addEntity(entityName, newEntity);

    DrawCommand lineCmd;
    lineCmd.type = DrawCommand::Lines;
    lineCmd.linesCmd.points = wireLines;
    lineCmd.linesCmd.width = 2.0f;

    glWidget->submitDrawCommand(lineCmd);
    redrawAllEntities();

    qDebug() << "done parsing and wireframe generated!";
    qDebug() << "Wireframe submitted with" << wireLines.size() / 2 << "lines.";
    file.close();
}

void MainWindow::redrawAllEntities() {
    // 1. 清空 GLWidget 里的旧命令
    glWidget->clearDrawCommands();

    // 2. 遍历仓库里的所有实体
    const auto& allEntities = m_repository.getAllEntities();
    for (const auto& pair : allEntities) {
        const MeshEntity& entity = pair.second;

        // 提交点云渲染命令 (如果你需要)
        DrawCommand ptCmd;
        ptCmd.type = DrawCommand::Points;
        ptCmd.pointsCmd.points = entity.nodes;
        ptCmd.pointsCmd.size = 2.5f;
        glWidget->submitDrawCommand(ptCmd);

        // 提交线框渲染命令
        DrawCommand lineCmd;
        lineCmd.type = DrawCommand::Lines;
        lineCmd.linesCmd.points = entity.wireLines;
        lineCmd.linesCmd.width = 2.0f;
        glWidget->submitDrawCommand(lineCmd);
    }

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
    // 1. 先清空当前的树，防止重复堆叠
    substanceTree->clear();

    // 2. 获取仓库中所有的实体
    const auto& entities = m_repository.getAllEntities();

    // 3. 遍历并创建节点
    for (auto it = entities.begin(); it != entities.end(); ++it) {
        const MeshEntity& entity = it->second;

        // 创建一级节点：显示实体名字
        QTreeWidgetItem* topItem = new QTreeWidgetItem(substanceTree);
        topItem->setText(0, entity.name);

        // 创建二级节点：显示基本参数（可选）
        QTreeWidgetItem* typeItem = new QTreeWidgetItem(topItem);
        typeItem->setText(0, "Type: " + entity.type);

        QTreeWidgetItem* nodeItem = new QTreeWidgetItem(topItem);
        nodeItem->setText(0, QString("Nodes: %1").arg(entity.nodes.size()));

        // 默认展开新生成的节点
        topItem->setExpanded(true);
    }
}

void MainWindow::onSubstanceTreeContextMenu(const QPoint& pos) {
    // 获取点击位置所在的项
    QTreeWidgetItem* item = substanceTree->itemAt(pos);
    if (!item) return; // 如果点在空白处，不弹菜单

    // 获取实体名称（如果是二级节点，则找父节点）
    QString entityName = item->parent() ? item->parent()->text(0) : item->text(0);

    // 创建菜单
    QMenu menu(this);
    QAction* deleteAction = menu.addAction(tr("Delete the substance: ") + entityName);

    // 执行菜单并获取用户的点击结果
    QAction* selectedAction = menu.exec(substanceTree->mapToGlobal(pos));

    if (selectedAction == deleteAction) {
        // --- 核心调用：执行删除流程 ---
        this->handleDeleteEntity(entityName);
    }
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