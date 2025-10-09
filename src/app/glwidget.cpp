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
    glClearColor(0.1f, 0.1f, 0.2f, 1.0f); // Dark blue background
    glEnable(GL_DEPTH_TEST);              // Enable depth testing for 3D
    glEnable(GL_CULL_FACE);               // Cull back-facing polygons for better transparency rendering
    glEnable(GL_POINT_SMOOTH);            // Render points as circles
    glEnable(GL_BLEND);                   // Enable alpha blending for transparency
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
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

    // Set up projection matrix
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixf(m_projMatrix.constData());

    // Set up model-view matrix
    glMatrixMode(GL_MODELVIEW);
    QMatrix4x4 modelMatrix;
    modelMatrix.rotate(m_rotation);
    modelMatrix.scale(m_zoom);
    QMatrix4x4 finalModelView = m_viewMatrix * modelMatrix;
    glLoadMatrixf(finalModelView.constData());

    // Draw scene elements
    drawAxes();
    if (!m_points.empty()) {
        drawPoints( m_points );
        if (!m_adjGraph.empty()) {
            drawGraph( m_adjGraph );
            if (!m_faces.empty()) {
                drawFaces( m_faces );
                if (!m_hexahedra.empty()) {
                    drawHexahedra( m_hexahedra );
                }
            }
        }
    }
}

// --- Drawing Functions ---
void GLWidget::drawAxes() {
    glLineWidth(2.0f);
    glBegin(GL_LINES);
    // X-axis (Red)
    glColor3f(1.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(2.0f, 0.0f, 0.0f);
    // Y-axis (Green)
    glColor3f(0.0f, 1.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 2.0f, 0.0f);
    // Z-axis (Blue)
    glColor3f(0.0f, 0.0f, 1.0f);
    glVertex3f(0.0f, 0.0f, 0.0f);
    glVertex3f(0.0f, 0.0f, 2.0f);
    glEnd();
    glLineWidth(1.0f);

    // Draw axis labels using QPainter as a 2D overlay
    QMatrix4x4 modelMatrix;
    modelMatrix.rotate(m_rotation);
    modelMatrix.scale(m_zoom);
    QMatrix4x4 finalModelView = m_viewMatrix * modelMatrix;
    QMatrix4x4 mvp = m_projMatrix * finalModelView;

    QPainter painter(this);
    painter.setPen(Qt::white);
    QPoint posX = project(mvp, QVector3D(2.2f, 0, 0));
    QPoint posY = project(mvp, QVector3D(0, 2.2f, 0));
    QPoint posZ = project(mvp, QVector3D(0, 0, 2.2f));
    painter.drawText(posX, "X");
    painter.drawText(posY, "Y");
    painter.drawText(posZ, "Z");
    painter.end();
}

void GLWidget::drawPoints( std::vector<MeshPoint> points ) {
    glColor3f(1.0f, 1.0f, 1.0f); // White points
    glPointSize(1.0f);
    glBegin(GL_POINTS);
    for(const auto& p : points) {
        glVertex3f(p.pos.x(), p.pos.y(), p.pos.z());
    }
    glEnd();
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
