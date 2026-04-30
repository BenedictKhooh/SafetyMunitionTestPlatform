#pragma comment(lib, "opengl32.lib")
#pragma comment(lib, "glu32.lib")
#include "glwidget.h"
#include <QDebug>
#include <QPainter>

GLWidget::GLWidget(QWidget *parent)
    : QOpenGLWidget(parent), m_zoom(1.0f) {
    // Initialize the camera's view matrix, moving it back from the origin.
    m_viewMatrix.translate(0.0f, 0.0f, -5.0f);
}

GLWidget::~GLWidget() {}

// --- Data Setter Functions ---
void GLWidget::setPoints(const std::vector<MeshPoint>& points) { m_points = points; update(); }
void GLWidget::setAdjacencyGraph(const AdjacencyGraph& graph) { m_adjGraph = graph; update(); }
void GLWidget::setFaces(const std::vector<QuadFace>& faces) { m_faces = faces; update(); }
void GLWidget::setHexahedra(const std::vector<Hexahedron>& hexahedra) { m_hexahedra = hexahedra; update(); }

// Resets all data to clear the view.
void GLWidget::reset() {
    m_points.clear(); m_adjGraph.clear(); m_faces.clear(); m_hexahedra.clear();
    update();
}

// --- OpenGL Functions ---
void GLWidget::initializeGL() {
    initializeOpenGLFunctions();
    glClearColor(0.1f, 0.15f, 0.2f, 1.0f); 
    glEnable(GL_DEPTH_TEST);              // Enable depth testing for 3D
    glEnable(GL_CULL_FACE);               // Cull back-facing polygons for better transparency rendering
    glEnable(GL_POINT_SMOOTH);            // Render points as circles
    glEnable(GL_BLEND);                   // Enable alpha blending for transparency
    glDepthFunc(GL_LEQUAL);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    // 2. Create VBO
    m_pointVBO.create();
    m_pointVBO.bind();

    // 3. Set a default initial size (or wait to allocate in drawPoints)
    // Here, we just allocate, but data will be sent later in drawPoints
    m_pointVBO.allocate(nullptr, 0);

    m_pointVBO.release();
}

void GLWidget::resizeGL(int w, int h) {
    // Set up the perspective projection matrix.
    m_projMatrix.setToIdentity();
    m_projMatrix.perspective(45.0f, GLfloat(w) / GLfloat(h ? h : 1), 0.1f, 100.0f);
}

// Helper function to project a 3D world point to 2D screen coordinates.
QPoint GLWidget::project(const QMatrix4x4 &mvp, const QVector3D &point3d) {
    QVector4D clipPoint = mvp * QVector4D(point3d, 1.0f);
    if (qFuzzyCompare(clipPoint.w(), 0.0f)) return QPoint(-1, -1); // Avoid division by zero

    // Perspective division
    QVector3D ndcPoint = clipPoint.toVector3D() / clipPoint.w();

    // Viewport transform
    float winX = (ndcPoint.x() * 0.5f + 0.5f) * width();
    float winY = (1.0f - (ndcPoint.y() * 0.5f + 0.5f)) * height(); // Y is inverted in Qt
    return QPoint(static_cast<int>(winX), static_cast<int>(winY));
}

void GLWidget::paintGL() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    // 设置投影矩阵
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(m_projMatrix.constData());

    // 设置模型视图矩阵
    glMatrixMode(GL_MODELVIEW);
    QMatrix4x4 modelMatrix;
    modelMatrix.rotate(m_rotation);
    modelMatrix.scale(m_zoom);
    QMatrix4x4 finalModelView = m_viewMatrix * modelMatrix;
    glLoadMatrixf(finalModelView.constData());

    if (!m_repository) return;

    const auto& allEntities = m_repository->getAllEntities();

    // 护眼调色板
    std::vector<QVector3D> colorPalette = {
        {0.2f, 0.6f, 1.0f}, // 天蓝色
        {1.0f, 0.5f, 0.2f}, // 橙色
        {0.2f, 0.8f, 0.4f}, // 翠绿色
        {0.8f, 0.3f, 0.8f}, // 紫色
        {0.9f, 0.2f, 0.2f}, // 红色
        {0.0f, 0.8f, 0.8f}, // 青色
        {0.9f, 0.8f, 0.1f}, // 金黄色
        {0.9f, 0.5f, 0.7f}  // 粉色
    };

    // 开启深度测试，确保前面的不透明面能遮挡后面的面
    glEnable(GL_DEPTH_TEST);

    int entityIndex = 0;

    // 遍历仓库，挨个画出来
    for (auto it = allEntities.begin(); it != allEntities.end(); ++it) {
        const MeshEntity& entity = it->second;
        QVector3D meshColor = colorPalette[entityIndex % colorPalette.size()];

        // ====================================================
        // 🌟 新增核心功能：画不透明的实心面 (Solid Faces)
        // ====================================================
        glEnable(GL_POLYGON_OFFSET_FILL);
        glPolygonOffset(1.0f, 1.0f); // 稍微向后偏移面，防止和后续的线框重合闪烁 (Z-fighting)
        glBegin(GL_QUADS);

        // 填色！最后的 1.0f 代表 Alpha 通道完全不透明
        glColor4f(meshColor.x(), meshColor.y(), meshColor.z(), 1.0f);

        for (const auto& hex : entity.hexes) {
            // 定义六面体的 6 个面 (注意点的缠绕顺序)
            int faces[6][4] = {
                {hex[0], hex[3], hex[2], hex[1]}, {hex[4], hex[5], hex[6], hex[7]},
                {hex[0], hex[4], hex[7], hex[3]}, {hex[1], hex[2], hex[6], hex[5]},
                {hex[0], hex[1], hex[5], hex[4]}, {hex[3], hex[7], hex[6], hex[2]}
            };
            for (int i = 0; i < 6; ++i) {
                for (int j = 0; j < 4; ++j) {
                    int idx = faces[i][j];
                    if (idx >= 0 && idx < entity.nodes.size()) {
                        glVertex3f(entity.nodes[idx].pos.x(), entity.nodes[idx].pos.y(), entity.nodes[idx].pos.z());
                    }
                }
            }
        }
        glEnd();
        glDisable(GL_POLYGON_OFFSET_FILL);


        // ====================================================
        // 画线 (Wireframe Lines)：描出深色边框增强立体感
        // ====================================================
        glLineWidth(1.5f);
        glBegin(GL_LINES);
        // 使用深灰色画线，而不是纯色，否则和面混在一起看不清网格结构
        glColor3f(0.15f, 0.15f, 0.15f);

        for (const auto& linePos : entity.wireLines) {
            glVertex3f(linePos.x(), linePos.y(), linePos.z());
        }
        glEnd();

        entityIndex++;
    }

    drawCornerAxes();
}

void GLWidget::drawPoints( std::vector<MeshPoint> points ) {
    glColor3f(1.0f, 9.0f, 1.0f); // White points
    glPointSize(1.0f);
    glBegin(GL_POINTS);
    for(const auto& p : points) {
        glVertex3f(p.pos.x(), p.pos.y(), p.pos.z());
    }
    glEnd();

    // Replace the line "updateGL();" with the following line to fix the error:
    update(); // update() is the correct method to trigger a repaint in QOpenGLWidget
}

void GLWidget::drawPoints(std::vector<Vector3> points) {
    glColor3f(1.0f, 1.0f, 1.0f); // White points
    glPointSize(1.0f);
    glBegin(GL_POINTS);
    for (const auto& p : points) {
        glVertex3f(p.x(), p.y(), p.z());
    }
    glEnd();

    // Replace the line "updateGL();" with the following line to fix the error:
    update(); // update() is the correct method to trigger a repaint in QOpenGLWidget
}

void GLWidget::drawGraph( AdjacencyGraph adjGraph ) {
    glColor3f(0.5f, 0.5f, 0.6f); // Grey lines
    glLineWidth(1.0f);
    glBegin(GL_LINES);
    for(const auto& pair : adjGraph) {
        if (pair.first >= (int)m_points.size()) continue;
        const Vector3& p1 = m_points[pair.first].pos;
        for(int neighbor_idx : pair.second) {
            if (neighbor_idx >= (int)m_points.size()) continue;
            const Vector3& p2 = m_points[neighbor_idx].pos;
            glVertex3f(p1.x(), p1.y(), p1.z());
            glVertex3f(p2.x(), p2.y(), p2.z());
        }
    }
    glEnd();
}

void GLWidget::drawFaces( std::vector<QuadFace> faces ) {
    glColor4f(0.2f, 0.5f, 1.0f, 0.1f); // Translucent blue faces
    for(const auto& face : faces) {
        glBegin(GL_QUADS);
        for(int i = 0; i < 4; ++i) {
            if (face[i] >= (int)m_points.size()) { glEnd(); return; }
            const auto& p = m_points[face[i]].pos;
            glVertex3f(p.x(), p.y(), p.z());
        }
        glEnd();
    }
}

void GLWidget::drawHexahedra( std::vector<Hexahedron> hexahedra ) {
    glColor4f(1.0f, 0.3f, 0.3f, 0.1f); // Translucent red for final hexes
    for(const auto& hex : hexahedra) {
        // Define faces with correct winding order for culling
        QuadFace faces[] = {
            {hex[0], hex[3], hex[2], hex[1]}, {hex[4], hex[5], hex[6], hex[7]},
            {hex[0], hex[4], hex[7], hex[3]}, {hex[1], hex[2], hex[6], hex[5]},
            {hex[0], hex[1], hex[5], hex[4]}, {hex[3], hex[7], hex[6], hex[2]}
        };
        for(const auto& face : faces) {
             glBegin(GL_QUADS);
             for(int idx : face) {
                 if (idx >= (int)m_points.size()) { glEnd(); continue; }
                 glVertex3f(m_points[idx].pos.x(), m_points[idx].pos.y(), m_points[idx].pos.z());
             }
             glEnd();
        }
    }
}

// --- Event Handlers for Camera ---
void GLWidget::mousePressEvent(QMouseEvent *event) {
    m_lastMousePos = QVector2D(event->pos());
}

void GLWidget::mouseMoveEvent(QMouseEvent *event) {
    QVector2D currentPos = QVector2D(event->pos());
    QVector2D diff = currentPos - m_lastMousePos;
    if (event->buttons() & Qt::LeftButton) { // Rotation
        QQuaternion rotX = QQuaternion::fromAxisAndAngle(0.0f, 1.0f, 0.0f, 0.5f * diff.x());
        QQuaternion rotY = QQuaternion::fromAxisAndAngle(1.0f, 0.0f, 0.0f, 0.5f * diff.y());
        m_rotation = rotX * rotY * m_rotation;
        update();
    }
    m_lastMousePos = currentPos;
}

void GLWidget::wheelEvent(QWheelEvent *event) {
    // Zoom in/out
    if (event->angleDelta().y() > 0) m_zoom *= 1.1f;
    else m_zoom *= 0.9f;
    update();
}

// --- Drawable Command Interface ---

// glwidget.cpp
void GLWidget::submitDrawCommand(const DrawCommand& cmd) {
    m_drawCommands.push_back(cmd);
    update();
}

void GLWidget::clearDrawCommands() {
    m_drawCommands.clear();
    update();
}

void GLWidget::drawLines(const std::vector<Vector3>& points) {
    if (points.empty()) return;

    glColor4f(0.0f, 0.0f, 255.0f, 0.3f);

    glLineWidth(0.5f);
    glBegin(GL_LINES);
    for (const auto& p : points) {
        glVertex3f(p.x(), p.y(), p.z());
    }
    glEnd();

    update(); // 触发重绘
}

// ==========================================
// [更新] 绘制左下角固定悬浮坐标轴，带文字标注 (X Y Z)
// ==========================================
void GLWidget::drawCornerAxes() {
    // 1. 保存当前的视口 (Viewport)
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT, viewport);

    // 2. 将绘图区域限制在左下角的一个小正方形 (120x120 像素)
    glViewport(20, 20, 120, 120);

    // 3. 切换到投影矩阵并保存
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
    glLoadIdentity();
    // 使用正交投影，保证坐标轴和文字大小绝对固定
    glOrtho(-1.3, 1.3, -1.3, 1.3, -10.0, 10.0); // 稍微放大一点投影范围给文字留空间

    // 4. 切换到模型视图矩阵并保存
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
    glLoadIdentity();

    // 核心逻辑：直接应用你代码中的四元数旋转 (m_rotation)
    QMatrix4x4 rotMatrix;
    rotMatrix.rotate(m_rotation);
    glMultMatrixf(rotMatrix.constData());

    // 关闭深度测试，确保坐标轴和文字永远浮在最上层
    glDisable(GL_DEPTH_TEST);

    // ------------------------------------------
    // A. 绘制 XYZ 坐标轴的线条 (加粗)
    // ------------------------------------------
    glLineWidth(3.0f);
    glBegin(GL_LINES);
    // X 轴 (红色)
    glColor3f(1.0f, 0.2f, 0.2f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(1.0f, 0.0f, 0.0f);
    // Y 轴 (绿色)
    glColor3f(0.2f, 1.0f, 0.2f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 1.0f, 0.0f);
    // Z 轴 (蓝色)
    glColor3f(0.2f, 0.6f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 1.0f);
    glEnd();

    // ------------------------------------------
    // B. [新增] 绘制文字标注 (X Y Z) 使用线条手工绘制
    // ------------------------------------------
    glLineWidth(2.0f); // 文字线条稍微细一点
    float size = 0.08f;  // 字母的大小
    float offset = 1.15f; // 字母距离原点的偏移量 (刚好在轴末端外面)

    glBegin(GL_LINES);

    // --- 绘制 'X' (在 X 轴末端) - 红色 ---
    glColor3f(1.0f, 0.2f, 0.2f);
    // 线条 1 (左上到右下)
    glVertex3f(offset - size / 2, size / 2, 0.0f);
    glVertex3f(offset + size / 2, -size / 2, 0.0f);
    // 线条 2 (右上到左下)
    glVertex3f(offset + size / 2, size / 2, 0.0f);
    glVertex3f(offset - size / 2, -size / 2, 0.0f);

    // --- 绘制 'Y' (在 Y 轴末端) - 绿色 ---
    glColor3f(0.2f, 1.0f, 0.2f);
    // 左上臂
    glVertex3f(-size / 2, offset + size / 2, 0.0f);
    glVertex3f(0.0f, offset, 0.0f);
    // 右上臂
    glVertex3f(size / 2, offset + size / 2, 0.0f);
    glVertex3f(0.0f, offset, 0.0f);
    // 下部直干
    glVertex3f(0.0f, offset, 0.0f);
    glVertex3f(0.0f, offset - size / 2, 0.0f);

    // --- 绘制 'Z' (在 Z 轴末端) - 蓝色 ---
    glColor3f(0.2f, 0.6f, 1.0f);
    // 顶部横线
    glVertex3f(-size / 2, 0.0f, offset + size / 2);
    glVertex3f(size / 2, 0.0f, offset + size / 2);
    // 斜线 (右上到左下)
    glVertex3f(size / 2, 0.0f, offset + size / 2);
    glVertex3f(-size / 2, 0.0f, offset - size / 2);
    // 底部横线
    glVertex3f(-size / 2, 0.0f, offset - size / 2);
    glVertex3f(size / 2, 0.0f, offset - size / 2);

    glEnd();

    // ------------------------------------------
    // 6. 还原现场
    // ------------------------------------------
    glLineWidth(1.0f); // 恢复默认线宽
    glEnable(GL_DEPTH_TEST);

    glPopMatrix(); // 弹出模型视图矩阵
    glMatrixMode(GL_PROJECTION);
    glPopMatrix(); // 弹出投影矩阵
    glMatrixMode(GL_MODELVIEW); // 恢复默认矩阵模式

    // 恢复原来的视口
    glViewport(viewport[0], viewport[1], viewport[2], viewport[3]);
}