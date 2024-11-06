#include "GLViewer.hpp"
#include <random>

#if defined(_DEBUG) && defined(_WIN32)
//#error "This sample should not be built in Debug mode, use RelWithDebInfo if you want to do step by step."
#endif

#define FADED_RENDERING
const float grid_size = 10.0f;

const GLchar* VERTEX_SHADER =
        "#version 330 core\n"
        "layout(location = 0) in vec3 in_Vertex;\n"
        "layout(location = 1) in vec4 in_Color;\n"
        "uniform mat4 u_mvpMatrix;\n"
        "out vec4 b_color;\n"
        "void main() {\n"
        "   b_color = in_Color;\n"
        "	gl_Position = u_mvpMatrix * vec4(in_Vertex, 1);\n"
        "}";

const GLchar* FRAGMENT_SHADER =
        "#version 330 core\n"
        "in vec4 b_color;\n"
        "layout(location = 0) out vec4 out_Color;\n"
        "void main() {\n"
        "   out_Color = b_color;\n"
        "}";

const GLchar* VERTEX_SHADER_TEXTURE =
    "#version 330 core\n"
    "layout(location = 0) in vec3 in_Vertex;\n"
    "layout(location = 1) in vec2 in_UVs;\n"
    "uniform mat4 u_mvpMatrix;\n"
    "out vec2 UV;\n"
    "void main() {\n"
    "   gl_Position = u_mvpMatrix * vec4(in_Vertex, 1);\n"
    "    UV = in_UVs;\n"
    "}\n";

const GLchar* FRAGMENT_SHADER_TEXTURE =
    "#version 330 core\n"
    "in vec2 UV;\n"
    "uniform sampler2D texture_sampler;\n"
    "void main() {\n"
    "    gl_FragColor = vec4(texture(texture_sampler, UV).bgr, 1.0);\n"
    "}\n";

void addVert(Simple3DObject &obj, float i_f, float limit, float height, sl::float4 &clr) {
    auto p1 = sl::float3(i_f, height, -limit);
    auto p2 = sl::float3(i_f, height, limit);
    auto p3 = sl::float3(-limit, height, i_f);
    auto p4 = sl::float3(limit, height, i_f);

    obj.addLine(p1, p2, clr);
    obj.addLine(p3, p4, clr);
}

GLViewer* currentInstance_ = nullptr;

GLViewer::GLViewer() : available(false) {
    currentInstance_ = this;
    mouseButton_[0] = mouseButton_[1] = mouseButton_[2] = false;
    clearInputs();
    previousMouseMotion_[0] = previousMouseMotion_[1] = 0;
}

GLViewer::~GLViewer() {
}

void GLViewer::exit() {
    if (currentInstance_ && available) {
        available = false;
        for (auto& pc : point_clouds)
        {
            pc.second.close();
        }
    }
}

bool GLViewer::isAvailable() {
    if (currentInstance_ && available) {
        glutMainLoopEvent();
    }    return available;
}

Simple3DObject createFrustum(sl::CameraParameters param) {
    // Create 3D axis
    Simple3DObject it(sl::Translation(0, 0, 0), true);

    float Z_ = -150;
    sl::float3 cam_0(0, 0, 0);
    sl::float3 cam_1, cam_2, cam_3, cam_4;

    float fx_ = 1.f / param.fx;
    float fy_ = 1.f / param.fy;

    cam_1.z = Z_;
    cam_1.x = (0 - param.cx) * Z_ *fx_;
    cam_1.y = (0 - param.cy) * Z_ *fy_;

    cam_2.z = Z_;
    cam_2.x = (param.image_size.width - param.cx) * Z_ *fx_;
    cam_2.y = (0 - param.cy) * Z_ *fy_;

    cam_3.z = Z_;
    cam_3.x = (param.image_size.width - param.cx) * Z_ *fx_;
    cam_3.y = (param.image_size.height - param.cy) * Z_ *fy_;

    cam_4.z = Z_;
    cam_4.x = (0 - param.cx) * Z_ *fx_;
    cam_4.y = (param.image_size.height - param.cy) * Z_ *fy_;

    sl::float4 clr(0.8f, 0.5f, 0.2f, 1.0f);
    it.addFace(cam_0, cam_1, cam_2, clr);
    it.addFace(cam_0, cam_2, cam_3, clr);
    it.addFace(cam_0, cam_3, cam_4, clr);
    it.addFace(cam_0, cam_4, cam_1, clr);

    it.setDrawingType(GL_TRIANGLES);
    return it;
}

void CloseFunc(void) {
    if (currentInstance_)
        currentInstance_->exit();
}

void GLViewer::init(int argc, char **argv, const std::vector<sl::CameraIdentifier>& cameras_id) {
    glutInit(&argc, argv);
    int wnd_w = glutGet(GLUT_SCREEN_WIDTH);
    int wnd_h = glutGet(GLUT_SCREEN_HEIGHT);

    glutInitWindowSize(wnd_w * 0.8, wnd_h * 0.8);
    glutInitWindowPosition(wnd_w * 0.1, wnd_h * 0.1);
    glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGBA | GLUT_DEPTH);
    glutCreateWindow("ZED Object Detection");

    GLenum err = glewInit();
    if (GLEW_OK != err)
        std::cout << "ERROR: glewInit failed: " << glewGetErrorString(err) << "\n";

    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);

    // Compile and create the shader
    shader.it.set(VERTEX_SHADER, FRAGMENT_SHADER);
    shader.MVP_Mat = glGetUniformLocation(shader.it.getProgramId(), "u_mvpMatrix");

    shaderLine.it.set(VERTEX_SHADER, FRAGMENT_SHADER);
    shaderLine.MVP_Mat = glGetUniformLocation(shaderLine.it.getProgramId(), "u_mvpMatrix");

    // Create the camera
    camera_ = CameraGL(sl::Translation(0, 2, 10), sl::Translation(0, 0, -1));

    BBox_edges = Simple3DObject(sl::Translation(0, 0, 0), false);
    BBox_edges.setDrawingType(GL_LINES);

    BBox_faces = Simple3DObject(sl::Translation(0, 0, 0), false);
    BBox_faces.setDrawingType(GL_QUADS);

    for (const sl::CameraIdentifier& cam_id : cameras_id) {
        BBox_edges_per_cam[cam_id.sn] = Simple3DObject(sl::Translation(0, 0, 0), false);
        BBox_edges_per_cam[cam_id.sn].setDrawingType(GL_LINES);

        BBox_faces_per_cam[cam_id.sn] = Simple3DObject(sl::Translation(0, 0, 0), false);
        BBox_faces_per_cam[cam_id.sn].setDrawingType(GL_QUADS);
    }

    bckgrnd_clr = sl::float4(0.2f, 0.19f, 0.2f, 1.0f);

    floor_grid = Simple3DObject(sl::Translation(0, 0, 0), true);
    floor_grid.setDrawingType(GL_LINES);

    float limit = 20.f;
    sl::float4 clr_grid(80, 80, 80, 255);
    clr_grid /= 255.f;
    float height = -3;
    for (int i = (int) (-limit); i <= (int) (limit); i++)
        addVert(floor_grid, i , limit , height , clr_grid);

    floor_grid.pushToGPU();

    std::random_device dev;
    rng = std::mt19937(dev());
    rng.seed(42);
    uint_dist360 = std::uniform_int_distribution<uint16_t>(0, 360);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glEnable(GL_LINE_SMOOTH);
    glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);

#ifndef JETSON_STYLE
    glEnable(GL_POINT_SMOOTH);
#endif

    glutSetOption(GLUT_ACTION_ON_WINDOW_CLOSE, GLUT_ACTION_CONTINUE_EXECUTION);

    // Map glut function on this class methods
    glutDisplayFunc(GLViewer::drawCallback);
    glutMouseFunc(GLViewer::mouseButtonCallback);
    glutMotionFunc(GLViewer::mouseMotionCallback);
    glutReshapeFunc(GLViewer::reshapeCallback);
    glutKeyboardFunc(GLViewer::keyPressedCallback);
    glutKeyboardUpFunc(GLViewer::keyReleasedCallback);
    glutCloseFunc(CloseFunc);

    available = true;
}

void GLViewer::render() {
    if (available) {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
        glClearColor(bckgrnd_clr.b, bckgrnd_clr.g, bckgrnd_clr.r, bckgrnd_clr.a);
        update();
        draw();
        printText();
        glutSwapBuffers();
        glutPostRedisplay();
    }
}

sl::float3 newColor(float hh) {
    float s = 0.75f;
    float v = 0.9f;

    sl::float3 clr;
    int i = (int)hh;
    float ff = hh - i;
    float p = v * (1.f - s);
    float q = v * (1.f - (s * ff));
    float t = v * (1.f - (s * (1.f - ff)));
    switch (i) {
    case 0:
        clr.r = v;
        clr.g = t;
        clr.b = p;
        break;
    case 1:
        clr.r = q;
        clr.g = v;
        clr.b = p;
        break;
    case 2:
        clr.r = p;
        clr.g = v;
        clr.b = t;
        break;

    case 3:
        clr.r = p;
        clr.g = q;
        clr.b = v;
        break;
    case 4:
        clr.r = t;
        clr.g = p;
        clr.b = v;
        break;
    case 5:
    default:
        clr.r = v;
        clr.g = p;
        clr.b = q;
        break;
    }
    return clr;
}

sl::float3 GLViewer::getColor(int id, bool for_bbox) {
    const std::lock_guard<std::mutex> lock(mtx_clr);
    if (for_bbox) {
        if (colors_bbox.find(id) == colors_bbox.end()) {
            float hh = uint_dist360(rng) / 60.f;
            colors_bbox[id] = newColor(hh);
        }
        return colors_bbox[id];
    }
    else {
        if (colors.find(id) == colors.end()) {
            int h_ = uint_dist360(rng);
            float hh = h_ / 60.f;
            colors[id] = newColor(hh);
        }
        return colors[id];
    }
}

void GLViewer::setCameraPose(int id, sl::Transform pose) {
    const std::lock_guard<std::mutex> lock(mtx);
    getColor(id, false);
    poses[id] = pose;
}

void GLViewer::updateCamera(int id, sl::Mat& view, sl::Mat& pc) {
    mtx.lock();
    auto clr = getColor(id, false);
    if (view.isInit() && viewers.find(id) == viewers.end())
        viewers[id].initialize(view, clr);

    if (pc.isInit() && point_clouds.find(id) == point_clouds.end())
        point_clouds[id].initialize(pc, clr);

    mtx.unlock();
}

void GLViewer::updateCamera(sl::Mat& pc) {
    const std::lock_guard<std::mutex> lock(mtx);
    int id = 0;
    auto clr = getColor(id, false);

    // we need to release old pc and initialize new one because fused point cloud don't have the same number of points for each process
    // I used close but it crashed in draw. Not yet investigated
    point_clouds[id].initialize(pc, clr);
}

void GLViewer::setRenderCameraProjection(sl::CameraParameters params, float znear, float zfar) {
    // Just slightly up the ZED camera FOV to make a small black border
    float fov_y = (params.v_fov + 0.5f) * M_PI / 180.f;
    float fov_x = (params.h_fov + 0.5f) * M_PI / 180.f;

    projection_(0, 0) = 1.0f / tanf(fov_x * 0.5f);
    projection_(1, 1) = 1.0f / tanf(fov_y * 0.5f);
    projection_(2, 2) = -(zfar + znear) / (zfar - znear);
    projection_(3, 2) = -1;
    projection_(2, 3) = -(2.f * zfar * znear) / (zfar - znear);
    projection_(3, 3) = 0;

    projection_(0, 0) = 1.0f / tanf(fov_x * 0.5f); //Horizontal FoV.
    projection_(0, 1) = 0;
    projection_(0, 2) = 2.0f * ((params.image_size.width - 1.0f * params.cx) / params.image_size.width) - 1.0f; //Horizontal offset.
    projection_(0, 3) = 0;

    projection_(1, 0) = 0;
    projection_(1, 1) = 1.0f / tanf(fov_y * 0.5f); //Vertical FoV.
    projection_(1, 2) = -(2.0f * ((params.image_size.height - 1.0f * params.cy) / params.image_size.height) - 1.0f); //Vertical offset.
    projection_(1, 3) = 0;

    projection_(2, 0) = 0;
    projection_(2, 1) = 0;
    projection_(2, 2) = -(zfar + znear) / (zfar - znear); //Near and far planes.
    projection_(2, 3) = -(2.0f * zfar * znear) / (zfar - znear); //Near and far planes.

    projection_(3, 0) = 0;
    projection_(3, 1) = 0;
    projection_(3, 2) = -1;
    projection_(3, 3) = 0.0f;
}

static void createBboxRendering(std::vector<sl::float3>& bbox,
                                sl::float4& bbox_clr,
                                Simple3DObject& BBox_edges,
                                Simple3DObject& BBox_faces)
{
    // First create top and bottom full edges
    BBox_edges.addFullEdges(bbox, bbox_clr);
    // Add faded vertical edges
    BBox_edges.addVerticalEdges(bbox, bbox_clr);
    // Add faces
    BBox_faces.addVerticalFaces(bbox, bbox_clr);
    // Add top face
    BBox_faces.addTopFace(bbox, bbox_clr);
}

static void addObjects(const sl::Objects objs,
                       Simple3DObject& BBox_edges,
                       Simple3DObject& BBox_faces,
                       const sl::float4 clr_id_ = sl::float4(-1.f, -1.f, -1.f, 1.f),
                       const bool draw_cross = false)
{
    for (unsigned int i = 0; i < objs.object_list.size(); i++) {
        if (renderObject(objs.object_list[i], objs.is_tracked)) {
            auto bb_ = objs.object_list[i].bounding_box;
            if (!bb_.empty()) {
                sl::float4 clr_id{clr_id_};
                if (clr_id.x == -1.f) {
                    if (objs.object_list[i].tracking_state != sl::OBJECT_TRACKING_STATE::OK) {
                        clr_id = getColorClass((int)objs.object_list[i].label);
                    }
                    else {
                        clr_id = generateColorID_f(objs.object_list[i].id);
                    }
                }

                if (draw_cross) { // display centroid as a cross
                    sl::float3 centroid = objs.object_list[i].position;
                    const float size_cendtroid = 0.005f; // m
                    sl::float3 centroid1 = centroid;
                    centroid1.y += size_cendtroid;
                    sl::float3 centroid2 = objs.object_list[i].position;
                    centroid2.y -= size_cendtroid;
                    sl::float3 centroid3 = objs.object_list[i].position;
                    centroid3.x += size_cendtroid;
                    sl::float3 centroid4 = objs.object_list[i].position;
                    centroid4.x -= size_cendtroid;

                    BBox_edges.addLine(centroid1, centroid2, sl::float4(1.f, 0.5f, 0.5f, 1.f));
                    BBox_edges.addLine(centroid3, centroid4, sl::float4(1.f, 0.5f, 0.5f, 1.f));
                }

                createBboxRendering(bb_, clr_id, BBox_edges, BBox_faces);
            }
        }
    }
}

static void addObjects(const std::unordered_map<sl::String /* Group name */, sl::Objects>& objs,
                       Simple3DObject& BBox_edges,
                       Simple3DObject& BBox_faces,
                       const sl::float4 clr_id_ = sl::float4(-1.f, -1.f, -1.f, 1.f),
                       const bool draw_cross = false)
{
    for (const std::pair<sl::String /* Group name */, sl::Objects>& it : objs) {
        if (it.second.is_new)
        {
            addObjects(it.second, BBox_edges, BBox_faces, clr_id_, draw_cross);
        }
    }
}

void GLViewer::updateObjects(const std::unordered_map<sl::String /* Group name */, sl::Objects>& objs,
                             const std::unordered_map<sl::CameraIdentifier, std::unordered_map<unsigned int /* instance id */, sl::Objects>>& singledata,
                             const sl::FusionMetrics& metrics) {
    mtx.lock();

    BBox_edges.clear();
    BBox_faces.clear();
    addObjects(objs, BBox_edges, BBox_faces);

    fusionStats.clear();
    float id = 0.5F;

    ObjectClassName obj_str;
    obj_str.name_lineA = "Publishers :" + std::to_string(metrics.mean_camera_fused);
    obj_str.name_lineB = "Sync :" + std::to_string(metrics.mean_stdev_between_camera * 1000.f);
    obj_str.color = sl::float4(0.9, 0.9, 0.9, 1);
    obj_str.position = sl::float3(10, (id * 30), 0);
    fusionStats.push_back(obj_str);

    for (auto& it : BBox_edges_per_cam)
        it.second.clear();
    for (auto& it : BBox_faces_per_cam)
        it.second.clear();

    for (const std::pair<sl::CameraIdentifier, std::unordered_map<unsigned int /* instance id */, sl::Objects>>& it : singledata) {
        auto clr = getColor(it.first.sn, false);
        ++id;

        ObjectClassName obj_str;
        obj_str.name_lineA = "CAM: " + std::to_string(it.first.sn);
        obj_str.name_lineA += " FPS: " + std::to_string(metrics.camera_individual_stats.at(it.first).received_fps);
        obj_str.name_lineB = "Ratio Detection :" + std::to_string(metrics.camera_individual_stats.at(it.first).ratio_detection);
        obj_str.name_lineB += " Delta " + std::to_string(metrics.camera_individual_stats.at(it.first).delta_ts * 1000.f);
        obj_str.color = clr;
        obj_str.position = sl::float3(10, (id * 30), 0);
        fusionStats.push_back(obj_str);

        std::swap(clr.r, clr.b);
        for (const std::pair<unsigned int /* instance id */, sl::Objects>& it2 : it.second) {
            addObjects(it2.second, BBox_edges_per_cam[it.first.sn], BBox_faces_per_cam[it.first.sn], clr);
        }
    }

    mtx.unlock();
}

void GLViewer::update() {
    if (keyStates_['q'] == KEY_STATE::UP || keyStates_['Q'] == KEY_STATE::UP || keyStates_[27] == KEY_STATE::UP) {
        currentInstance_->exit();
        return;
    }

    if (keyStates_['r'] == KEY_STATE::UP || keyStates_['R'] == KEY_STATE::UP)
        currentInstance_->show_raw = !currentInstance_->show_raw;

    if (keyStates_['f'] == KEY_STATE::UP || keyStates_['F'] == KEY_STATE::UP)
        currentInstance_->show_fused = !currentInstance_->show_fused;

    if (keyStates_['c'] == KEY_STATE::UP || keyStates_['C'] == KEY_STATE::UP)
        currentInstance_->draw_flat_color = !currentInstance_->draw_flat_color;

    if (keyStates_['s'] == KEY_STATE::UP || keyStates_['s'] == KEY_STATE::UP)
        currentInstance_->show_pc = !currentInstance_->show_pc;

    if (keyStates_['p'] == KEY_STATE::UP || keyStates_['P'] == KEY_STATE::UP || keyStates_[32] == KEY_STATE::UP)
        play = !play;

    // Rotate camera with mouse
    if (mouseButton_[MOUSE_BUTTON::LEFT]) {
        camera_.rotate(sl::Rotation((float)mouseMotion_[1] * MOUSE_R_SENSITIVITY, camera_.getRight()));
        camera_.rotate(sl::Rotation((float)mouseMotion_[0] * MOUSE_R_SENSITIVITY, camera_.getVertical() * -1.f));
    }

    // Translate camera with mouse
    if (mouseButton_[MOUSE_BUTTON::RIGHT]) {
        camera_.translate(camera_.getUp() * (float)mouseMotion_[1] * MOUSE_T_SENSITIVITY);
        camera_.translate(camera_.getRight() * (float)mouseMotion_[0] * MOUSE_T_SENSITIVITY);
    }

    // Zoom in with mouse wheel
    if (mouseWheelPosition_ != 0) {
        //float distance = sl::Translation(camera_.getOffsetFromPosition()).norm();
        if (mouseWheelPosition_ > 0 /* && distance > camera_.getZNear()*/) { // zoom
            camera_.translate(camera_.getForward() * MOUSE_UZ_SENSITIVITY * 0.5f * -1);
        }
        else if (/*distance < camera_.getZFar()*/ mouseWheelPosition_ < 0) {// unzoom
            //camera_.setOffsetFromPosition(camera_.getOffsetFromPosition() * MOUSE_DZ_SENSITIVITY);
            camera_.translate(camera_.getForward() * MOUSE_UZ_SENSITIVITY * 0.5f);
        }
    }

    camera_.update();
    mtx.lock();
    // Update point cloud buffers
    BBox_edges.pushToGPU();
    BBox_faces.pushToGPU();
    for(auto &it: BBox_edges_per_cam)
        it.second.pushToGPU();
    for(auto &it: BBox_faces_per_cam)
        it.second.pushToGPU();
   
    // Update point cloud buffers
    for (auto& it : point_clouds)
        it.second.pushNewPC();

    for (auto& it : viewers)
    {
        it.second.pushNewImage();
    }


    mtx.unlock();
    clearInputs();
}

void GLViewer::draw() {
    glEnable(GL_DEPTH_TEST);
    sl::Transform vpMatrix = camera_.getViewProjectionMatrix();

    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    glUseProgram(shader.it.getProgramId());
    glUniformMatrix4fv(shader.MVP_Mat, 1, GL_TRUE, vpMatrix.m);
    glLineWidth(1.f);
    floor_grid.draw();

    glLineWidth(1.5f);
    if (show_fused)
        BBox_edges.draw();
    if (show_raw)
        for (auto& it : BBox_edges_per_cam)
            it.second.draw();
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    if (show_fused)
        BBox_faces.draw();
    if (show_raw)
        for (auto& it : BBox_faces_per_cam)
            it.second.draw();
    glLineWidth(2.f);
    glPointSize(1.f);
    glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    for (auto& it : viewers) {
        sl::Transform pose_ = vpMatrix * poses[it.first];
        glUniformMatrix4fv(shader.MVP_Mat, 1, GL_TRUE, pose_.m);
        it.second.frustum.draw();
    }
    glUseProgram(0);

    for (auto& it : poses) {
        sl::Transform vpMatrix_world = vpMatrix * it.second;

        if (show_pc)
            if (point_clouds.find(it.first) != point_clouds.end())
                point_clouds[it.first].draw(vpMatrix_world, draw_flat_color);

        if (viewers.find(it.first) != viewers.end())
            viewers[it.first].draw(vpMatrix_world);
    }
}

sl::float2 compute3Dprojection(sl::float3& pt, const sl::Transform& cam, sl::Resolution wnd_size) {
    sl::float4 pt4d(pt.x, pt.y, pt.z, 1.);
    auto proj3D_cam = pt4d * cam;
    sl::float2 proj2D;
    proj2D.x = ((proj3D_cam.x / pt4d.w) * wnd_size.width) / (2.f * proj3D_cam.w) + wnd_size.width / 2.f;
    proj2D.y = ((proj3D_cam.y / pt4d.w) * wnd_size.height) / (2.f * proj3D_cam.w) + wnd_size.height / 2.f;
    return proj2D;
}

void GLViewer::printText() {

    sl::Resolution wnd_size(glutGet(GLUT_WINDOW_WIDTH), glutGet(GLUT_WINDOW_HEIGHT));
    for (auto& it : fusionStats) {

        sl::float2 pt2d(it.position.x, it.position.y);
        glColor4f(it.color.b, it.color.g, it.color.r, .85f);
        const auto* string = it.name_lineA.c_str();
        glWindowPos2f(pt2d.x, pt2d.y + 15);
        int len = (int)strlen(string);
        for (int i = 0; i < len; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, string[i]);

        string = it.name_lineB.c_str();
        glWindowPos2f(pt2d.x, pt2d.y);
        len = (int)strlen(string);
        for (int i = 0; i < len; i++)
            glutBitmapCharacter(GLUT_BITMAP_HELVETICA_12, string[i]);
    }
}

void GLViewer::clearInputs() {
    mouseMotion_[0] = mouseMotion_[1] = 0;
    mouseWheelPosition_ = 0;
    for (unsigned int i = 0; i < 256; ++i) {
        if (keyStates_[i] == KEY_STATE::UP)
            last_key = i;
        if (keyStates_[i] != KEY_STATE::DOWN)
            keyStates_[i] = KEY_STATE::FREE;
    }
}

void GLViewer::drawCallback() {
    currentInstance_->render();
}

void GLViewer::mouseButtonCallback(int button, int state, int x, int y) {
    if (button < 5) {
        if (button < 3) {
            currentInstance_->mouseButton_[button] = state == GLUT_DOWN;
        } else {
            currentInstance_->mouseWheelPosition_ += button == MOUSE_BUTTON::WHEEL_UP ? 1 : -1;
        }
        currentInstance_->mouseCurrentPosition_[0] = x;
        currentInstance_->mouseCurrentPosition_[1] = y;
        currentInstance_->previousMouseMotion_[0] = x;
        currentInstance_->previousMouseMotion_[1] = y;
    }
}

void GLViewer::mouseMotionCallback(int x, int y) {
    currentInstance_->mouseMotion_[0] = x - currentInstance_->previousMouseMotion_[0];
    currentInstance_->mouseMotion_[1] = y - currentInstance_->previousMouseMotion_[1];
    currentInstance_->previousMouseMotion_[0] = x;
    currentInstance_->previousMouseMotion_[1] = y;
}

void GLViewer::reshapeCallback(int width, int height) {
    glViewport(0, 0, width, height);
    float hfov = (180.0f / M_PI) * (2.0f * atan(width / (2.0f * 500)));
    float vfov = (180.0f / M_PI) * (2.0f * atan(height / (2.0f * 500)));
    currentInstance_->camera_.setProjection(hfov, vfov, currentInstance_->camera_.getZNear(), currentInstance_->camera_.getZFar());
}

void GLViewer::keyPressedCallback(unsigned char c, int x, int y) {
    currentInstance_->keyStates_[c] = KEY_STATE::DOWN;
}

void GLViewer::keyReleasedCallback(unsigned char c, int x, int y) {
    currentInstance_->keyStates_[c] = KEY_STATE::UP;
}

void GLViewer::idle() {
    glutPostRedisplay();
}


Simple3DObject::Simple3DObject(sl::Translation position, bool isStatic) : isStatic_(isStatic) {
    vaoID_ = 0;
    drawingType_ = GL_TRIANGLES;
    isStatic_ = false;
    position_ = position;
    rotation_.setIdentity();
}

Simple3DObject::~Simple3DObject() {
    clear();
    if (vaoID_ != 0) {
        glDeleteBuffers(3, vboID_);
        glDeleteVertexArrays(1, &vaoID_);
    }
}

void Simple3DObject::addBBox(std::vector<sl::float3> &pts, sl::float4 clr) {
    int start_id = vertices_.size() / 3;

    float transparency_top = 0.05f, transparency_bottom = 0.75f;
    for (unsigned int i = 0; i < pts.size(); i++) {
        addPt(pts[i]);
        clr.a = (i < 4 ? transparency_top : transparency_bottom);
        addClr(clr);
    }

    const std::vector<int> boxLinks = {4, 5, 5, 6, 6, 7, 7, 4, 0, 4, 1, 5, 2, 6, 3, 7};

    for (unsigned int i = 0; i < boxLinks.size(); i += 2) {
        indices_.push_back(start_id + boxLinks[i]);
        indices_.push_back(start_id + boxLinks[i + 1]);
    }
}

void Simple3DObject::addPt(sl::float3 pt) {
    vertices_.push_back(pt);
}

void Simple3DObject::addClr(sl::float4 clr) {
    colors_.push_back(clr);
}

void Simple3DObject::addPoint(sl::float3 pt, sl::float4 clr) {
    vertices_.push_back(pt);
    colors_.push_back(clr);
    indices_.push_back((int)indices_.size());
}

void Simple3DObject::addLine(sl::float3 p1, sl::float3 p2, sl::float4 clr) {
    addPoint(p1, clr);
    addPoint(p2, clr);
}

void Simple3DObject::addFace(sl::float3 p1, sl::float3 p2, sl::float3 p3, sl::float4 clr) {
    addPoint(p1, clr);
    addPoint(p2, clr);
    addPoint(p3, clr);
}

void Simple3DObject::addFullEdges(std::vector<sl::float3> &pts, sl::float4 clr) {
    clr.w = 0.4f;
    int start_id = vertices_.size();

    for (unsigned int i = 0; i < pts.size(); i++) {
        addPt(pts[i]);
        addClr(clr);
    }

    const std::vector<int> boxLinksTop = {0, 1, 1, 2, 2, 3, 3, 0};
    for (unsigned int i = 0; i < boxLinksTop.size(); i += 2) {
        indices_.push_back(start_id + boxLinksTop[i]);
        indices_.push_back(start_id + boxLinksTop[i + 1]);
    }

    const std::vector<int> boxLinksBottom = {4, 5, 5, 6, 6, 7, 7, 4};
    for (unsigned int i = 0; i < boxLinksBottom.size(); i += 2) {
        indices_.push_back(start_id + boxLinksBottom[i]);
        indices_.push_back(start_id + boxLinksBottom[i + 1]);
    }
}

void Simple3DObject::addVerticalEdges(std::vector<sl::float3> &pts, sl::float4 clr) {
    auto addSingleVerticalLine = [&](sl::float3 top_pt, sl::float3 bot_pt) {
        std::vector<sl::float3> current_pts{
            top_pt,
            ((grid_size - 1.0f) * top_pt + bot_pt) / grid_size,
            ((grid_size - 2.0f) * top_pt + bot_pt * 2.0f) / grid_size,
            (2.0f * top_pt + bot_pt * (grid_size - 2.0f)) / grid_size,
            (top_pt + bot_pt * (grid_size - 1.0f)) / grid_size,
            bot_pt};

        int start_id = vertices_.size();
        for (unsigned int i = 0; i < current_pts.size(); i++) {
            addPt(current_pts[i]);
            clr.a = (i == 2 || i == 3) ? 0.0f : 0.4f;
            addClr(clr);
        }

        const std::vector<int> boxLinks = {0, 1, 1, 2, 2, 3, 3, 4, 4, 5};
        for (unsigned int i = 0; i < boxLinks.size(); i += 2) {
            indices_.push_back(start_id + boxLinks[i]);
            indices_.push_back(start_id + boxLinks[i + 1]);
        }
    };

    addSingleVerticalLine(pts[0], pts[4]);
    addSingleVerticalLine(pts[1], pts[5]);
    addSingleVerticalLine(pts[2], pts[6]);
    addSingleVerticalLine(pts[3], pts[7]);
}

void Simple3DObject::addTopFace(std::vector<sl::float3> &pts, sl::float4 clr) {
    clr.a = 0.3f;
    for (auto it : pts)
        addPoint(it, clr);
}

void Simple3DObject::addVerticalFaces(std::vector<sl::float3> &pts, sl::float4 clr) {
    auto addQuad = [&](std::vector<sl::float3> quad_pts, float alpha1, float alpha2) { // To use only with 4 points
        for (unsigned int i = 0; i < quad_pts.size(); ++i) {
            addPt(quad_pts[i]);
            clr.a = (i < 2 ? alpha1 : alpha2);
            addClr(clr);
        }

        indices_.push_back((int) indices_.size());
        indices_.push_back((int) indices_.size());
        indices_.push_back((int) indices_.size());
        indices_.push_back((int) indices_.size());
    };

    // For each face, we need to add 4 quads (the first 2 indexes are always the top points of the quad)
    std::vector<std::vector<int>> quads
    {
        {
            0, 3, 7, 4
        }, // front face
        {
            3, 2, 6, 7
        }, // right face
        {
            2, 1, 5, 6
        }, // back face
        {
            1, 0, 4, 5
        } // left face
    };
    float alpha = 0.5f;

    for (const auto quad : quads) {

        // Top quads
        std::vector<sl::float3> quad_pts_1{
            pts[quad[0]],
            pts[quad[1]],
            ((grid_size - 0.5f) * pts[quad[1]] + 0.5f * pts[quad[2]]) / grid_size,
            ((grid_size - 0.5f) * pts[quad[0]] + 0.5f * pts[quad[3]]) / grid_size};
        addQuad(quad_pts_1, alpha, alpha);

        std::vector<sl::float3> quad_pts_2{
            ((grid_size - 0.5f) * pts[quad[0]] + 0.5f * pts[quad[3]]) / grid_size,
            ((grid_size - 0.5f) * pts[quad[1]] + 0.5f * pts[quad[2]]) / grid_size,
            ((grid_size - 1.0f) * pts[quad[1]] + pts[quad[2]]) / grid_size,
            ((grid_size - 1.0f) * pts[quad[0]] + pts[quad[3]]) / grid_size};
        addQuad(quad_pts_2, alpha, 2 * alpha / 3);

        std::vector<sl::float3> quad_pts_3{
            ((grid_size - 1.0f) * pts[quad[0]] + pts[quad[3]]) / grid_size,
            ((grid_size - 1.0f) * pts[quad[1]] + pts[quad[2]]) / grid_size,
            ((grid_size - 1.5f) * pts[quad[1]] + 1.5f * pts[quad[2]]) / grid_size,
            ((grid_size - 1.5f) * pts[quad[0]] + 1.5f * pts[quad[3]]) / grid_size};
        addQuad(quad_pts_3, 2 * alpha / 3, alpha / 3);

        std::vector<sl::float3> quad_pts_4{
            ((grid_size - 1.5f) * pts[quad[0]] + 1.5f * pts[quad[3]]) / grid_size,
            ((grid_size - 1.5f) * pts[quad[1]] + 1.5f * pts[quad[2]]) / grid_size,
            ((grid_size - 2.0f) * pts[quad[1]] + 2.0f * pts[quad[2]]) / grid_size,
            ((grid_size - 2.0f) * pts[quad[0]] + 2.0f * pts[quad[3]]) / grid_size};
        addQuad(quad_pts_4, alpha / 3, 0.0f);

        // Bottom quads
        std::vector<sl::float3> quad_pts_5{
            (pts[quad[1]] * 2.0f + (grid_size - 2.0f) * pts[quad[2]]) / grid_size,
            (pts[quad[0]] * 2.0f + (grid_size - 2.0f) * pts[quad[3]]) / grid_size,
            (pts[quad[0]] * 1.5f + (grid_size - 1.5f) * pts[quad[3]]) / grid_size,
            (pts[quad[1]] * 1.5f + (grid_size - 1.5f) * pts[quad[2]]) / grid_size};
        addQuad(quad_pts_5, 0.0f, alpha / 3);

        std::vector<sl::float3> quad_pts_6{
            (pts[quad[1]] * 1.5f + (grid_size - 1.5f) * pts[quad[2]]) / grid_size,
            (pts[quad[0]] * 1.5f + (grid_size - 1.5f) * pts[quad[3]]) / grid_size,
            (pts[quad[0]] + (grid_size - 1.0f) * pts[quad[3]]) / grid_size,
            (pts[quad[1]] + (grid_size - 1.0f) * pts[quad[2]]) / grid_size};
        addQuad(quad_pts_6, alpha / 3, 2 * alpha / 3);

        std::vector<sl::float3> quad_pts_7{
            (pts[quad[1]] + (grid_size - 1.0f) * pts[quad[2]]) / grid_size,
            (pts[quad[0]] + (grid_size - 1.0f) * pts[quad[3]]) / grid_size,
            (pts[quad[0]] * 0.5f + (grid_size - 0.5f) * pts[quad[3]]) / grid_size,
            (pts[quad[1]] * 0.5f + (grid_size - 0.5f) * pts[quad[2]]) / grid_size};
        addQuad(quad_pts_7, 2 * alpha / 3, alpha);

        std::vector<sl::float3> quad_pts_8{
            (pts[quad[0]] * 0.5f + (grid_size - 0.5f) * pts[quad[3]]) / grid_size,
            (pts[quad[1]] * 0.5f + (grid_size - 0.5f) * pts[quad[2]]) / grid_size,
            pts[quad[2]],
            pts[quad[3]]};
        addQuad(quad_pts_8, alpha, alpha);
    }
}

void Simple3DObject::pushToGPU() {
    if (!isStatic_ || vaoID_ == 0) {
        if (vaoID_ == 0) {
            glGenVertexArrays(1, &vaoID_);
            glGenBuffers(3, vboID_);
        }

        if (vertices_.size() > 0) {
            glBindVertexArray(vaoID_);
            glBindBuffer(GL_ARRAY_BUFFER, vboID_[0]);
            glBufferData(GL_ARRAY_BUFFER, vertices_.size() * sizeof (sl::float3), &vertices_[0], isStatic_ ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
            glVertexAttribPointer(Shader::ATTRIB_VERTICES_POS, 3, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(Shader::ATTRIB_VERTICES_POS);
        }

        if (colors_.size() > 0) {
            glBindBuffer(GL_ARRAY_BUFFER, vboID_[1]);
            glBufferData(GL_ARRAY_BUFFER, colors_.size() * sizeof (sl::float4), &colors_[0], isStatic_ ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
            glVertexAttribPointer(Shader::ATTRIB_COLOR_POS, 4, GL_FLOAT, GL_FALSE, 0, 0);
            glEnableVertexAttribArray(Shader::ATTRIB_COLOR_POS);
        }

        if (indices_.size() > 0) {
            glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboID_[2]);
            glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices_.size() * sizeof (unsigned int), &indices_[0], isStatic_ ? GL_STATIC_DRAW : GL_DYNAMIC_DRAW);
        }

        glBindVertexArray(0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
    }
}

void Simple3DObject::clear() {
    vertices_.clear();
    colors_.clear();
    indices_.clear();
}

void Simple3DObject::setDrawingType(GLenum type) {
    drawingType_ = type;
}

void Simple3DObject::draw() {
    if (indices_.size() && vaoID_) {
        glBindVertexArray(vaoID_);
        glDrawElements(drawingType_, (GLsizei) indices_.size(), GL_UNSIGNED_INT, 0);
        glBindVertexArray(0);
    }
}

void Simple3DObject::translate(const sl::Translation& t) {
    position_ = position_ + t;
}

void Simple3DObject::setPosition(const sl::Translation& p) {
    position_ = p;
}

void Simple3DObject::setRT(const sl::Transform& mRT) {
    position_ = mRT.getTranslation();
    rotation_ = mRT.getOrientation();
}

void Simple3DObject::rotate(const sl::Orientation& rot) {
    rotation_ = rot * rotation_;
}

void Simple3DObject::rotate(const sl::Rotation& m) {
    this->rotate(sl::Orientation(m));
}

void Simple3DObject::setRotation(const sl::Orientation& rot) {
    rotation_ = rot;
}

void Simple3DObject::setRotation(const sl::Rotation& m) {
    this->setRotation(sl::Orientation(m));
}

const sl::Translation& Simple3DObject::getPosition() const {
    return position_;
}

sl::Transform Simple3DObject::getModelMatrix() const {
    sl::Transform tmp;
    tmp.setOrientation(rotation_);
    tmp.setTranslation(position_);
    return tmp;
}

Shader::Shader(const GLchar* vs, const GLchar* fs) {
    set(vs, fs);
}

void Shader::set(const GLchar* vs, const GLchar* fs) {
    if (!compile(verterxId_, GL_VERTEX_SHADER, vs)) {
        std::cout << "ERROR: while compiling vertex shader" << std::endl;
    }
    if (!compile(fragmentId_, GL_FRAGMENT_SHADER, fs)) {
        std::cout << "ERROR: while compiling fragment shader" << std::endl;
    }

    programId_ = glCreateProgram();

    glAttachShader(programId_, verterxId_);
    glAttachShader(programId_, fragmentId_);

    glBindAttribLocation(programId_, ATTRIB_VERTICES_POS, "in_vertex");
    glBindAttribLocation(programId_, ATTRIB_COLOR_POS, "in_texCoord");

    glLinkProgram(programId_);

    GLint errorlk(0);
    glGetProgramiv(programId_, GL_LINK_STATUS, &errorlk);
    if (errorlk != GL_TRUE) {
        std::cout << "ERROR: while linking Shader :" << std::endl;
        GLint errorSize(0);
        glGetProgramiv(programId_, GL_INFO_LOG_LENGTH, &errorSize);

        char *error = new char[errorSize + 1];
        glGetShaderInfoLog(programId_, errorSize, &errorSize, error);
        error[errorSize] = '\0';
        std::cout << error << std::endl;

        delete[] error;
        glDeleteProgram(programId_);
    }
}

Shader::~Shader() {
    if (verterxId_ != 0 && glIsShader(verterxId_))
        glDeleteShader(verterxId_);
    if (fragmentId_ != 0 && glIsShader(fragmentId_))
        glDeleteShader(fragmentId_);
    if (programId_ != 0 && glIsProgram(programId_))
        glDeleteProgram(programId_);
}

GLuint Shader::getProgramId() {
    return programId_;
}

bool Shader::compile(GLuint &shaderId, GLenum type, const GLchar* src) {
    shaderId = glCreateShader(type);
    if (shaderId == 0) {
        std::cout << "ERROR: shader type (" << type << ") does not exist" << std::endl;
        return false;
    }
    glShaderSource(shaderId, 1, (const char**) &src, 0);
    glCompileShader(shaderId);

    GLint errorCp(0);
    glGetShaderiv(shaderId, GL_COMPILE_STATUS, &errorCp);
    if (errorCp != GL_TRUE) {
        std::cout << "ERROR: while compiling Shader :" << std::endl;
        GLint errorSize(0);
        glGetShaderiv(shaderId, GL_INFO_LOG_LENGTH, &errorSize);

        char *error = new char[errorSize + 1];
        glGetShaderInfoLog(shaderId, errorSize, &errorSize, error);
        error[errorSize] = '\0';
        std::cout << error << std::endl;

        delete[] error;
        glDeleteShader(shaderId);
        return false;
    }
    return true;
}

const GLchar* POINTCLOUD_VERTEX_SHADER =
    "#version 330 core\n"
    "layout(location = 0) in vec4 in_VertexRGBA;\n"
    "out vec4 b_color;\n"
    "uniform mat4 u_mvpMatrix;\n"
    "uniform vec3 u_color;\n"
    "uniform bool u_drawFlat;\n"
    "void main() {\n"
    "   if(u_drawFlat)\n"
    "       b_color = vec4(u_color.bgr, .85f);\n"
    "else{"
    // Decompose the 4th channel of the XYZRGBA buffer to retrieve the color of the point (1float to 4uint)
    "       uint vertexColor = floatBitsToUint(in_VertexRGBA.w); \n"
    "       vec3 clr_int = vec3((vertexColor & uint(0x000000FF)), (vertexColor & uint(0x0000FF00)) >> 8, (vertexColor & uint(0x00FF0000)) >> 16);\n"
    "       b_color = vec4(clr_int.b / 255.0f, clr_int.g / 255.0f, clr_int.r / 255.0f, .85f);\n"
    "   }"
    "   gl_Position = u_mvpMatrix * vec4(in_VertexRGBA.xyz, 1);\n"
    "}";

const GLchar* POINTCLOUD_FRAGMENT_SHADER =
    "#version 330 core\n"
    "in vec4 b_color;\n"
    "layout(location = 0) out vec4 out_Color;\n"
    "void main() {\n"
    "   out_Color = b_color;\n"
    "}";

PointCloud::PointCloud() : numBytes_(0), xyzrgbaMappedBuf_(nullptr) {

}

PointCloud::~PointCloud() {
    close();
}

void PointCloud::close() {
    if (refMat.isInit()) {
        auto err = cudaGraphicsUnmapResources(1, &bufferCudaID_, 0);
        if (err) std::cerr << "Error CUDA " << cudaGetErrorString(err) << std::endl;
        err = cudaGraphicsUnregisterResource(bufferCudaID_);
        if (err) std::cerr << "Error CUDA " << cudaGetErrorString(err) << std::endl;
        glDeleteBuffers(1, &bufferGLID_);
        refMat.free();
    }
}

void PointCloud::initialize(sl::Mat& ref, sl::float3 clr_) 
{
    refMat = ref;
    clr = clr_;
}

void PointCloud::pushNewPC() {
    if (refMat.isInit()) {

        int sizebytes = refMat.getResolution().area() * sizeof(sl::float4);
        if (numBytes_ != sizebytes) {

            if (numBytes_ == 0) {
                glGenBuffers(1, &bufferGLID_);

                shader_.set(POINTCLOUD_VERTEX_SHADER, POINTCLOUD_FRAGMENT_SHADER);
                shMVPMatrixLoc_ = glGetUniformLocation(shader_.getProgramId(), "u_mvpMatrix");
                shColor = glGetUniformLocation(shader_.getProgramId(), "u_color");
                shDrawColor = glGetUniformLocation(shader_.getProgramId(), "u_drawFlat");
            }
            else {
                cudaGraphicsUnmapResources(1, &bufferCudaID_, 0);
                cudaGraphicsUnregisterResource(bufferCudaID_);
            }

            glBindBuffer(GL_ARRAY_BUFFER, bufferGLID_);
            glBufferData(GL_ARRAY_BUFFER, sizebytes, 0, GL_STATIC_DRAW);
            glBindBuffer(GL_ARRAY_BUFFER, 0);

            cudaError_t err = cudaGraphicsGLRegisterBuffer(&bufferCudaID_, bufferGLID_, cudaGraphicsRegisterFlagsNone);
            if (err) std::cerr << "Error CUDA " << cudaGetErrorString(err) << std::endl;

            err = cudaGraphicsMapResources(1, &bufferCudaID_, 0);
            if (err) std::cerr << "Error CUDA " << cudaGetErrorString(err) << std::endl;

            err = cudaGraphicsResourceGetMappedPointer((void**)&xyzrgbaMappedBuf_, &numBytes_, bufferCudaID_);
            if (err) std::cerr << "Error CUDA " << cudaGetErrorString(err) << std::endl;
        }

        cudaMemcpy(xyzrgbaMappedBuf_, refMat.getPtr<sl::float4>(sl::MEM::CPU), numBytes_, cudaMemcpyHostToDevice);
    }
}

void PointCloud::draw(const sl::Transform& vp, bool draw_flat) {
    if (refMat.isInit()) {
#ifndef JETSON_STYLE
        glDisable(GL_BLEND);
#endif

        glUseProgram(shader_.getProgramId());
        glUniformMatrix4fv(shMVPMatrixLoc_, 1, GL_TRUE, vp.m);

        glUniform3fv(shColor, 1, clr.v);
        glUniform1i(shDrawColor, draw_flat);

        glBindBuffer(GL_ARRAY_BUFFER, bufferGLID_);
        glVertexAttribPointer(Shader::ATTRIB_VERTICES_POS, 4, GL_FLOAT, GL_FALSE, 0, 0);
        glEnableVertexAttribArray(Shader::ATTRIB_VERTICES_POS);

        glDrawArrays(GL_POINTS, 0, refMat.getResolution().area());
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glUseProgram(0);

#ifndef JETSON_STYLE
        glEnable(GL_BLEND);
#endif
    }
}


CameraViewer::CameraViewer() {

}

CameraViewer::~CameraViewer() {
    close();
}

void CameraViewer::close() {
    if (ref.isInit()) {

        auto err = cudaGraphicsUnmapResources(1, &cuda_gl_ressource, 0);
        if (err) std::cerr << "Error CUDA " << cudaGetErrorString(err) << std::endl;
        err = cudaGraphicsUnregisterResource(cuda_gl_ressource);
        if (err) std::cerr << "Error CUDA " << cudaGetErrorString(err) << std::endl;

        glDeleteTextures(1, &texture);
        glDeleteBuffers(3, vboID_);
        glDeleteVertexArrays(1, &vaoID_);
        ref.free();
    }
}

bool CameraViewer::initialize(sl::Mat& im, sl::float3 clr) {

    frustum = Simple3DObject(sl::Translation(0, 0, 0), false);

    // Create 3D axis
    float fx, fy, cx, cy;
    fx = fy = 1400;
    float width, height;
    width = 2208;
    height = 1242;
    cx = width / 2;
    cy = height / 2;

    float Z_ = .5f;
    sl::float3 toOGL(1, -1, -1);
    sl::float3 cam_0(0, 0, 0);
    sl::float3 cam_1, cam_2, cam_3, cam_4;

    float fx_ = 1.f / fx;
    float fy_ = 1.f / fy;

    cam_1.z = Z_;
    cam_1.x = (0 - cx) * Z_ * fx_;
    cam_1.y = (0 - cy) * Z_ * fy_;
    cam_1 *= toOGL;

    cam_2.z = Z_;
    cam_2.x = (width - cx) * Z_ * fx_;
    cam_2.y = (0 - cy) * Z_ * fy_;
    cam_2 *= toOGL;

    cam_3.z = Z_;
    cam_3.x = (width - cx) * Z_ * fx_;
    cam_3.y = (height - cy) * Z_ * fy_;
    cam_3 *= toOGL;

    cam_4.z = Z_;
    cam_4.x = (0 - cx) * Z_ * fx_;
    cam_4.y = (height - cy) * Z_ * fy_;
    cam_4 *= toOGL;

    frustum.addFace(cam_0, cam_1, cam_2, sl::float4(clr.b, clr.g, clr.r, 1.f));
    frustum.addFace(cam_0, cam_2, cam_3, sl::float4(clr.b, clr.g, clr.r, 1.f));
    frustum.addFace(cam_0, cam_3, cam_4, sl::float4(clr.b, clr.g, clr.r, 1.f));
    frustum.addFace(cam_0, cam_4, cam_1, sl::float4(clr.b, clr.g, clr.r, 1.f));
    frustum.setDrawingType(GL_TRIANGLES);

    frustum.pushToGPU();

    vert.push_back(cam_1);
    vert.push_back(cam_2);
    vert.push_back(cam_3);
    vert.push_back(cam_4);

    uv.push_back(sl::float2(0, 0));
    uv.push_back(sl::float2(1, 0));
    uv.push_back(sl::float2(1, 1));
    uv.push_back(sl::float2(0, 1));

    faces.push_back(sl::uint3(0, 1, 2));
    faces.push_back(sl::uint3(0, 2, 3));

    ref = im;
    shader.set(VERTEX_SHADER_TEXTURE, FRAGMENT_SHADER_TEXTURE);
    shMVPMatrixLocTex_ = glGetUniformLocation(shader.getProgramId(), "u_mvpMatrix");

    glGenVertexArrays(1, &vaoID_);
    glGenBuffers(3, vboID_);

    glBindVertexArray(vaoID_);
    glBindBuffer(GL_ARRAY_BUFFER, vboID_[0]);
    glBufferData(GL_ARRAY_BUFFER, vert.size() * sizeof(sl::float3), &vert[0], GL_STATIC_DRAW);
    glVertexAttribPointer(Shader::ATTRIB_VERTICES_POS, 3, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(Shader::ATTRIB_VERTICES_POS);

    glBindBuffer(GL_ARRAY_BUFFER, vboID_[1]);
    glBufferData(GL_ARRAY_BUFFER, uv.size() * sizeof(sl::float2), &uv[0], GL_STATIC_DRAW);
    glVertexAttribPointer(Shader::ATTRIB_COLOR_POS, 2, GL_FLOAT, GL_FALSE, 0, 0);
    glEnableVertexAttribArray(Shader::ATTRIB_COLOR_POS);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, vboID_[2]);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, faces.size() * sizeof(sl::uint3), &faces[0], GL_STATIC_DRAW);

    glBindVertexArray(0);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);

    auto res = ref.getResolution();
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, res.width, res.height, 0, GL_BGRA_EXT, GL_UNSIGNED_BYTE, NULL);
    glBindTexture(GL_TEXTURE_2D, 0);
    cudaError_t err = cudaGraphicsGLRegisterImage(&cuda_gl_ressource, texture, GL_TEXTURE_2D, cudaGraphicsRegisterFlagsWriteDiscard);
    if (err) std::cout << "err alloc " << err << " " << cudaGetErrorString(err) << "\n";
    glDisable(GL_TEXTURE_2D);

    err = cudaGraphicsMapResources(1, &cuda_gl_ressource, 0);
    if (err) std::cout << "err 0 " << err << " " << cudaGetErrorString(err) << "\n";
    err = cudaGraphicsSubResourceGetMappedArray(&ArrIm, cuda_gl_ressource, 0, 0);
    if (err) std::cout << "err 1 " << err << " " << cudaGetErrorString(err) << "\n";

    return (err == cudaSuccess);
}

void CameraViewer::pushNewImage() {
    if (!ref.isInit())  return;
    auto err = cudaMemcpy2DToArray(ArrIm, 0, 0, ref.getPtr<sl::uchar1>(sl::MEM::CPU), ref.getStepBytes(sl::MEM::CPU), ref.getPixelBytes() * ref.getWidth(), ref.getHeight(), cudaMemcpyHostToDevice);
    if (err) std::cout << "err 2 " << err << " " << cudaGetErrorString(err) << "\n";
}

void CameraViewer::draw(sl::Transform vpMatrix) {

    if (!ref.isInit())  return;

    glUseProgram(shader.getProgramId());
    glUniformMatrix4fv(shMVPMatrixLocTex_, 1, GL_FALSE, sl::Transform::transpose(vpMatrix).m);
    glBindTexture(GL_TEXTURE_2D, texture);
    glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);

    glBindVertexArray(vaoID_);
    glDrawElements(GL_TRIANGLES, (GLsizei)faces.size() * 3, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    glUseProgram(0);
}

const sl::Translation CameraGL::ORIGINAL_FORWARD = sl::Translation(0, 0, 1);
const sl::Translation CameraGL::ORIGINAL_UP = sl::Translation(0, 1, 0);
const sl::Translation CameraGL::ORIGINAL_RIGHT = sl::Translation(1, 0, 0);

CameraGL::CameraGL(sl::Translation position, sl::Translation direction, sl::Translation vertical) {
    this->position_ = position;
    setDirection(direction, vertical);

    offset_ = sl::Translation(0, 0, 0);
    view_.setIdentity();
    updateView();
    setProjection(70, 70, 0.200f, 50.f);
    updateVPMatrix();
}

CameraGL::~CameraGL() {
}

void CameraGL::update() {
    if (sl::Translation::dot(vertical_, up_) < 0)
        vertical_ = vertical_ * -1.f;
    updateView();
    updateVPMatrix();
}

void CameraGL::setProjection(float horizontalFOV, float verticalFOV, float znear, float zfar) {
    horizontalFieldOfView_ = horizontalFOV;
    verticalFieldOfView_ = verticalFOV;
    znear_ = znear;
    zfar_ = zfar;

    float fov_y = verticalFOV * M_PI / 180.f;
    float fov_x = horizontalFOV * M_PI / 180.f;

    projection_.setIdentity();
    projection_(0, 0) = 1.0f / tanf(fov_x * 0.5f);
    projection_(1, 1) = 1.0f / tanf(fov_y * 0.5f);
    projection_(2, 2) = -(zfar + znear) / (zfar - znear);
    projection_(3, 2) = -1;
    projection_(2, 3) = -(2.f * zfar * znear) / (zfar - znear);
    projection_(3, 3) = 0;
}

const sl::Transform& CameraGL::getViewProjectionMatrix() const {
    return vpMatrix_;
}

float CameraGL::getHorizontalFOV() const {
    return horizontalFieldOfView_;
}

float CameraGL::getVerticalFOV() const {
    return verticalFieldOfView_;
}

void CameraGL::setOffsetFromPosition(const sl::Translation& o) {
    offset_ = o;
}

const sl::Translation& CameraGL::getOffsetFromPosition() const {
    return offset_;
}

void CameraGL::setDirection(const sl::Translation& direction, const sl::Translation& vertical) {
    sl::Translation dirNormalized = direction;
    dirNormalized.normalize();
    this->rotation_ = sl::Orientation(ORIGINAL_FORWARD, dirNormalized * -1.f);
    updateVectors();
    this->vertical_ = vertical;
    if (sl::Translation::dot(vertical_, up_) < 0)
        rotate(sl::Rotation(M_PI, ORIGINAL_FORWARD));
}

void CameraGL::translate(const sl::Translation& t) {
    position_ = position_ + t;
}

void CameraGL::setPosition(const sl::Translation& p) {
    position_ = p;
}

void CameraGL::rotate(const sl::Orientation& rot) {
    rotation_ = rot * rotation_;
    updateVectors();
}

void CameraGL::rotate(const sl::Rotation& m) {
    this->rotate(sl::Orientation(m));
}

void CameraGL::setRotation(const sl::Orientation& rot) {
    rotation_ = rot;
    updateVectors();
}

void CameraGL::setRotation(const sl::Rotation& m) {
    this->setRotation(sl::Orientation(m));
}

const sl::Translation& CameraGL::getPosition() const {
    return position_;
}

const sl::Translation& CameraGL::getForward() const {
    return forward_;
}

const sl::Translation& CameraGL::getRight() const {
    return right_;
}

const sl::Translation& CameraGL::getUp() const {
    return up_;
}

const sl::Translation& CameraGL::getVertical() const {
    return vertical_;
}

float CameraGL::getZNear() const {
    return znear_;
}

float CameraGL::getZFar() const {
    return zfar_;
}

void CameraGL::updateVectors() {
    forward_ = ORIGINAL_FORWARD * rotation_;
    up_ = ORIGINAL_UP * rotation_;
    right_ = sl::Translation(ORIGINAL_RIGHT * -1.f) * rotation_;
}

void CameraGL::updateView() {
    sl::Transform transformation(rotation_, (offset_ * rotation_) + position_);
    view_ = sl::Transform::inverse(transformation);
}

void CameraGL::updateVPMatrix() {
    vpMatrix_ = projection_ * view_;
}