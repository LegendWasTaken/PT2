#include <pt2/pt2.h>

#include <random>
#include <thread>
#include <mutex>
#include <memory>
#include <queue>
#include <chrono>
#include <iostream>
#include <unordered_map>

#define STB_IMAGE_IMPLEMENTATION

#include <stb/stb_image.h>

#define TINYOBJLOADER_IMPLEMENTATION

#include <tinyobj/tinyobjloader.h>

#include <embree3/rtcore.h>

#include <imgui.h>
#include <imgui/imgui_impl_glfw.h>
#include <imgui/imgui_impl_opengl3.h>

namespace PT2
{
    void Renderer::start_gui() { _handle_window(); }

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

        // Update the rendering context
        _rendering_context.gl_program         = program;
        _rendering_context.gl_fragment_shader = fragment_shader;
        _rendering_context.gl_vertex_shader   = vertex_shader;
        _rendering_context.resolution_x       = mode->width;
        _rendering_context.resolution_y       = mode->height;

        // Setup the texture for displaying the result of the ray tracing buffer
        GLuint texture = 0;

        glGenTextures(1, &texture);
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, texture);
        int width, height, components;
        stbi_set_flip_vertically_on_load(true);
        auto *data =
          stbi_load("./assets/images/background_test.jpeg", &width, &height, &components, 3);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        if (!data) std::cerr << "Error loading the image" << std::endl;
        stbi_image_free(data);

        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO &io = ImGui::GetIO();
        (void) io;

        ImGui::StyleColorsDark();

        ImGui_ImplGlfw_InitForOpenGL(_window, true);
        ImGui_ImplOpenGL3_Init("#version 150");

        while (!glfwWindowShouldClose(_window))
        {
            glfwPollEvents();
            ImGui_ImplOpenGL3_NewFrame();
            ImGui_ImplGlfw_NewFrame();
            ImGui::NewFrame();

            ImGui::Begin("Display Options");
            ImGui::Text("Texture Display Offset");
            ImGui::SliderFloat("Pos X", &_render_target_setting.x_offset, 0.0f, 1.0f);
            ImGui::SliderFloat("Pos Y", &_render_target_setting.y_offset, 0.0f, 1.0f);
            ImGui::SliderFloat("Scale X", &_render_target_setting.x_scale, 0.0f, 1.0f);
            ImGui::SliderFloat("Scale Y", &_render_target_setting.y_scale, 0.0f, 1.0f);
            ImGui::End();

            glUseProgram(program);
            glClearColor(0.7, 0.7, 0.7, 1.0f);
            glClear(GL_COLOR_BUFFER_BIT);

            _update_uniforms();

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

    void Renderer::_update_uniforms() const
    {
        // Update the texture offset uniforms
        glUniform1f(
          glGetUniformLocation(_rendering_context.gl_program, "x_offset"),
          -_render_target_setting.x_offset);    // 1 - x is added to make it feel more natural
        glUniform1f(
          glGetUniformLocation(_rendering_context.gl_program, "y_offset"),
          -_render_target_setting.y_offset);
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

    void Renderer::submit_detail(const RenderDetail &detail) { }

    void Renderer::load_model(const std::string &model, ModelType model_type) { }
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
        // Start of splitting the scene into different areas and work loads for the threads to use
        std::mutex                                work_access_mutex;
        std::queue<std::pair<uint16_t, uint16_t>> work_queue;

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
            const auto                       model_full_path = model_path.first + model_path.second;
            const auto                       ret             = tinyobj::LoadObj(
              &attrib,
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

                        //                        material_data.diffuseness[idx.vertex_index]    =
                        //                        .0f;
                        //                        material_data.reflectiveness[idx.vertex_index] =
                        //                        .0f; material_data.glassiness[idx.vertex_index]
                        //                        = 1.f;

                        if (mat_exists)
                        {
                            emissions[idx.vertex_index] =
                              Vec3(mat.emission[0], mat.emission[1], mat.emission[2]);

                            if (mat.name == "Material.002")
                            {
                                material_data[idx.vertex_index].diffuseness = 0;
                                material_data[idx.vertex_index].metalness   = 1;
                                colours[idx.vertex_index]                   = Vec3(0.7, 0.7, 0.7);
                            }

                            if (mat.name == "Material.003")
                            {
                                material_data[idx.vertex_index].diffuseness = 0;
                                material_data[idx.vertex_index].glassiness  = 1;
                                colours[idx.vertex_index]                   = Vec3(0.2, 0.2, 0.8);
                            }

                            if (mat.name == "windows")
                            {
                                material_data[idx.vertex_index].diffuseness = 0.f;
                                material_data[idx.vertex_index].glassiness  = 1.f;
                                colours[idx.vertex_index]                   = Vec3(0.2, 0.2, 0.2);
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
                                          atan2f(ray.direction.z, ray.direction.x) / (2 * 3.1415);
                                        float v = 0.5f - asinf(ray.direction.y) / 3.1415;
                                        v *= -1;
                                        v += 1;
                                        uint32_t   image = skybox == pt2::INVALID_HANDLE
                                            ? 0
                                            : local_skybox->get(u, v);
                                        const auto at    = Vec3(
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
        const auto window = glfwCreateWindow(detail.width, detail.height, "PT2", nullptr, nullptr);
        glfwMakeContextCurrent(window);

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
            const auto a_sq       = record.material_data.roughness * record.material_data.roughness;
            const auto h          = record.normal.half_unit_from(ray.direction.inv());
            const auto n_dot_h_sq = powf(record.normal.dot(h), 2.f);

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
            const auto g0_n_l     = 2 * n_dot_l / n_dot_l + sqrtf(a_sq + (1 - a_sq) * (n_dot_l_sq));

            const auto n_dot_v    = record.normal.dot(ray.direction.inv());
            const auto n_dot_v_sq = n_dot_v * n_dot_v;
            const auto g0_n_v     = 2 * n_dot_v / n_dot_v + sqrtf(a_sq + (1 - a_sq) * (n_dot_v_sq));

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
            auto reflected      = rayOrigin - record.normal * record.normal.dot(ray.direction) * 2;
            auto nit            = 0.f;
            auto cosine         = 0.f;
            if (ray.direction.dot(record.normal) > 0)
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
            best.normal = Vec3(rayhit.hit.Ng_x, rayhit.hit.Ng_y, rayhit.hit.Ng_z).to_unit_vector();
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