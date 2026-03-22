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
#include "src/core/common/EntityRepository.h"
#include "commandline.h"

// Keyword Cards headers
#include "src/core/common/LSDynaDeck.h"
#include "src/core/common/Part/PartCard.h"
#include "src/core/common/Contact/ContactCard.h"
#include "src/core/common/Material/MaterialCard.h"
#include "src/core/common/EOS/EOS.h"
#include "src/core/common/InitialConditions/InitialConditions.h"
#include "src/core/common/GlobalControl/GlobalControl.h"
#include "src/core/common/Section/SectionCard.h"
#include <memory>
//

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

    //清空列表
    void handleClearSummary();

    void handleAddSensor();

private:
    void createMenuBar();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
    void createCommandLine();
    void logCommand(const QString &command, const QString &response = "");
    void clean();
    void showHelp();

    //记录每个实体被选中的传感器局部节点索引
    QMap<QString, QList<int>> m_sensorNodes;


private:
    // 定义一个结构体来管理每个独立窗口的 UI 控件
    struct GeneratorUI {
        QComboBox* shapeComboBox;
        QWidget* paramContainer;
        QVBoxLayout* paramLayout;
        QLineEdit* nameInput;
        QMap<QString, QDoubleSpinBox*> paramInputs;
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

    // 更新辅助函数，加入 GeneratorUI 参数
    void addNumParamToUI(GeneratorUI& ui, const QString& labelText, const QString& key, double defaultValue);

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
    QDockWidget *propertiesDock;
    QDockWidget *layersDock;
    QDockWidget *commandDock;
    QDockWidget *substanceDock;
    QDockWidget *boundaryDock;
    QDockWidget* interactionDock;

    CommandLine *commandLine;
    QTextEdit *propertiesEditor;
    QTreeWidget *layersTree;
    QTreeWidget *substanceTree;
    void updateSubstanceTree();
    void onSubstanceTreeContextMenu(const QPoint& pos);

    void handleDeleteEntity(QString name);

    QTreeWidget *boundaryTree;
    QTreeWidget* interactionTree;

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

    QString m_workingDirectory; // [新增] 用于保存当前的工作目录路径

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

        // ---底部实时观察面板 ---
        QListWidget* setupSummaryList; // 用于显示已添加的参数条目
        QPushButton* btnClearSummary;  // 清空列表按钮
        QPushButton* btnExportKFile;   // 最终的导出按钮

        QComboBox* presetSelector; //材料预设下拉框

        // Sensors

        QComboBox* sensorEntitySelector;
        QDoubleSpinBox* sensorX;
        QDoubleSpinBox* sensorY;
        QDoubleSpinBox* sensorZ;
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

    // ==========================================
    // 🌟 后端核心数据集与实体指针映射
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

private slots:
    void handlePresetChanged(const QString& presetName);
};
#endif // MAINWINDOW_H
