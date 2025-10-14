#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtWidgets>
#include <vector>
#include "src/core/reconstruction/reconstruction_engine.h"
#include "src/core/generation/sphere_projection.h"
#include "src/core/generation/cylinder_projection.h"
#include "src/core/common/common_types.h"
#include "commandline.h"

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

private:
    void createMenuBar();
    void createToolBars();
    void createStatusBar();
    void createDockWidgets();
    void createCommandLine();
    void logCommand(const QString &command, const QString &response = "");
    void clean();
    void showHelp();
    QTextEdit *commandHistoryEdit;

    std::vector<Instance> drawables;

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

    CommandLine *commandLine;
    QTextEdit *propertiesEditor;
    QTreeWidget *layersTree;
    QTreeWidget *substanceTree;

    // Data containers for the reconstruction process
    //std::vector<MeshPoint> m_points;
    //std::vector<MeshPoint> m_points_sphere;

    //store all the points
    
    std::unordered_set<MeshPoint, MeshPointHasher, MeshPointComparator> points;

    inline bool contains_point(
        const std::unordered_set<MeshPoint, MeshPointHasher, MeshPointComparator>& set,
        const MeshPoint& search_point
	) {
		return set.find(search_point) != set.end();
	}

    inline bool remove_point(
        std::unordered_set<MeshPoint, MeshPointHasher, MeshPointComparator>& set,
         const MeshPoint& point_to_remove
     ) {
         // set.erase() 返回被删除的元素数量 (0 或 1)
         size_t count = set.erase(point_to_remove);

         if (count > 0) {
             return true;
         }
         else {
             return false;
         }
     }

    inline void insert_points_vector(
        std::unordered_set<MeshPoint, MeshPointHasher, MeshPointComparator>& set,
        const std::vector<MeshPoint> points) {

		for (const auto& p : points) {
			set.insert(p);
		}
    }
    
    //

    AdjacencyGraph m_adjGraph;
    AdjacencyGraph m_adjGraph_sphere;
    std::vector<QuadFace> m_faces;
    std::vector<Hexahedron> m_hexahedra;

};
#endif // MAINWINDOW_H
