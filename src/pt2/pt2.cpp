#include <pt2/pt2.h>

#include <random>
#include <memory>
#include <queue>
#include <chrono>
#include <iostream>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION

#include <stb/stb_image.h>

#define STB_IMAGE_WRITE_IMPLEMENTATION

#include <stb/stb_write.h>

#define TINYOBJLOADER_IMPLEMENTATION

#include <tinyobj/tinyobjloader.h>

#include <embree3/rtcore.h>

#include <imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

#include <pt2/imgui_custom.h>
#include <pt2/sampling.h>

namespace
{
    [[nodiscard]] float rand_float()
    {
        thread_local static std::mt19937                          gen;
        thread_local static std::uniform_real_distribution<float> dist(0.f, 1.f);
        return dist(gen);
    }
}    // namespace

namespace PT2
{
    Renderer::Renderer() : _render_pool(16) { _initialize(); }

    Renderer::~Renderer() = default;

    void Renderer::start_gui() { _handle_window(); }

    void Renderer::_initialize()
    {
        _device   = rtcNewDevice(nullptr);
        _scene    = rtcNewScene(_device);
        _geometry = rtcNewGeometry(_device, RTC_GEOMETRY_TYPE_TRIANGLE);
        rtcCommitGeometry(_geometry);
        rtcAttachGeometry(_scene, _geometry);
        rtcCommitScene(_scene);
    }

    void Renderer::_read_file(const std::string &path, std::string &contents)
    {
        constexpr auto read_size = std::size_t { 4096 };
        auto           stream    = std::ifstream { path.data() };
        stream.exceptions(std::ios_base::badbit);

        auto out = std::string {};
        auto buf = std::string(read_size, '\0');
        while (stream.read(&buf[0], read_size)) { out.append(buf, 0, stream.gcount()); }
        out.append(buf, 0, stream.gcount());
        contents = out;
    }

    void Renderer::_handle_window()
    {
        // GLFW - Initialize / Configre
        glfwInit();

        // GLFW - Window creation
        const auto mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
        _window = glfwCreateWindow(mode->width, mode->height, "PT2 - Rewrite", nullptr, nullptr);
        if (!_window)
        {
            std::cerr << "Failed to create window" << std::endl;
            glfwTerminate();
            exit(-1);
        }
        glfwMakeContextCurrent(_window);

        // Glad - Load function pointers
        const auto res = gladLoadGLLoader((GLADloadproc) glfwGetProcAddress);
        if (!res)
        {
            std::cerr << "Failed to initialize GLAD" << std::endl;
            exit(-1);
        }

        // Triangle Setup
        const float triangle_vertices[] = {
            -1.f, -1.f,  0.f,    // bottom left
            -1.f, 4.f,   0.f,    // top left
            4.f,  -1.0f, 0.f,    // bottom right
        };
        // Vertex buffer object
        unsigned int VBO;
        glCreateBuffers(1, &VBO);
        glNamedBufferStorage(
          VBO,
          sizeof(triangle_vertices),
          triangle_vertices,
          GL_DYNAMIC_STORAGE_BIT);

        // Vertex array object
        unsigned int VAO;
        glCreateVertexArrays(1, &VAO);
        GLuint vao_binding_point = 0;
        glVertexArrayVertexBuffer(VAO, vao_binding_point, VBO, 0, 3 * sizeof(float));
        glEnableVertexArrayAttrib(VAO, 0);
        glVertexArrayAttribFormat(VAO, 0, 3, GL_FLOAT, false, 0);
        glVertexArrayAttribBinding(VAO, 0, vao_binding_point);

        // Shaders
        std::string frag_shader;
        _read_file("./assets/shaders/shader.frag", frag_shader);
        GLuint fragment_shader;
        fragment_shader      = glCreateShader(GL_FRAGMENT_SHADER);
        const auto *frag_ptr = frag_shader.c_str();
        glShaderSource(fragment_shader, 1, &frag_ptr, nullptr);
        glCompileShader(fragment_shader);
        int params = -1;
        glGetShaderiv(fragment_shader, GL_COMPILE_STATUS, &params);
        if (GL_TRUE != params)
        {
            std::cerr << "There was an error compiled the fragment shader" << std::endl;
            exit(-1);
        }

        std::string vert_shader;
        _read_file("./assets/shaders/shader.vert", vert_shader);
        GLuint vertex_shader;
        vertex_shader        = glCreateShader(GL_VERTEX_SHADER);
        const auto *vert_ptr = vert_shader.c_str();
        glShaderSource(vertex_shader, 1, &vert_ptr, nullptr);
        glCompileShader(vertex_shader);
        params = -1;
        glGetShaderiv(vertex_shader, GL_COMPILE_STATUS, &params);
        if (GL_TRUE != params)
        {
            std::cerr << "There was an error compiled the vertex shader" << std::endl;
            exit(-1);
        }

        // Program setup
        GLuint program = glCreateProgram();
        glAttachShader(program, fragment_shader);
        glAttachShader(program, vertex_shader);
        glLinkProgram(program);
        glUseProgram(program);

        // Store the window size
        glfwGetWindowSize(_window, &_screen_resolution.x, &_screen_resolution.y);

        // Setup mouse callback (Currently only used for material selector)
        glfwSetWindowUserPointer(_window, this);
        glfwSetMouseButtonCallback(
          _window,
          [](GLFWwindow *window, int button, int action, int mods) {
              const auto window_ptr = (PT2::Renderer *) glfwGetWindowUserPointer(
                window);    // Yes I know C style cast leave me alone
              const auto click_type = button == GLFW_MOUSE_BUTTON_RIGHT ? ClickType::RIGHT
              : button == GLFW_MOUSE_BUTTON_LEFT                        ? ClickType::LEFT
                                                                        : ClickType::UNKNOWN;
              auto       cursor_x   = 0.0;
              auto       cursor_y   = 0.0;
              glfwGetCursorPos(window, &cursor_x, &cursor_y);
              if (click_type != ClickType::UNKNOWN && action == GLFW_PRESS)
                  window_ptr->_handle_mouse(cursor_x, cursor_y, click_type);
          });

        // Update the rendering context
        _rendering_context.gl_program         = program;
        _rendering_context.gl_fragment_shader = fragment_shader;
        _rendering_context.gl_vertex_shader   = vertex_shader;
        _rendering_context.resolution_x       = mode->width;
        _rendering_context.resolution_y       = mode->height;

        // Setup the texture for displaying the result of the ray tracing buffer
        _ray_tracing_context.buffer.reserve(
          _ray_tracing_context.resolution.x * _ray_tracing_context.resolution.y * 3);

        GLuint texture = 0;

        glGenTextures(1, &texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(
          GL_TEXTURE_2D,
          0,
          GL_RGBA8,
          _ray_tracing_context.resolution.x,
          _ray_tracing_context.resolution.y,
          0,
          GL_RGBA,
          GL_UNSIGNED_BYTE,
          _ray_tracing_context.buffer.data());
        glGenerateMipmap(GL_TEXTURE_2D);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(_window, true);
        ImGui_ImplOpenGL3_Init("#version 150");

        _render_screen();
        while (!glfwWindowShouldClose(_window))
        {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            {
                ImGui::Begin("Display Options");
                ImGui::Text("Texture Display Offset");
                ImGui::SliderFloat("Scale X", &_render_target_setting.x_scale, 0.0f, 1.0f);
                ImGui::SliderFloat("Scale Y", &_render_target_setting.y_scale, 0.0f, 1.0f);
                ImGui::End();
            }

            {
                ImGui::Begin("Material Editor");
                if (ImGui::BeginCombo(
                      "Edit Material",
                      _selected_material == nullptr ? nullptr : _selected_material->name.c_str()))
                {
                    for (auto &mat : _loaded_materials)
                    {
                        const auto selected = ImGui::Selectable(mat.name.c_str());
                        if (selected) _selected_material = &mat;
                    }
                    ImGui::EndCombo();
                }

                if (_selected_material != nullptr && !_selected_material->name.empty())
                {
                    std::string material_type = [&]() {
                        if (_selected_material->type == Material::DIFFUSE)
                            return "Diffuse";
                        else if (_selected_material->type == Material::REFRACTIVE)
                            return "Refractive";
                        else if (_selected_material->type == Material::MIRROR)
                            return "Mirror";
                        else if (_selected_material->type == Material::METAL)
                            return "Metal";
                        else
                            return "Unknown";
                    }();
                    if (ImGui::BeginCombo("Material Type", material_type.c_str()))
                    {
                        if (ImGui::Selectable("Diffuse"))
                            _selected_material->type = Material::DIFFUSE;
                        if (ImGui::Selectable("Refractive"))
                            _selected_material->type = Material::REFRACTIVE;
                        if (ImGui::Selectable("Mirror"))
                            _selected_material->type = Material::MIRROR;
                        if (ImGui::Selectable("Metal")) _selected_material->type = Material::METAL;
                        ImGui::EndCombo();
                    }
                    ImGui::ColorEdit3("Material Color", &_selected_material->color[0]);
                    ImGui::SliderFloat("Emission", &_selected_material->emission, 0.0f, 1.0f);
                    switch (_selected_material->type)
                    {
                    case Material::DIFFUSE:
                        _selected_material->reflectiveness = 1.0f / 3.1415f;
                        break;
                    case Material::REFRACTIVE:
                        ImGui::SliderFloat("Roughness", &_selected_material->roughness, 0.0f, 1.0f);
                        ImGui::SliderFloat("IOR", &_selected_material->ior, 1.0f, 3.0f);

                        break;
                    case Material::MIRROR:
                        ImGui::SliderFloat(
                          "Reflectiveness",
                          &_selected_material->reflectiveness,
                          0.0f,
                          1.0f);

                        break;
                    case Material::METAL:
                        ImGui::SliderFloat(
                          "Reflectiveness",
                          &_selected_material->reflectiveness,
                          0.0f,
                          1.0f);
                        ImGui::SliderFloat("Roughness", &_selected_material->roughness, 0.0f, 1.0f);
                        break;
                    }
                }
                ImGui::End();
            }

            {
                ImGui::Begin("Envmap Loader");
                static std::filesystem::path selectedImage;
                if (ImGui::BeginCombo("Selected Image", selectedImage.filename().c_str()))
                {
                    for (const auto &directory_entry :
                         std::filesystem::directory_iterator("./assets/images"))
                    {
                        if (directory_entry.is_regular_file())
                        {
                            const auto selected =
                              ImGui::Selectable(directory_entry.path().filename().c_str());
                            if (selected) selectedImage = directory_entry.path();
                        }
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Button("Load Background") && selectedImage != std::filesystem::path())
                {
                    int   width, height, components;
                    auto *data = stbi_load(selectedImage.c_str(), &width, &height, &components, 3);
                    _envmap    = Image();
                    _envmap->data = std::vector<uint8_t>();
                    _envmap->data.resize(width * height * 3, 0);
                    std::memcpy(_envmap->data.data(), data, width * height * 3);
                    _envmap->width  = width;
                    _envmap->height = height;
                    stbi_image_free(data);
                    _render_pool.stop();
                    _render_pool.start();
                    _render_screen();
                }
                ImGui::End();
            }

            {
                ImGui::Begin("Model Loader");
                static std::filesystem::path selectedModel;
                static bool                  idk;
                if (ImGui::BeginCombo("Selected Model", selectedModel.filename().c_str()))
                {
                    for (const auto &directory_entry :
                         std::filesystem::directory_iterator("./assets/models"))
                    {
                        if (directory_entry.is_regular_file())
                        {
                            const auto selected =
                              ImGui::Selectable(directory_entry.path().filename().c_str(), &idk);
                            if (selected) selectedModel = directory_entry.path();
                        }
                    }
                    ImGui::EndCombo();
                }

                if (ImGui::Button("Add Model") && selectedModel != std::filesystem::path())
                {
                    _render_pool.stop();
                    load_model(selectedModel, ModelType::OBJ);
                    _render_pool.start();
                    _render_screen();
                }
                ImGui::End();
            }

            {
                static auto ctx             = RayTracingContext();
                static auto camera_position = glm::vec3(-15, 12, 8);
                static auto camera_look_at  = glm::vec3(0, 0, 0);
                static auto fov             = 90.f;

                ImGui::Begin("Rendering Context");
                ImGui::InputInt("Max Bounces", &ctx.bounces, 1, 5);
                ImGui::InputInt("Samples Per Pixel", &ctx.spp, 1, 2);
                ImGui::InputInt("Res X", &ctx.resolution.x, 2, 10);
                ImGui::InputInt("Res Y", &ctx.resolution.y, 2, 10);
                if (ctx.resolution.x % 2 != 0) ctx.resolution.x--;
                if (ctx.resolution.y % 2 != 0) ctx.resolution.y--;
                ImGui::SliderInt("Tile Count", &ctx.tiles.count, 4, 32);
                ImGui::InputFloat3("Position", &camera_position[0]);
                ImGui::InputFloat3("Look At", &camera_look_at[0]);
                ImGui::SliderFloat("FOV", &fov, 30, 120);
                const auto update = ImGui::Button("Re-Render");
                ImGui::NewLine();
                static auto file_name = std::string("");
                file_name.reserve(65);
                ImGui::InputTextWithHint(
                  "File Name",
                  "The max file name length is 64!",
                  file_name.data(),
                  64);
                const auto export_render = ImGui::Button("Export / Save");
                ImGui::End();

                if (update)
                {
                    // Stop the thread pool
                    _render_pool.stop();
                    // Update the ray tracing context
                    ctx.tiles.x_size = ctx.resolution.x / ctx.tiles.count;
                    ctx.tiles.y_size = ctx.resolution.y / ctx.tiles.count;
                    ctx.camera       = Camera(
                      camera_position,
                      camera_look_at,
                      fov,
                      ctx.resolution.x / ((float) ctx.resolution.y));
                    ctx.buffer.resize(ctx.resolution.x * ctx.resolution.y * 3, 0);
                    _ray_tracing_context = ctx;
                    _render_pool.start();
                    _render_screen();
                }

                if (export_render)
                {
                    // Ensure that the export directory is created
                    const auto export_directory        = std::filesystem::path("./export");
                    const auto export_directory_exists = std::filesystem::exists(export_directory);

                    if (!export_directory_exists)
                        std::filesystem::create_directory(export_directory);

                    // Create the file name day-month-year hour:minute:second.jpg
                    const auto now            = std::chrono::system_clock::now();
                    const auto time_now       = std::chrono::system_clock::to_time_t(now);
                    const auto local_time_now = *localtime(&time_now);

                    auto export_file_name_string = std::string();
                    if (file_name.empty())
                    {
                        auto export_file_name = std::stringstream();

                        export_file_name << "./export/";
                        export_file_name << local_time_now.tm_hour << ':';      // Hour
                        export_file_name << local_time_now.tm_min << ".jpg";    // Second
                        export_file_name_string = std::string(export_file_name.str());
                    }
                    else
                    {
                        export_file_name_string = file_name;
                    }


                    stbi_flip_vertically_on_write(true);

                    stbi_write_jpg(
                      export_file_name_string.c_str(),
                      _ray_tracing_context.resolution.x,
                      _ray_tracing_context.resolution.y,
                      4,
                      _ray_tracing_context.buffer.data(),
                      100);
                }
            }

            glUseProgram(program);
            glClearColor(0.7, 0.7, 0.7, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            _update_uniforms();
            glTexImage2D(
              GL_TEXTURE_2D,
              0,
              GL_RGBA8,
              _ray_tracing_context.resolution.x,
              _ray_tracing_context.resolution.y,
              0,
              GL_RGBA,
              GL_UNSIGNED_BYTE,
              _ray_tracing_context.buffer.data());
            glGenerateMipmap(GL_TEXTURE_2D);

            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, texture);

            glBindVertexArray(VAO);
            glDrawArrays(GL_TRIANGLES, 0, 3);
            ImGui::Render();
            ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

            glfwSwapBuffers(_window);
        }
        glfwTerminate();
    }

    void Renderer::_handle_mouse(float mouse_x, float mouse_y, ClickType click_type)
    {
        // Get the normalized screen coordinates of the mouse click
        auto normal_x = mouse_x / _screen_resolution.x;
        auto normal_y = mouse_y / _screen_resolution.y;

        // Account for the image scaling / this stuff being dumb :(
        normal_x /= .5f;

        if (0.0f < normal_x && normal_x < 1.0f && 0.0f < normal_y && normal_y < 1.0f)
        {
            normal_y *= -1.f;
            normal_y += 1.f;
            const auto material_ray = _ray_tracing_context.camera.get_ray(normal_x, normal_y);

            // Send a ray into the scene and get the material selected
            const auto record = _intersect_scene(material_ray);

            if (record.hit) _selected_material = record.hit_material;
        }
    }

    void Renderer::_update_uniforms() const
    {
        glUniform1f(
          glGetUniformLocation(_rendering_context.gl_program, "x_scale"),
          _render_target_setting.x_scale);
        glUniform1f(
          glGetUniformLocation(_rendering_context.gl_program, "y_scale"),
          _render_target_setting.y_scale);

        glUniform1ui(
          glGetUniformLocation(_rendering_context.gl_program, "screen_width"),
          _rendering_context.resolution_x);

        glUniform1ui(
          glGetUniformLocation(_rendering_context.gl_program, "screen_height"),
          _rendering_context.resolution_y);
    }

    void Renderer::load_model(const std::string &model, ModelType model_type)
    {
        _loaded_materials.clear();
        _material_indices.clear();
        _vertices.clear();
        _indices.clear();
        if (model_type == ModelType::OBJ)
        {
            tinyobj::attrib_t                attrib;
            std::vector<tinyobj::shape_t>    shapes;
            std::vector<tinyobj::material_t> materials;
            std::string                      warn;
            std::string                      err;
            const auto                       ret = tinyobj::LoadObj(
              &attrib,
              &shapes,
              &materials,
              &warn,
              &err,
              model.c_str(),
              nullptr,
              true);

            if (!warn.empty()) std::cout << warn << std::endl;
            if (!err.empty()) std::cerr << err << std::endl;
            if (!ret) exit(-1);

            if (attrib.vertices.size() % 3 != 0)
            {
                std::cerr << "Bad model lol" << std::endl;
                exit(-1);
            }

            const auto vertex_count = attrib.vertices.size() / 3;
            _vertices.reserve(vertex_count);
            for (auto i = 0; i < attrib.vertices.size(); i += 3)
            {
                _vertices.emplace_back(
                  attrib.vertices[i + 0],
                  attrib.vertices[i + 1],
                  attrib.vertices[i + 2]);
            }

            for (const auto &shape : shapes)
            {
                _indices.reserve(shape.mesh.indices.size());
                for (const auto idx : shape.mesh.indices) _indices.push_back(idx.vertex_index);
                auto material           = Material();
                material.name           = shape.name;
                material.type           = Material::DIFFUSE;
                material.reflectiveness = 1.f;
                material.color          = glm::vec3(1.f, 1.f, 1.f);

                _loaded_materials.push_back(std::move(material));

                _material_indices.reserve(shape.mesh.indices.size() / 3);
                for (auto f = 0; f < shape.mesh.num_face_vertices.size(); f++)
                    _material_indices.push_back(_loaded_materials.size() - 1);
            }
        }

        // Once we're done loading the model into our indices / vertices / Todo: materials
        // We rebuild the scene
        auto *vertices = (float *) rtcSetNewGeometryBuffer(
          _geometry,
          RTC_BUFFER_TYPE_VERTEX,
          0,
          RTC_FORMAT_FLOAT3,
          3 * sizeof(float),
          _vertices.size());

        auto *indices = (unsigned *) rtcSetNewGeometryBuffer(
          _geometry,
          RTC_BUFFER_TYPE_INDEX,
          0,
          RTC_FORMAT_UINT3,
          3 * sizeof(unsigned),
          _indices.size());

        if (vertices != nullptr)
            std::memcpy(vertices, _vertices.data(), sizeof(glm::vec3) * _vertices.size());

        if (indices != nullptr)
            std::memcpy(indices, _indices.data(), sizeof(uint32_t) * _indices.size());

        _selected_material = nullptr;
        rtcCommitGeometry(_geometry);
        rtcAttachGeometry(_scene, _geometry);
        rtcCommitScene(_scene);
    }

    void Renderer::_render_screen(uint64_t spp)
    {
        auto detail = RenderTaskDetail();

        const auto resolution = _ray_tracing_context.resolution;
        const auto tile_size  = _ray_tracing_context.tiles;

        std::vector<std::function<void()>> tasks;
#if 1
        for (int j = 0; j * tile_size.y_size < resolution.y; ++j)
            for (int i = 0; i * tile_size.x_size < resolution.x; ++i)
            {
                detail.x = i;
                detail.y = j;

                tasks.emplace_back([detail, this] { _render_task(detail); });
            }

        _render_pool.add_tasks(tasks);
#else
        detail.x = tile_count / 2;
        detail.y = tile_count / 2;
        for (int i = 0; i < tile_count * tile_count; ++i)
        {
            _render_pool.add_task([detail, this] { _render_task(detail); });
            if ((detail.x) <= (detail.y) && (detail.x != detail.y || detail.x >= 0))
                detail.x += ((detail.y >= 0) ? 1 : -1);
            else
                detail.y += ((detail.x >= 0) ? -1 : 1);
        }
#endif
    }

    HitRecord Renderer::_intersect_scene(const Ray &ray)
    {
        auto ctx = RTCIntersectContext();
        rtcInitIntersectContext(&ctx);
        auto best             = HitRecord();
        auto ray_hit          = RTCRayHit();
        ray_hit.ray.org_x     = ray.origin.x;
        ray_hit.ray.org_y     = ray.origin.y;
        ray_hit.ray.org_z     = ray.origin.z;
        ray_hit.ray.dir_x     = ray.direction.x;
        ray_hit.ray.dir_y     = ray.direction.y;
        ray_hit.ray.dir_z     = ray.direction.z;
        ray_hit.ray.tnear     = 0.f;
        ray_hit.ray.tfar      = std::numeric_limits<float>::infinity();
        ray_hit.ray.mask      = -1;
        ray_hit.hit.geomID    = RTC_INVALID_GEOMETRY_ID;
        ray_hit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;

        rtcIntersect1(_scene, &ctx, &ray_hit);

        if (ray_hit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
        {
            best.hit                = true;
            best.distance           = ray_hit.ray.tfar;
            best.intersection_point = ray.point_at(ray_hit.ray.tfar);
            best.normal =
              glm::normalize(glm::vec3(ray_hit.hit.Ng_x, ray_hit.hit.Ng_y, ray_hit.hit.Ng_z));
            best.hit_material = &(_loaded_materials[_material_indices[ray_hit.hit.primID]]);
        }

        return best;
    }

    Ray Renderer::_process_hit(const HitRecord &record, const Ray &ray, float &out_reflection)
    {
        out_reflection = record.hit_material->reflectiveness;
        if (record.hit_material->type == Material::MIRROR)
        {
            auto new_ray      = ray;
            new_ray.direction = glm::reflect(ray.direction, record.normal);
            new_ray.origin    = record.intersection_point + record.normal * 0.01f;
            return new_ray;
        }
        else if (record.hit_material->type == Material::REFRACTIVE)
        {
            auto       new_ray = ray;
            const auto ior     = record.hit_material->ior;

            const auto refract =
              [](const glm::vec3 &v, const glm::vec3 &n, float nit, glm::vec3 &refracted) {
                  const auto uv   = glm::normalize(v);
                  const auto dt   = glm::dot(uv, n);
                  const auto disc = 1.0f - nit * nit * (1 - dt * dt);
                  if (disc > 0)
                  {
                      refracted = (uv - n * dt) * nit - n * sqrtf(disc);
                      return true;
                  }
                  return false;
              };

            const auto schlick = [](float cosine, float ref_index) {
                const auto r0 = powf((1.0f - ref_index) / (1.0f + ref_index), 2.0f);
                return r0 + (1.0f - r0) * powf((1 - cosine), 5);
            };

            auto outward_normal = glm::vec3();
            auto nit            = 0.0f;
            auto cosine         = 0.0f;
            auto reflected      = glm::reflect(ray.direction, record.normal);

            if (glm::dot(ray.direction, record.normal) > 0.0f)
            {
                outward_normal = record.normal;
                nit            = ior;
                cosine         = ior * glm::dot(ray.direction, record.normal);
            }
            else
            {
                outward_normal = record.normal * -1.f;
                nit            = 1.0f / ior;
                cosine         = -glm::dot(ray.direction, record.normal);
            }

            auto refracted      = glm::vec3();
            auto reflect_chance = 1.0f;

            if (refract(ray.direction, outward_normal, nit, refracted))
                reflect_chance = schlick(cosine, ior);

            if (rand_float() < reflect_chance)
            {
                new_ray.origin    = record.intersection_point + outward_normal * 0.1f;
                new_ray.direction = reflected;
            }
            else
            {
                new_ray.origin    = record.intersection_point + outward_normal * -0.1f;
                new_ray.direction = refracted;
            }

            return new_ray;
        }
        else if (record.hit_material->type == Material::DIFFUSE)
        {
            auto       new_ray = ray;
            const auto x       = rand_float();
            const auto y       = rand_float();
            auto       sample  = cos_sample_hemisphere(x, y);
            if (glm::dot(sample, record.normal) < 0.0f) sample *= -1.f;
            new_ray.direction = glm::normalize(record.normal + sample);
            new_ray.origin    = record.intersection_point + record.normal * 0.01f;
            out_reflection    = fmaxf(0.f, glm::dot(record.normal, new_ray.direction));
            return new_ray;
        }
        else if (record.hit_material->type == Material::METAL)
        {
            auto new_ray   = ray;
            new_ray.origin = record.intersection_point + record.normal * 0.01f;

            const auto reflected = glm::normalize(glm::reflect(ray.direction, record.normal));
            new_ray.direction    = reflected;
            if (record.hit_material->roughness > 0.0f)
            {
                const auto x      = rand_float();
                const auto y      = rand_float();
                auto       sample = cos_sample_hemisphere(x, y);
                if (glm::dot(sample, record.normal) < 0.0f) sample *= -1.f;
                new_ray.direction =
                  glm::normalize(reflected + record.hit_material->roughness * sample);
            }

            return new_ray;
        }
        else
        {
            auto new_ray      = ray;
            new_ray.direction = glm::reflect(ray.direction, record.normal);
            new_ray.origin    = record.intersection_point + record.normal * 0.01f;
            out_reflection    = 1.f;
            return new_ray;
        }
    }

    void Renderer::_render_task(RenderTaskDetail detail)
    {
        const auto x_max = detail.x - 1 == _ray_tracing_context.tiles.count
          ? _ray_tracing_context.resolution.x
          : std::min(
              detail.x * _ray_tracing_context.tiles.x_size + _ray_tracing_context.tiles.x_size,
              (int) _ray_tracing_context.resolution.x);
        const auto y_max = detail.y - 1 == _ray_tracing_context.tiles.count
          ? _ray_tracing_context.resolution.y
          : std::min(
              detail.y * _ray_tracing_context.tiles.y_size + _ray_tracing_context.tiles.y_size,
              (int) _ray_tracing_context.resolution.y);

        for (uint64_t x = detail.x * _ray_tracing_context.tiles.x_size; x < x_max; x++)
        {
            for (uint64_t y = detail.y * _ray_tracing_context.tiles.y_size; y < y_max; y++)
            {
                auto final_spp = glm::vec3(0, 0, 0);
                for (auto spp = 0; spp < _ray_tracing_context.spp; spp++)
                {
                    auto ray = _ray_tracing_context.camera.get_ray(
                      ((float) x + rand_float()) / _ray_tracing_context.resolution.x,
                      ((float) y + rand_float()) / _ray_tracing_context.resolution.y);
                    auto throughput = glm::vec3(1, 1, 1);
                    auto final      = glm::vec3(0, 0, 0);

                    for (auto bounce = 0; bounce < _ray_tracing_context.bounces; bounce++)
                    {
                        auto current = _intersect_scene(ray);

                        if (!current.hit)
                        {
                            // We didn't intersect anything, so lets get the skybox colour and dip
                            // out of here.
                            auto skybox_uv = glm::vec2(
                              0.5f + atan2f(ray.direction.z, ray.direction.x) / (2 * 3.1415),
                              0.5f - asinf(ray.direction.y) / 3.1415);

                            if (_envmap.has_value())
                            {
                                const uint64_t u     = skybox_uv.x * _envmap->width;
                                const uint64_t v     = skybox_uv.y * _envmap->height;
                                const auto     index = u + v * _envmap->width;
                                const auto     out   = glm::vec3(
                                  _envmap->data[index * 3 + 0] / 255.f,
                                  _envmap->data[index * 3 + 1] / 255.f,
                                  _envmap->data[index * 3 + 2] / 255.f);
                                final += throughput * out;
                            }
                            else
                            {
                                const auto blue  = glm::vec3(0.4, 0.4, 1.0);
                                const auto white = glm::vec3(1, 1, 1);
                                const auto out   = glm::mix(white, blue, skybox_uv.y);
                                final += throughput * out;
                            }
                            break;
                        }
                        else
                        {
                            auto reflection = -1.f;
                            ray             = _process_hit(current, ray, reflection);
                            if (_loaded_materials.empty())
                            {
                                final += throughput * 0.3f;
                                throughput *= glm::vec3(1, 1, 1) * .2f;
                            }
                            else
                            {
                                final += throughput * current.hit_material->emission *
                                  current.hit_material->color;
                                throughput *= current.hit_material->color * reflection;
                            }
                        }
                    }
                    final_spp += final;
                }

                final_spp /= _ray_tracing_context.spp;
                const auto index = (x + y * _ray_tracing_context.resolution.x);

                _ray_tracing_context.buffer[index] |= static_cast<uint8_t>(final_spp[0] * 255.f);
                _ray_tracing_context.buffer[index] |= static_cast<uint8_t>(final_spp[1] * 255.f)
                  << 8;
                _ray_tracing_context.buffer[index] |= static_cast<uint8_t>(final_spp[2] * 255.f)
                  << 16;
                _ray_tracing_context.buffer[index] |= static_cast<uint8_t>(~0) << 24;
            }
        }
    }
}    // namespace PT2
     /*
     namespace pt2
     {
         std::vector<std::pair<std::string, std::string>> models;
         std::vector<pt2::Image>                          loaded_images;
         uint32_t                                         skybox = pt2::INVALID_HANDLE;
         HitRecord           intersect_scene(const Ray &ray, RTCScene scene);
         void                process_hit_material(Ray &ray, HitRecord &record);
         [[nodiscard]] float rand()
         {
             thread_local static std::mt19937                          gen;
             thread_local static std::uniform_real_distribution<float> dist(0.f, 1.f);
             return dist(gen);
         }
     
         uint32_t load_image(std::string_view image_name)
         {
             int   width, height, components;
             auto *data = stbi_load(image_name.data(), &width, &height, &components, 3);
             loaded_images.emplace_back(width, height, data);
             stbi_image_free(data);
             return loaded_images.size() - 1;
         }
     
         void load_model(std::string path, std::string name)
         {
             models.emplace_back(std::move(path), std::move(name));
         }
     
         void render_scene(SceneRenderDetail detail, uint32_t *buffer)
         {
             // Start of splitting the scene into different areas and work loads for the threads to
     use std::mutex                                work_access_mutex; std::queue<std::pair<uint16_t,
     uint16_t>> work_queue;
     
             const auto next_pixels = [&](std::pair<uint16_t, uint16_t> &vec) {
                 if (!work_queue.empty())
                 {
                     work_access_mutex.lock();
                     std::swap(vec, work_queue.front());
                     work_queue.pop();
                     work_access_mutex.unlock();
                 }
                 return !work_queue.empty();
             };
     
             const auto x_tiles = detail.width / 16;
             const auto y_tiles = detail.height / 16;
     
             bool     left     = false;
             bool     down     = false;
             uint16_t distance = 1;
             //        for (auto i = 0; i < 16; i++)
             //            for (auto j = 0; j < 16; j++) work_queue.emplace(j, i);
             //        work_queue.emplace(15, 15);
     
             // Todo: Fix this, bcuz its really cool and worth it
     
             std::pair<uint16_t, uint16_t> coord;
             coord.first  = 7;
             coord.second = 7;
             work_queue.emplace(coord.first);
             while (work_queue.size() < 16 * 16)
             {
                 for (int i = 0; i < distance; i++)
                 {
                     coord.first += down ? -1 : 1;
                     work_queue.emplace(coord);
                 }
                 down = !down;
     
                 for (int i = 0; i < distance; i++)
                 {
                     coord.second += left ? -1 : 1;
                     work_queue.emplace(coord);
                 }
                 left = !left;
                 distance++;
             }
     
             const auto scene_camera = Camera(
               detail.camera.origin,
               detail.camera.lookat,
               60,
               (float) detail.width / (float) detail.height);
     
             const auto local_skybox = &loaded_images[skybox];
     
             std::vector<Vec3>         emissions;
             std::vector<Vec3>         vertices;
             std::vector<Vec3>         normals;
             std::vector<Vec3>         colours;
             std::vector<MaterialData> material_data;
             std::vector<uint32_t>     indices;
     
             for (const auto &model_path : models)
             {
                 tinyobj::attrib_t                attrib;
                 std::vector<tinyobj::shape_t>    shapes;
                 std::vector<tinyobj::material_t> materials;
                 std::string                      warn;
                 std::string                      err;
                 const auto                       model_full_path = model_path.first +
     model_path.second; const auto                       ret             = tinyobj::LoadObj( &attrib,
                   &shapes,
                   &materials,
                   &warn,
                   &err,
                   model_full_path.c_str(),
                   model_path.first.c_str(),
                   true);
     
                 if (!warn.empty()) std::cout << warn << std::endl;
                 if (!err.empty()) std::cerr << err << std::endl;
                 if (!ret) exit(-1);
     
                 const auto vertex_count = attrib.vertices.size() / 3;
                 colours.reserve(colours.size() + vertex_count);
                 indices.reserve(indices.size() + vertex_count);
                 vertices.reserve(vertices.size() + vertex_count);
                 emissions.reserve(emissions.size() + vertex_count);
                 material_data.resize(material_data.size() + vertex_count);
                 for (auto i = 0; i < attrib.vertices.size(); i += 3)
                 {
                     colours.emplace_back(.3, .3, .3);
                     emissions.emplace_back(0, 0, 0);
                     vertices.emplace_back(
                       attrib.vertices[i + 0],
                       attrib.vertices[i + 1],
                       attrib.vertices[i + 2]);
                 }
     
                 std::unordered_map<std::string, std::unique_ptr<Image>> loaded_textures;
                 for (const auto &material : materials)
                 {
                     const auto diffuse_name = material.diffuse_texname;
                     if (!diffuse_name.empty())
                     {
                         int   width, height, components;
                         auto *data = stbi_load(
                           (model_path.first + diffuse_name).data(),
                           &width,
                           &height,
                           &components,
                           3);
                         loaded_textures.insert_or_assign(
                           diffuse_name,
                           std::move(std::make_unique<Image>(width, height, data)));
                         stbi_image_free(data);
                     }
                 }
     
                 for (const auto &shape : shapes)
                 {
                     auto index_offset = size_t(0);
                     for (auto f = 0; f < shape.mesh.num_face_vertices.size(); f++)
                     {
                         const auto &vertex     = shape.mesh.num_face_vertices[f];
                         const auto  mat_exists = materials.size() > shape.mesh.material_ids[f];
                         const auto &mat        = materials[shape.mesh.material_ids[f]];
                         const auto &tex        = mat.diffuse_texname;
                         for (auto v = 0; v < vertex; v++)
                         {
                             const auto idx = shape.mesh.indices[index_offset + v];
                             indices.push_back(idx.vertex_index);
     
                             //                        material_data.diffuseness[idx.vertex_index] =
                             //                        .0f;
                             //                        material_data.reflectiveness[idx.vertex_index]
     =
                             //                        .0f;
     material_data.glassiness[idx.vertex_index]
                             //                        = 1.f;
     
                             if (mat_exists)
                             {
                                 emissions[idx.vertex_index] =
                                   Vec3(mat.emission[0], mat.emission[1], mat.emission[2]);
     
                                 if (mat.name == "Material.002")
                                 {
                                     material_data[idx.vertex_index].diffuseness = 0;
                                     material_data[idx.vertex_index].metalness   = 1;
                                     colours[idx.vertex_index]                   = Vec3(0.7, 0.7,
     0.7);
                                 }
     
                                 if (mat.name == "Material.003")
                                 {
                                     material_data[idx.vertex_index].diffuseness = 0;
                                     material_data[idx.vertex_index].glassiness  = 1;
                                     colours[idx.vertex_index]                   = Vec3(0.2, 0.2,
     0.8);
                                 }
     
                                 if (mat.name == "windows")
                                 {
                                     material_data[idx.vertex_index].diffuseness = 0.f;
                                     material_data[idx.vertex_index].glassiness  = 1.f;
                                     colours[idx.vertex_index]                   = Vec3(0.2, 0.2,
     0.2);
                                 }
     
                                 if (mat.name == "Material")
                                 { emissions[idx.vertex_index] = Vec3(0.6, 0.6, 3); }
     
                                 if (!tex.empty())
                                 {
                                     const auto u     = attrib.texcoords[2 * idx.texcoord_index + 0];
                                     const auto v     = attrib.texcoords[2 * idx.texcoord_index + 1];
                                     const auto image = loaded_textures.find(tex)->second->get(u, v);
     
                                     colours[idx.vertex_index] = Vec3(
                                       static_cast<float>(image >> 0 & 0xFF) / 255.f,
                                       static_cast<float>(image >> 8 & 0xFF) / 255.f,
                                       static_cast<float>(image >> 16 & 0xFF) / 255.f);
                                 }
                             }
                         }
                         index_offset += vertex;
                     }
                 }
             }
             std::vector<std::thread> work_threads;
             const auto               thread_count = detail.thread_count == pt2::MAX_THREAD_COUNT
                             ? std::thread::hardware_concurrency() - 1
                             : detail.thread_count;
             work_threads.reserve(thread_count);
     
             const auto device    = rtcNewDevice(nullptr);
             const auto scene     = rtcNewScene(device);
             auto       geom      = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
             auto *     _vertices = (float *) rtcSetNewGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_VERTEX,
               0,
               RTC_FORMAT_FLOAT3,
               3 * sizeof(float),
               vertices.size());
             auto *_indices = (unsigned *) rtcSetNewGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_INDEX,
               0,
               RTC_FORMAT_UINT3,
               3 * sizeof(unsigned),
               indices.size() / 3);
     
             rtcSetGeometryVertexAttributeCount(geom, 7);
     
             rtcSetSharedGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
               0,
               RTC_FORMAT_FLOAT3,
               colours.data(),
               0,
               sizeof(Vec3),
               colours.size());
     
             rtcSetSharedGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
               1,
               RTC_FORMAT_FLOAT3,
               emissions.data(),
               0,
               sizeof(Vec3),
               emissions.size());
     
             rtcSetSharedGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
               2,
               RTC_FORMAT_FLOAT,
               material_data.data(),
               0,
               sizeof(MaterialData),
               material_data.size());
     
             rtcSetSharedGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
               3,
               RTC_FORMAT_FLOAT,
               material_data.data(),
               sizeof(float) * 1,
               sizeof(MaterialData),
               material_data.size());
     
             rtcSetSharedGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
               4,
               RTC_FORMAT_FLOAT,
               material_data.data(),
               sizeof(float) * 2,
               sizeof(MaterialData),
               material_data.size());
     
             rtcSetSharedGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
               5,
               RTC_FORMAT_FLOAT,
               material_data.data(),
               sizeof(float) * 3,
               sizeof(MaterialData),
               material_data.size());
     
             rtcSetSharedGeometryBuffer(
               geom,
               RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
               6,
               RTC_FORMAT_FLOAT,
               material_data.data(),
               sizeof(float) * 4,
               sizeof(MaterialData),
               material_data.size());
     
             if (_vertices != nullptr)
                 ::memcpy(_vertices, vertices.data(), sizeof(Vec3) * vertices.size());
             if (_indices != nullptr)
                 ::memcpy(_indices, indices.data(), sizeof(uint32_t) * indices.size());
     
             rtcCommitGeometry(geom);
             rtcAttachGeometry(scene, geom);
             rtcCommitScene(scene);
     
             const auto start = std::chrono::steady_clock::now();
             for (uint8_t i = 0; i < thread_count; i++)
             {
                 work_threads.emplace_back([&] {
                     std::pair<uint16_t, uint16_t> next_grid;
                     while (true)
                     {
                         if (!next_pixels(next_grid)) break;
                         next_grid.first *= x_tiles;
                         next_grid.second *= y_tiles;
     
                         const auto x_max =
                           next_grid.first == 15 ? detail.width : next_grid.first + x_tiles;
                         const auto y_max =
                           next_grid.second == 15 ? detail.height : next_grid.second + y_tiles;
     
                         for (int x = next_grid.first; x < x_max; x++)
                         {
                             for (int y = next_grid.second; y < y_max; y++)
                             {
                                 auto final_pixel = Vec3();
                                 for (auto spp = 0; spp < detail.spp; spp++)
                                 {
                                     auto ray = scene_camera.get_ray(
                                       ((float) x + rand()) / (float) detail.width,
                                       ((float) y + rand()) / (float) detail.height);
                                     auto throughput = Vec3(1, 1, 1);
                                     auto final      = Vec3(0, 0, 0);
     
                                     for (auto bounce = 0; bounce < detail.max_bounces; bounce++)
                                     {
                                         auto current = pt2::intersect_scene(ray, scene);
     
                                         if (!current.hit)
                                         {
                                             float u = 0.5f +
                                               atan2f(ray.direction.z, ray.direction.x) / (2
     * 3.1415); float v = 0.5f - asinf(ray.direction.y) / 3.1415; v *= -1; v += 1; uint32_t   image =
     skybox == pt2::INVALID_HANDLE ? 0 : local_skybox->get(u, v); const auto at    = Vec3(
                                               static_cast<float>(image >> 0 & 0xFF) / 255.f,
                                               static_cast<float>(image >> 8 & 0xFF) / 255.f,
                                               static_cast<float>(image >> 16 & 0xFF) / 255.f);
                                             final += throughput * at;
                                             break;
                                         }
                                         process_hit_material(ray, current);
                                         final += throughput * current.emission;
                                         throughput *= current.albedo * current.reflectiveness;
                                     }
     
                                     final_pixel += final;
                                 }
     
                                 buffer[x + y * detail.width] |= static_cast<uint8_t>(
                                   fminf(sqrtf(final_pixel.x / (float) detail.spp), 1.f) * 255);
                                 buffer[x + y * detail.width] |=
                                   static_cast<uint8_t>(
                                     fminf(sqrtf(final_pixel.y / (float) detail.spp), 1.f) * 255)
                                   << 8;
                                 buffer[x + y * detail.width] |=
                                   static_cast<uint8_t>(
                                     fminf(sqrtf(final_pixel.z / (float) detail.spp), 1.f) * 255)
                                   << 16;
                                 buffer[x + y * detail.width] |= static_cast<uint8_t>(~0) << 24;
                             }
                         }
                     }
                 });
             }
     
             for (uint8_t i = 0; i < thread_count; i++) work_threads[i].join();
     
             const auto end = std::chrono::steady_clock::now();
             std::cout << "Time difference = "
                       << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                       << "[ms]" << std::endl;
         }
     
     #ifdef PT2_GLFW
     
         void render_and_display_scene(SceneRenderDetail detail)
         {
             glfwInit();
             const auto window = glfwCreateWindow(detail.width, detail.height, "PT2", nullptr,
     nullptr); glfwMakeContextCurrent(window);
     
             std::vector<uint32_t> data;
             data.resize(detail.width * detail.height);
     
             std::thread render_thread([&] { render_scene(detail, data.data()); });
             while (!glfwWindowShouldClose(window))
             {
                 glClear(GL_COLOR_BUFFER_BIT);
                 glDrawPixels(detail.width, detail.height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
                 glfwSwapBuffers(window);
                 glfwPollEvents();
             }
             render_thread.join();
         }
     
     #endif
     
         void process_hit_material(Ray &ray, HitRecord &record)
         {
             ray.origin            = record.intersection_point + record.normal * 0.01f;
             ray.direction         = ray.direction.reflect(record.normal);
             record.reflectiveness = 1.f;
             return;
     
             auto material_branch = rand();
     
             if (material_branch < record.material_data.diffuseness)
             {
                 ray.origin           = record.intersection_point + record.normal * 0.01f;
                 const auto u         = rand();
                 const auto v         = rand();
                 const auto phi       = 2.0f * 3.1415 * u;
                 const auto cos_theta = 2.0f * v - 1.0f;
                 const auto a         = sqrtf(1.0f - cos_theta * cos_theta);
                 ray.direction =
                   record.normal + Vec3(a * cosf(phi), a * sinf(phi), cos_theta).to_unit_vector();
                 record.reflectiveness = fmaxf(0.f, record.normal.dot(ray.direction));
                 return;
             }
             material_branch -= record.material_data.diffuseness;
     
             if (material_branch < record.material_data.metalness)
             {
                 /** Cook Torrence BRDF
                  *
                  * Equation:
                  * f_r(v,l) = D(h,a)G(v,l,a)F(v,h,f0) / 4(n*v)(n*l)
                  *
                  * Where:
                  * v = Inverse Ray Direction
                  * l = Scattered Ray Direction
                  * n = Surface Normal
                  * h = Half Unit Vector Between l and v
                  * a = roughness
                  *
                  */

/** GGX NDF
 *
 * Equation:
 * D_ggx(h,a) = a^2 / pi((n*h)^2(a^2-1) + 1)^2
 *
 */

/*
            const auto a_sq       = record.material_data.roughness *
   record.material_data.roughness; const auto h          =
   record.normal.half_unit_from(ray.direction.inv()); const auto n_dot_h_sq =
   powf(record.normal.dot(h), 2.f);

            const auto bottom_term = M_PI * powf(n_dot_h_sq * (a_sq - 1), 2.f);
            const auto d_ggx       = a_sq / bottom_term;

            /** Geometric Mapping
             *
             * Equation:
             * G_ggx(v,l,a) = G_0(n,l)G_0(n,v)
             *
             * G_0(x,y) = 2(x*y) / x*y + sqrt(a^2 + (1-a^2)(n*l)^2)
             */

/*
            const auto n_dot_l    = record.normal.dot(ray.direction.reflect(record.normal));
            const auto n_dot_l_sq = n_dot_l * n_dot_h_sq;
            const auto g0_n_l     = 2 * n_dot_l / n_dot_l + sqrtf(a_sq + (1 - a_sq) *
   (n_dot_l_sq));

            const auto n_dot_v    = record.normal.dot(ray.direction.inv());
            const auto n_dot_v_sq = n_dot_v * n_dot_v;
            const auto g0_n_v     = 2 * n_dot_v / n_dot_v + sqrtf(a_sq + (1 - a_sq) *
   (n_dot_v_sq));

            const auto g_ggx = g0_n_l * g0_n_v;

            /** Fresnel (specular F)
             *
             * Equation:
             * F_schlick(v,h,f_0) = f0 + (1 - f0)(1 - v*h)^5
             *
             */
/*
            constexpr auto f0        = 1.5f;
            const auto     f         = powf(1.f - h.dot(record.normal), 5.f);
            const auto     f_schlick = f0 + (1 - f0) * f;

            const auto brdf = d_ggx * g_ggx * f_schlick / 4 * n_dot_v * n_dot_l;

            return;
        }
        material_branch -= record.material_data.metalness;

        if (material_branch < record.material_data.glassiness)
        {
            const auto refract = [](Vec3 &v, Vec3 &n, float nit, Vec3 &refracted) {
                Vec3  uv   = v.to_unit_vector();
                float dt   = uv.dot(n);
                float disc = 1.0f - nit * nit * (1 - dt * dt);
                if (disc > 0)
                {
                    refracted = (uv - n * dt) * nit - n * sqrtf(disc);
                    return true;
                }
                return false;
            };

            const auto schlick = [](float cosine, float refIndex) {
                float r0 = (1.0f - refIndex) / (1.0f + refIndex);
                r0       = r0 * r0;
                return r0 + (1.0f - r0) * powf((1 - cosine), 5);
            };

            auto outward_normal = Vec3();
            auto rayOrigin      = ray.direction;
            auto reflected      = rayOrigin - record.normal * record.normal.dot(ray.direction) *
2; auto nit            = 0.f; auto cosine         = 0.f; if (ray.direction.dot(record.normal) >
0)
            {
                outward_normal = record.normal * -1.0f;
                nit            = 1.5f;
                cosine         = 1.5f * ray.direction.dot(record.normal);
            }
            else
            {
                outward_normal = record.normal;
                nit            = 1.0f / 1.5f;
                cosine         = -ray.direction.dot(record.normal);
            }
            auto  refracted = Vec3();
            float reflectChance;
            if (refract(ray.direction, outward_normal, nit, refracted))
            { reflectChance = schlick(cosine, 1.5f); }
            else
            {
                reflectChance = 1;
            }
            if (rand() < reflectChance)
            {
                ray = Ray(record.intersection_point + outward_normal * 0.1f, reflected);
                record.reflectiveness = 1.f;
            }
            else
            {
                ray = Ray(record.intersection_point + outward_normal * -0.1f, refracted);
                record.reflectiveness = 1.f;
            }
            return;
        }
        material_branch -= record.material_data.glassiness;
    }

    HitRecord intersect_scene(const Ray &ray, RTCScene scene)
    {
        struct RTCIntersectContext ctx
        {
        };
        rtcInitIntersectContext(&ctx);
        auto best = HitRecord();

        struct RTCRayHit rayhit
        {
        };
        rayhit.ray.org_x     = ray.origin.x;
        rayhit.ray.org_y     = ray.origin.y;
        rayhit.ray.org_z     = ray.origin.z;
        rayhit.ray.dir_x     = ray.direction.x;
        rayhit.ray.dir_y     = ray.direction.y;
        rayhit.ray.dir_z     = ray.direction.z;
        rayhit.ray.tnear     = 0.f;
        rayhit.ray.tfar      = std::numeric_limits<float>::infinity();
        rayhit.ray.mask      = -1;
        rayhit.ray.flags     = 0;
        rayhit.hit.geomID    = RTC_INVALID_GEOMETRY_ID;
        rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
        rtcIntersect1(scene, &ctx, &rayhit);

        if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID)
        {
            const auto geo = rtcGetGeometry(scene, rayhit.hit.geomID);
            rtcInterpolate0(
              geo,
              rayhit.hit.primID,
              rayhit.hit.u,
              rayhit.hit.v,
              RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
              0,
              reinterpret_cast<float *>(&best.albedo),
              3);

            rtcInterpolate0(
              geo,
              rayhit.hit.primID,
              rayhit.hit.u,
              rayhit.hit.v,
              RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
              1,
              reinterpret_cast<float *>(&best.emission),
              3);

            rtcInterpolate0(
              geo,
              rayhit.hit.primID,
              rayhit.hit.u,
              rayhit.hit.v,
              RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
              2,
              reinterpret_cast<float *>(&best.material_data.diffuseness),
              1);

            rtcInterpolate0(
              geo,
              rayhit.hit.primID,
              rayhit.hit.u,
              rayhit.hit.v,
              RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
              3,
              reinterpret_cast<float *>(&best.material_data.glassiness),
              1);

            rtcInterpolate0(
              geo,
              rayhit.hit.primID,
              rayhit.hit.u,
              rayhit.hit.v,
              RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
              4,
              reinterpret_cast<float *>(&best.material_data.metalness),
              1);

            rtcInterpolate0(
              geo,
              rayhit.hit.primID,
              rayhit.hit.u,
              rayhit.hit.v,
              RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
              5,
              reinterpret_cast<float *>(&best.material_data.roughness),
              1);

            rtcInterpolate0(
              geo,
              rayhit.hit.primID,
              rayhit.hit.u,
              rayhit.hit.v,
              RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE,
              6,
              reinterpret_cast<float *>(&best.material_data.reflectiveness),
              1);

            best.intersection_point = ray.point_at(rayhit.ray.tfar);
            best.distance           = rayhit.ray.tfar;
            best.hit                = true;
            best.normal = Vec3(rayhit.hit.Ng_x, rayhit.hit.Ng_y,
rayhit.hit.Ng_z).to_unit_vector();
        }

        return best;
    }

    void set_skybox(uint32_t image_handle) { skybox = image_handle; }

    Image::Image(int width, int height, const uint8_t *data) : _width(width), _height(height)
    {
        _data.resize(_width * _height);
        for (size_t i = 0; i < _width * _height; i++)
        {
            _data[i] |= data[i * 3 + 0] << 0;
            _data[i] |= data[i * 3 + 1] << 8;
            _data[i] |= data[i * 3 + 2] << 16;
            _data[i] |= 0 << 24;
        }
    }

    auto Image::width() const noexcept -> uint32_t { return _width; }

    auto Image::height() const noexcept -> uint32_t { return _height; }

    auto Image::get(float u, float v) const noexcept -> uint32_t
    {
        return get(
          static_cast<uint32_t>(u * _width),
          static_cast<uint32_t>((v * -1 + 1) * _height));
    }

    auto Image::get(uint32_t x, uint32_t y) const noexcept -> uint32_t
    {
        return _data[x % _width + y % _height * _width];
    }
}    // namespace pt2
*/