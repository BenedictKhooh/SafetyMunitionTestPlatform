#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>
#include <vector>

#include "src/core/common/common_types.h"
#include "src/core/common/MeshGenerator.h"
#include "src/core/common/MeshManager.h"
#include "src/core/generation/SphereGenerator.h"
#include "src/core/generation/CylinderGenerator.h"
#include "src/core/generation/CylindricalShellGenerator.h"
#include "src/core/generation/CubeGenerator.h"
#include "src/core/generation/HemisphereGenerator.h"
#include "src/core/generation/FrustumGenerator.h"
#include "src/core/generation/HalfCylinderGenerator.h"
#include "src/core/generation/HalfCylindricalShellGenerator.h"
#include "src/core/generation/OpenCylindricalShellGenerator.h"
#include "src/core/generation/HexagonalPrismGenerator.h"
#include "src/core/generation/PentagonalPrismGenerator.h"
#include "src/core/generation/TriangularPrismGenerator.h"
#include "src/core/generation/FSPGenerator.h"
#include "src/core/generation/FrustumShellGenerator.h"
#include "src/core/generation/RefinedCylinderGenerator.h"
#include "src/core/generation/RefinedFrustumGenerator.h"
#include "src/core/generation/RefinedCylindricalShellGenerator.h"
#include "src/core/generation/RefinedFrustumShellGenerator.h"
#include "src/core/generation/RefinedHalfCylindricalShellGenerator.h"
#include "src/core/generation/RefinedOpenCylindricalShellGenerator.h"
#include "src/core/generation/BulletGenerator.h"
#include "src/core/common/EntityRepository.h"
#include "commandline.h"

// Keyword Cards headers
#include "src/core/common/LSDynaDeck.h"
#include "src/core/common/Part/PartCard.h"
#include "src/core/common/Contact/ContactCard.h"
#include "src/core/common/Material/MaterialCard.h"
#include "src/core/common/EOS/EOS.h"
#include "src/core/common/Boundary/BoundaryCards.h"
#include "src/core/common/InitialConditions/InitialConditions.h"
#include "src/core/common/GlobalControl/GlobalControl.h"
#include "src/core/common/Section/SectionCard.h"
#include <memory>
//

#include <QTabWidget>    // 用于顶层隔离
#include <QProcess>      // 用于调用和监控 LS-DYNA 求解器
#include "src/app/PostProcessing/PostProcessWidget.h"

#include <QTableWidget>
#include <QHeaderView>

// Forward declaration
class GLWidget;
class QPushButton;

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void onCommandEntered(const QString &command);
    void onDrawingComplete();
    void startMeshing();

    void parseMeshFile( QString fileName );

    //各个“添加”操作的响应函数
    void handleAddMaterial();
    void handleAddContact();
    void handleAddIC();
    void handleAddSection();
    void handleAddGlobalControl();

    //清空列表
    void handleClearSummary();

    void handleAddSensor();


	//Postprocessing
    // --- 求解与监控相关槽函数 ---
    void browseKFile();
    void browseSolver();
    void startCalculation();
    void stopCalculation();
    void readSolverOutput(); // 实时读取求解器日志
    void handleSolverFinished(int exitCode, QProcess::ExitStatus exitStatus);

    void showSummaryContextMenu(const QPoint& pos);


private:
    void createMenuBar();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
    void createCommandLine();
    void logCommand(const QString &command, const QString &response = "");
    void clean();
    void showHelp();

	//Postprocessing
    //记录每个实体被选中的传感器局部节点索引
    QMap<QString, QList<int>> m_sensorNodes;

    // --- 顶层布局控件 ---
    QTabWidget* mainModeTab;
    QWidget* preProcessWidget;  // 存放你原有的所有前处理界面
    QWidget* postProcessWidget; // 新的后处理界面

    PostProcessWidget* dataVisualizerWidget;

    // --- 求解器控制台控件 ---
    QLineEdit* kFilePathEdit;
    QLineEdit* solverPathEdit;
    QSpinBox* cpuCoresSpin;
    QPushButton* btnRunSolver;
    QPushButton* btnStopSolver;
    QTextEdit* solverConsole;   // 实时日志输出窗口

    // --- 异步进程对象 ---
    QProcess* m_solverProcess;

    void setupPostProcessUI(); // 初始化后处理界面的函数
	//Postprocessing

private:
    // 定义一个结构体来管理每个独立窗口的 UI 控件
    struct GeneratorUI {
        QComboBox* shapeComboBox;
        QWidget* paramContainer;
        QVBoxLayout* paramLayout;
        QLineEdit* nameInput;
        QMap<QString, QDoubleSpinBox*> paramInputs;

        QCheckBox* chkVerticalRefinement;
        QWidget* refinementContainer;
        QDoubleSpinBox* spinZStart;
        QDoubleSpinBox* spinZEnd;
        QDoubleSpinBox* spinMsLocalZ;

        QCheckBox* chkWallRefinement;
        QWidget* wallRefineContainer;
        QDoubleSpinBox* spinMsWall;
    };

    // 声明三个独立窗口的 UI 管理器
    GeneratorUI m_fragmentUI; // 破片
    GeneratorUI m_shellUI;    // 壳体
    GeneratorUI m_chargeUI;   // 装药

    // 声明统一的创建窗口的辅助函数
    void setupGeneratorTab(QTabWidget* tabWidget, const QString& title, const QStringList& shapes, GeneratorUI& ui);
    // 重构的槽函数，将具体的 ui 结构体作为参数传入
    void handleShapeTypeChanged(GeneratorUI& ui, const QString& text);
    void handleGenerateButtonClicked(GeneratorUI& ui);

    // 辅助函数：向特定的 UI 结构体中添加带单位后缀的输入框
    void addNumParamToUI(GeneratorUI& ui, const QString& labelText, const QString& key, double defaultValue, const QString& unit = "");

    QTextEdit *commandHistoryEdit;

    void DrawLine(QStringList args);

    std::vector<Instance> drawables;
    std::vector<GeoLine> drawables_lines;
    MeshManager* m_meshManager;

    // UI Widgets
    GLWidget *glWidget;
    QMenuBar *menuBar;
    QToolBar *fileToolBar;
    QToolBar *editToolBar;
    QToolBar *viewToolBar;
    QStatusBar *statusBar;
    QDockWidget* substanceDock;
    QTreeWidget* substanceTree;
    QDockWidget* commandDock; 
    CommandLine* commandLine;

    void updateSubstanceTree();
    void onSubstanceTreeContextMenu(const QPoint& pos);

    void handleDeleteEntity(QString name);

   /*  Data containers for the reconstruction process
    std::vector<MeshPoint> m_points;
    std::vector<MeshPoint> m_points_sphere;*/

    //store all the points
    EntityRepository m_repository; // 实体仓库实例
    void redrawAllEntities();      // 封装一个全量重绘的函数
    void clearAllEntities();

    void exportToKFile(const QString& fileName); // 导出逻辑

	void handleTranslateEntity(const QString& entityName);//平移
	void handleScaleEntity(const QString& entityName);   //缩放
	void handleRotateEntity(const QString& entityName);   //旋转
	void applyTransformation(const QString& entityName, const QMatrix4x4& mat); // 变换应用函数

    QString m_workingDirectory; //用于保存当前的工作目录路径
    // ==========================================
    // 全局系统环境与路径设置
    // ==========================================
    /** @brief LS-DYNA 求解器可执行文件全局路径 */
    QString m_dynaSolverPath = "D:\\Program Files\\ANSYS Inc\\v241\\ansys\\bin\\winx64\\lsdyna_sp.exe";

    /** @brief LS-DYNA 运行依赖库/环境变量全局目录路径 */
    QString m_dynaEnvPath = "D:\\Program Files\\ANSYS Inc\\v241\\ansys\\bin\\winx64\\lsprepost410";

    // 仿真设置 UI 管理器
    struct SimulationSetupUI {
        QTabWidget* mainTab;

        // --- 材料标签页组件 ---
        QComboBox* entitySelector;   // 选择给哪个实体赋予材料
        QComboBox* materialSelector; // 比如 "MAT_JOHNSON_COOK", "MAT_HIGH_EXPLOSIVE_BURN"
        QComboBox* eosSelector;      // 比如 "EOS_JWL", "None" (动态显示/隐藏)
        QWidget* matParamContainer;  // 材料参数动态生成区
        QFormLayout* matParamLayout;

        // --- 控制卡标签页组件 ---
        QDoubleSpinBox* endtimeInput;
        QDoubleSpinBox* dtinitInput;
        QDoubleSpinBox* d3plotFreqInput;

        // 保存所有的动态输入框指针，便于一键获取数据
        QMap<QString, QDoubleSpinBox*> currentMatInputs;

        // ---材料失效与侵蚀 (附加在材料 Tab 底部) ---
        QGroupBox* erosionGroup;      // 侵蚀设置的组合框
        QCheckBox* erosionEnable;     // 是否启用 *MAT_ADD_EROSION
        QDoubleSpinBox* erosionMxeps;   // 最大失效应变

        // ---初始条件 (Initial Conditions) ---
        QComboBox* icEntitySelector;    // 选择施加初始速度的实体
        QDoubleSpinBox* icVx;
        QDoubleSpinBox* icVy;
        QDoubleSpinBox* icVz;

        // ---接触定义 (Contact) ---
        QComboBox* contactTypeSelector; // 接触类型下拉框
        QComboBox* contactMasterSelector; // 主面实体
        QComboBox* contactSlaveSelector;  // 从面实体
        QDoubleSpinBox* contactFs;        // 静摩擦系数
        QDoubleSpinBox* contactFd;        // 动摩擦系数

        // ---截面属性 (Section) ---
        QComboBox* sectionEntitySelector; // 目标实体
        QComboBox* sectionTypeSelector;   // Solid 还是 Shell
        QComboBox* sectionElformSelector; // 单元算法 (ELFORM)

        // ---求解控制 (Control) ---
        QLineEdit* jobTitleInput;         //项目名称
        QDoubleSpinBox* tssfacInput;      //时间步缩放因子
       
        // ---各个面板的“添加”按钮 ---
        QPushButton* btnAddMaterial;
        QPushButton* btnAddContact;
        QPushButton* btnAddIC;
        QPushButton* btnAddSection;
        QPushButton* btnAddControl;

        // ---底部实时观察面板 ---
        QListWidget* setupSummaryList; // 用于显示已添加的参数条目
        QPushButton* btnClearSummary;  // 清空列表按钮
        QPushButton* btnExportKFile;   // 最终的导出按钮

        QComboBox* presetSelector; //材料预设下拉框

        // Sensors

        QComboBox* sensorEntitySelector;
        QComboBox* sensorModeSelector;

        // 2. 通用坐标 (起点或单点位置)
        QDoubleSpinBox* sensorStartX;
        QDoubleSpinBox* sensorStartY;
        QDoubleSpinBox* sensorStartZ;
        QDoubleSpinBox* sensorEndX;      // 阵列终点 X
        QDoubleSpinBox* sensorEndY;      // 阵列终点 Y
        QDoubleSpinBox* sensorEndZ;      // 阵列终点 Z

        QWidget* sensorArrayContainer;
        QSpinBox* sensorNumPoints;

        QDoubleSpinBox* sensorX; QDoubleSpinBox* sensorY; QDoubleSpinBox* sensorZ;
        QPushButton* btnAddSensor;
    };

    SimulationSetupUI m_simSetupUI;
    void updateAllEntitySelectors();//一个辅助函数，用于统一刷新所有的“实体选择下拉框”

    // 创建面板的函数
    void createSimulationSetupDock();

    // 动态生成表单的响应槽函数
    void handleMaterialTypeChanged(const QString& matType);

    // 将 UI 数据保存到 LSDynaDeck 仓库中的函数
    void handleApplySimulationSettings();

    //响应点击“设置工作目录”菜单的槽函数
    void onSetWorkingDirectory();
    /**
     * @brief 响应用户动作：打开全局系统设置面板 (配置 LS-DYNA 路径等)
     */
    void onGlobalSettings();

    // ==========================================
    // 数据集与实体指针映射
    // ==========================================
    LSDynaDeck m_deck; // 纯粹的容器管家

    // 记录 UI 上的实体名称对应的 PartCard 智能指针
    QMap<QString, std::shared_ptr<PartCard>> m_entityParts;

    // 核心桥梁：获取实体的 Part 指针，如果不存在则自动创建并丢入 m_deck
    std::shared_ptr<PartCard> getOrCreatePart(const QString& entityName);

    //JSON 预设解析数据结构
    struct MaterialPreset {
        QString matType;
        QString eosType;
        QString source; // 数据来源文献
        QMap<QString, double> params;
    };
    QMap<QString, MaterialPreset> m_materialPresets; // 内存中的预设字典
    void loadMaterialPresetsFromJson(); // 读取 JSON 文件的辅助函数


    //专门追踪唯一的控制卡片，确保它在 K 文件中最先输出
    std::shared_ptr<GlobalControlCard> m_globalControlCard = nullptr;

private slots:

    void handlePresetChanged(const QString& presetName);
    void handleViewEntityKeyword(const QString& entityName);

    void handleApplySymmetryBoundary(const QString& entityName, char axis);

    void handleGenerateMeshConvergenceBatch();  // 生成网格收敛性批处理
    /**
     * @brief 响应用户动作：仅导出基础仿真环境文件
     */
    void handleGenerateThresholdFiles();

    /**
     * @brief 响应用户动作：初始化升降法状态机并调度计算序列
     */
    void handleSubmitThresholdTask();

    /**
     * @brief 响应用户动作：安全回收系统资源与销毁底层进程树
     */
    void handleTerminateProcess();
    void handleAnalyzeConvergence();
    void handleRefreshEntityTable();
 
    /** * @brief 处理批处理进程的标准输出流
     */
    void handleBatchProcessOutput();

    /**
     * @brief 响应用户动作：刷新并读取物理模型中已设置的初始冲击速度
     */
    void handleRefreshInitialVelocity();

    /** * @brief 处理批处理进程的结束信号
     * @param exitCode 进程的退出状态码
     * @param exitStatus 进程的退出状态枚举值
     */
    void handleBatchProcessFinished(int exitCode, QProcess::ExitStatus exitStatus);

    /**
     * @brief 响应定时器轮询事件：执行物理求解文件的在途能量突跃检测
     */
    void handleMatsumPolling();
    void handleSkipCurrentStep();

    void handleRunExistingBat();           // 运行已有的网格收敛性分析 bat 文件
    void updateMeshMonitorConsole();       // 更新网格收敛性分析的 Tab 终端的输出
    void updateMeshConvergencePlot();      // 定时器触发：增量解析 glstat 并绘图
    void onMeshMonitorMetricChanged();     // 切换监控指标（内能/动能）

    void handleStopMeshBatch();            // 停止当前批处理计算

private:

    struct SymmetryRule {
        QString entityName;
        char axis;
    };
    std::vector<SymmetryRule> m_symmetryRules;

    //重建算法
    void remeshEntityWithNewSize(MeshEntity* entity, double newMeshSize);

    //网格收敛性分析 UI 控件
    QTableWidget* tableMeshSettings;
    QSpinBox* spinMeshSteps;
    QComboBox* comboTargetMetric;
    QDoubleSpinBox* spinTolerance;

    // =========================================================
    // 起爆阈值闭环寻优 (Up-and-Down Method) UI 控件组
    // =========================================================
    /** @brief 初始撞击速度输入控件 (默认从物理模型中继承) */
    QDoubleSpinBox* spinStartVelocity;

    /** @brief 升降法速度寻优步长输入控件 */
    QDoubleSpinBox* spinVelocityStep;

    /** @brief 最大有效测试总次数输入控件 */
    QSpinBox* spinMaxSteps;
    QTableWidget* tableThresholdResults = nullptr;      //工况历史记录表
    QDoubleSpinBox* spinAcceptableVelocityThreshold = nullptr; //临界判定速度阈值

    /** @brief 动作按钮：仅生成基础共享文件，不唤醒求解器 */
    QPushButton* btnGenerateThreshold;

    /** @brief 动作按钮：生成文件并将其提交至闭环状态机 */
    QPushButton* btnSubmitThreshold;

    /** @brief 动作按钮：强行中断当前状态机并销毁底层进程树 */
    QPushButton* btnTerminateProcess;

    /** @brief 外部批处理进程管理器，用于在后台非阻塞执行 .bat 脚本 */
    QProcess* m_batchProcess = nullptr;
    QTabWidget* solveTaskTabs;
    /** @brief 动作按钮：从已生成的初始条件卡片中读取冲击速度 */
    QPushButton* btnRefreshVelocity;

    // =========================================================
    // 升降法 (Up-and-Down Method) 状态机变量与控制方法
    // =========================================================

    /** @brief 标识当前系统是否处于升降法闭环寻优模式 */
    bool m_isUpAndDownMode = false;

    /** @brief 当前已执行的测试步数 */
    int m_upDownCurrentStep = 0;

    /** @brief 设定的最大测试总次数 */
    int m_upDownMaxSteps = 0;

    /** @brief 当前工况设定的撞击速度 (m/s) */
    double m_upDownCurrentVelocity = 0.0;

    /** @brief 速度递增或递减的步长 (m/s) */
    double m_upDownStepSize = 0.0;

    /** @brief 当前执行工况所在的子目录路径 */
    QString m_upDownCurrentDir = "";

    /** @brief 历史测试记录集合：存储结构为 <设定速度, 是否发生起爆> */
    std::vector<std::pair<double, bool>> m_upDownHistory;

    /**
     * @brief 组装并调度升降法序列中的下一个求解工况
     */
    void executeNextUpAndDownStep();

    /**
     * @brief 解析求解器输出文件以研判指定药柱是否发生起爆
     * @param stepDir 当前求解工况所在的子目录路径
     * @param explosivePartId 炸药药柱在 LS-DYNA 模型中的 Part ID (默认设为 1)
     * @return 若判定为发生起爆则返回 true，未起爆返回 false
     */
    bool checkDetonationResult(const QString& stepDir, int explosivePartId = 1);
    QTextEdit* thresholdConsole;

    /** @brief 实时轮询定时器，用于在求解计算中途异步调取状态文件 */
    QTimer* m_matsumPollingTimer = nullptr;

    /** @brief 状态机转移旗标：标识当前求解工况是否因侦测到起爆突跃而被强行截断 */
    bool m_isEarlyDetonated = false;

private:
    double m_previousInternalEnergy = -1.0; // 上一采样时刻的内能 (初始值设为负数)
    int m_energyDropCounter = 0;            // 内能连续下降的采样次数计数器
    bool m_isEarlyMisfire = false;          // 标记是否因“内能倒流”被探针主动判定为死火
    //用于“能量停滞”检测的时域基线
    double m_baselineTime = -1.0;
    double m_initialFragKE = -1.0;          // 记录 t=0 时刻破片的初始宏观动能
    double m_lowerBoundMag = -1.0; ///< 已知死火的最大绝对速度 (-1表示未找到)
    double m_upperBoundMag = -1.0; ///< 已知起爆的最小绝对速度 (-1表示未找到)
    QPushButton* btnSkipStep = nullptr;

private:

    QPushButton* btnSubmitExistingBat;     // 提交现有批处理按钮
    QTextEdit* meshMonitorConsole;         // 专属监控终端
    QCustomPlot* meshConvergencePlot;      // 实时曲线图

    // 逻辑控制
    QProcess* m_meshBatchProcess = nullptr; // 独立的网格收敛批处理进程
    QTimer* m_meshMonitorTimer = nullptr;   // 实时刷新定时器
    int m_currentMeshStepToMonitor = 0;     // 当前监控的 Step 编号 (Step_1, Step_2...)
    qint64 m_lastGlstatPos = 0;             // 文件读取指针（实现增量读取）

    QPushButton* btnStopMeshBatch;         // 停止按钮

private:
    QCustomPlot* m_solverPlot;       // 求解监控图表
    QTimer* m_solverPlotTimer;      // 实时解析 glstat 的定时器
    QComboBox* m_solverDataSourceCombo = nullptr; ///< 数据源选择 
    QComboBox* m_solverIdCombo = nullptr;         ///< 单元/节点 ID 选择
    QComboBox* m_solverParamCombo = nullptr;      ///< 具体参数选择
    void setupSolverPlotUI(QBoxLayout* layout); // 初始化图表函数声明

    qint64 m_lastSolverGlstatPos = 0;
    qint64 m_lastSolverNodoutPos = 0;
    qint64 m_lastSolverEloutPos = 0;

    double m_rtNodoutTime = 0.0;
    bool m_rtNodoutIsDataBlock = false;
    double m_rtEloutTime = 0.0;
    int m_rtEloutId = -1;
    int m_rtEloutBlock = 0; // 0:NONE, 1:STRESS, 2:HISTOR

    QVector<double> m_rtGlstatTime, m_rtGlstatKe, m_rtGlstatIe;
    QMap<int, QVector<double>> m_rtNodoutTimeMap;
    QMap<int, QMap<QString, QVector<double>>> m_rtNodoutData;
    QMap<int, QVector<double>> m_rtEloutTimeMap;
    QMap<int, QMap<QString, QVector<double>>> m_rtEloutData;
    /**
     * @brief [核心后处理] 借助内置 Python 脚本提取 d3plot 中的炸药最终反应度
     * @param workDir 当前工况的计算目录 (用于生成临时脚本和提取 CSV)
     * @param targetElemId 目标观测单元的 ID
     * @return bool 是否发生起爆 (判定标准：最大反应度 >= 0.5)
     */
    bool checkDetonationFromD3plotPython(const QString& workDir, int targetElemId);

private slots:
    void updateSolverPlot();
    void updateSolverPlotUI();
    void redrawSolverPlot();
private:
    void parseRealTimeGlstat(const QString& filePath);
    void parseRealTimeNodout(const QString& filePath);
    void parseRealTimeElout(const QString& filePath);

    // ==========================================
    // 新增：升降法专属独立页面组件
    // ==========================================
    QWidget* upDownSolverWidget = nullptr;       // 升降法独立 Tab 页面
    void setupUpDownSolverTab();                 // 升降法页面初始化函数

    QCustomPlot* thresholdPlot = nullptr;             
    QComboBox* m_thresholdDataSourceCombo = nullptr; 
    QComboBox* m_thresholdIdCombo = nullptr;
    QComboBox* m_thresholdParamCombo = nullptr; 

    QTimer* m_thresholdPlotTimer = nullptr;

    qint64 m_lastThresholdGlstatPos = 0;
    qint64 m_lastThresholdNodoutPos = 0;
    qint64 m_lastThresholdEloutPos = 0;

private slots:
    void updateThresholdPlotUI(); // 更新下拉框内容
    void redrawThresholdPlot();   // 重绘右侧图窗
    void updateThresholdPlot();   // 定时器调用的文件解析
    // 用于记录图表的读取位置
    
};
#endif // MAINWINDOW_H
