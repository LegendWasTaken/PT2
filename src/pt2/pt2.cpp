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

#include <pt2/camera.h>

#include <embree3/rtcore.h>

namespace pt2 {
    std::vector<std::pair<std::string, std::string>> models;
    std::unique_ptr<BvhNode> top_node;
    std::vector<pt2::Image> loaded_images;
    std::mt19937 gen;
    std::uniform_real_distribution<float> dist(0.f, 1.f);
    uint32_t skybox = pt2::INVALID_HANDLE;


    HitRecord intersect_scene(const Ray &ray, RTCScene scene);

    float rand() {
        return dist(gen);
    }

    uint32_t load_image(std::string_view image_name) {
        int width, height, components;
        auto *data = stbi_load(image_name.data(), &width, &height, &components, 3);
        loaded_images.emplace_back(width, height, data);
        stbi_image_free(data);
        return loaded_images.size() - 1;
    }

    void load_model(std::string path, std::string name) {
        models.emplace_back(std::move(path), std::move(name));
    }

    void render_scene(SceneRenderDetail detail, uint32_t *buffer) {

        // Start of splitting the scene into different areas and work loads for the threads to use
        std::mutex work_access_mutex;
        std::queue<std::pair<uint16_t, uint16_t>> work_queue;

        const auto next_pixels = [&](std::pair<uint16_t, uint16_t> &vec) {
            if (!work_queue.empty()) {
                work_access_mutex.lock();
                std::swap(vec, work_queue.front());
                work_queue.pop();
                work_access_mutex.unlock();
            }
            return !work_queue.empty();
        };

        const auto x_tiles = detail.width / 16;
        const auto y_tiles = detail.height / 16;

        bool left = false;
        bool down = false;
        uint16_t distance = 1;
        for (auto i = 0; i < 16; i++)
            for (auto j = 0; j < 16; j++)
                work_queue.emplace(j, i);
        work_queue.emplace(15, 15);

//        while (work_queue.size() < 16 * 16) {
//            for (int i = 0; i < distance; i++) {
//                coord.first += down ? -1 : 1;
//                work_queue.emplace(coord);
//            }
//            down = !down;
//
//            for (int i = 0; i < distance; i++) {
//                coord.second += left ? -1 : 1;
//                work_queue.emplace(coord);
//            }
//            left = !left;
//            distance++;
//        }
//
        const auto scene_camera = Camera(detail.camera.origin, detail.camera.lookat, 90,
                                         (float) detail.width / (float) detail.height);

        const auto local_skybox = &loaded_images[skybox];

        std::vector<Vec3> emissions;
        std::vector<Vec3> vertices;
        std::vector<Vec3> normals;
        std::vector<Vec3> colours;
        std::vector<uint32_t> indices;


        for (const auto &model_path : models) {
            tinyobj::attrib_t attrib;
            std::vector<tinyobj::shape_t> shapes;
            std::vector<tinyobj::material_t> materials;
            std::string warn;
            std::string err;
            const auto model_full_path = model_path.first + model_path.second;
            const auto ret = tinyobj::LoadObj(&attrib, &shapes, &materials, &warn, &err,
                                              model_full_path.c_str(), model_path.first.c_str(), true);

            if (!warn.empty()) std::cout << warn << std::endl;
            if (!err.empty()) std::cerr << err << std::endl;
            if (!ret) exit(-1);

            colours.reserve(colours.size() + attrib.vertices.size() / 3);
            indices.reserve(indices.size() + attrib.vertices.size() / 3);
            vertices.reserve(vertices.size() + attrib.vertices.size() / 3);
            emissions.reserve(emissions.size() + attrib.vertices.size() / 3);
            for (auto i = 0; i < attrib.vertices.size(); i += 3) {
                colours.emplace_back(1, 1, 1);
                emissions.emplace_back(1, 1, 1);
                vertices.emplace_back(
                        attrib.vertices[i + 0],
                        attrib.vertices[i + 1],
                        attrib.vertices[i + 2]
                );
            }

            std::unordered_map<std::string, std::unique_ptr<Image>> loaded_textures;
            for (const auto &material : materials) {
                const auto diffuse_name = material.diffuse_texname;
                if (!diffuse_name.empty()) {
                    int width, height, components;
                    auto *data = stbi_load((model_path.first + diffuse_name).data(), &width, &height, &components, 3);
                    loaded_textures.insert_or_assign(diffuse_name,
                                                     std::move(std::make_unique<Image>(width, height, data)));
                    stbi_image_free(data);
                }
            }

            for (const auto &shape : shapes) {
                auto index_offset = size_t(0);
                for (auto f = 0; f < shape.mesh.num_face_vertices.size(); f++) {
                    const auto &vertex = shape.mesh.num_face_vertices[f];
                    const auto mat_exists = materials.size() > shape.mesh.material_ids[f];
                    const auto &mat = materials[shape.mesh.material_ids[f]];
                    const auto &tex = mat.diffuse_texname;
                    for (auto v = 0; v < vertex; v++) {
                        const auto idx = shape.mesh.indices[index_offset + v];
                        indices.push_back(idx.vertex_index);

                        if (mat_exists) {
                            emissions[idx.texcoord_index] = Vec3(
                                    mat.emission[0],
                                    mat.emission[1],
                                    mat.emission[2]
                            );

                            if (!tex.empty()) {
                                const auto u = attrib.texcoords[2 * idx.texcoord_index + 0];
                                const auto v = attrib.texcoords[2 * idx.texcoord_index + 1];
                                const auto image = loaded_textures.find(tex)->second->get(u, v);

                                colours[idx.vertex_index] = Vec3(
                                        static_cast<float>(image >> 0 & 0xFF) / 255.f,
                                        static_cast<float>(image >> 8 & 0xFF) / 255.f,
                                        static_cast<float>(image >> 16 & 0xFF) / 255.f);
                            }
                        } else {
                            emissions[idx.texcoord_index] = Vec3(0, 0, 0);
                            colours[idx.vertex_index] = Vec3(1, 1, 1);
                        }


                    }
                    index_offset += vertex;
                }
            }
            std::cout << "done" << std::endl;
        }
        std::vector<std::thread> work_threads;
        const auto thread_count =
                detail.thread_count == pt2::MAX_THREAD_COUNT ? std::thread::hardware_concurrency() * 2 - 1
                                                             : detail.thread_count;
        work_threads.reserve(thread_count);

        const auto device = rtcNewDevice(nullptr);
        const auto scene = rtcNewScene(device);
        auto geom = rtcNewGeometry(device, RTC_GEOMETRY_TYPE_TRIANGLE);
        auto *_vertices = (float *) rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX, 0, RTC_FORMAT_FLOAT3,
                                                            3 * sizeof(float), vertices.size());
        auto *_indices = (unsigned *) rtcSetNewGeometryBuffer(geom, RTC_BUFFER_TYPE_INDEX, 0, RTC_FORMAT_UINT3,
                                                              3 * sizeof(unsigned), indices.size());

        rtcSetGeometryVertexAttributeCount(geom, 2);
        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0, RTC_FORMAT_FLOAT3, colours.data(), 0,
                                   sizeof(Vec3), colours.size());

        rtcSetSharedGeometryBuffer(geom, RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 1, RTC_FORMAT_FLOAT3, emissions.data(), 0,
                                   sizeof(Vec3), emissions.size());

        if (_vertices != nullptr) ::memcpy(_vertices, vertices.data(), sizeof(Vec3) * vertices.size());
        if (_indices != nullptr) ::memcpy(_indices, indices.data(), sizeof(uint32_t) * indices.size());

        rtcCommitGeometry(geom);
        rtcAttachGeometry(scene, geom);
        rtcCommitScene(scene);

        const auto start = std::chrono::steady_clock::now();
        for (uint8_t i = 0; i < thread_count; i++) {
            work_threads.emplace_back([&] {
                std::pair<uint16_t, uint16_t> next_grid;
                while (true) {
                    if (!next_pixels(next_grid)) break;
                    next_grid.first *= x_tiles;
                    next_grid.second *= y_tiles;

                    const auto x_max = next_grid.first == 15 ? detail.width : next_grid.first + x_tiles;
                    const auto y_max = next_grid.second == 15 ? detail.height : next_grid.second + y_tiles;

                    for (int x = next_grid.first; x < x_max; x++) {
                        for (int y = next_grid.second; y < y_max; y++) {
                            auto final_pixel = Vec3();
                            for (auto spp = 0; spp < detail.spp; spp++) {
                                auto ray = scene_camera.get_ray(((float) x + rand()) / (float) detail.width,
                                                                ((float) y + rand()) / (float) detail.height);
                                auto throughput = Vec3(1, 1, 1);
                                auto final = Vec3(0, 0, 0);

                                for (int bounce = 0; bounce < detail.max_bounces; bounce++) {
                                    const auto current = pt2::intersect_scene(ray, scene);

                                    if (!current.hit) {
                                        float u = 0.5f + atan2f(ray.direction.z, ray.direction.x) / (2 * 3.1415);
                                        float v = 0.5f - asinf(ray.direction.y) / 3.1415;
                                        uint32_t image = skybox == pt2::INVALID_HANDLE ? ~0 : local_skybox->get(u, v);
                                        const auto at = Vec3(
                                                static_cast<float>(image >> 0 & 0xFF) / 255.f,
                                                static_cast<float>(image >> 8 & 0xFF) / 255.f,
                                                static_cast<float>(image >> 16 & 0xFF) / 255.f);
//                                        const auto at = Vec3(1, 1, 1);
                                        final += throughput * at;
                                        break;
                                    }

                                    final += throughput * current.emission;
                                    throughput *= current.albedo * current.reflectiveness;

                                    ray.origin = current.intersection_point + current.normal * 0.01f;
                                    const auto u = rand();
                                    const auto v = rand();
                                    const auto phi = 2.0f * 3.1415 * u;
                                    const auto cos_theta = 2.0f * v - 1.0f;
                                    const auto a = sqrtf(1.0f - cos_theta * cos_theta);
                                    ray.direction =
                                            current.normal +
                                            Vec3(a * cosf(phi), a * sinf(phi), cos_theta).to_unit_vector();
                                }

                                final_pixel += final;
                            }

                            buffer[x + y * detail.width] |=
                                    static_cast<uint8_t>((final_pixel.x / (float) detail.spp) * 255);
                            buffer[x + y * detail.width] |=
                                    static_cast<uint8_t>((final_pixel.y / (float) detail.spp) * 255) << 8;
                            buffer[x + y * detail.width] |=
                                    static_cast<uint8_t>((final_pixel.z / (float) detail.spp) * 255) << 16;
                            buffer[x + y * detail.width] |= static_cast<uint8_t>(~0) << 24;
                        }
                    }
                }
            });
        }

        for (uint8_t i = 0; i < thread_count; i++)
            work_threads[i].join();

        const auto end = std::chrono::steady_clock::now();
        std::cout << "Time difference = " << std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count()
                  << "[ms]" << std::endl;
    }

#ifdef PT2_GLFW

    void render_and_display_scene(SceneRenderDetail detail) {
        glfwInit();
        const auto window = glfwCreateWindow(detail.width, detail.height, "PT2", nullptr, nullptr);
        glfwMakeContextCurrent(window);

        std::vector<uint32_t> data;
        data.resize(detail.width * detail.height);


        std::thread render_thread([&] {
            render_scene(detail, data.data());
        });
        while (!glfwWindowShouldClose(window)) {
            glClear(GL_COLOR_BUFFER_BIT);
            glDrawPixels(detail.width, detail.height, GL_RGBA, GL_UNSIGNED_BYTE, data.data());
            glfwSwapBuffers(window);
            glfwPollEvents();
        }
        render_thread.join();
    }

#endif

    HitRecord intersect_scene(const Ray &ray, RTCScene scene) {
        struct RTCIntersectContext ctx{};
        rtcInitIntersectContext(&ctx);
        auto best = HitRecord();

        struct RTCRayHit rayhit{};
        rayhit.ray.org_x = ray.origin.x;
        rayhit.ray.org_y = ray.origin.y;
        rayhit.ray.org_z = ray.origin.z;
        rayhit.ray.dir_x = ray.direction.x;
        rayhit.ray.dir_y = ray.direction.y;
        rayhit.ray.dir_z = ray.direction.z;
        rayhit.ray.tnear = 0.f;
        rayhit.ray.tfar = std::numeric_limits<float>::infinity();
        rayhit.ray.mask = -1;
        rayhit.ray.flags = 0;
        rayhit.hit.geomID = RTC_INVALID_GEOMETRY_ID;
        rayhit.hit.instID[0] = RTC_INVALID_GEOMETRY_ID;
        rtcIntersect1(scene, &ctx, &rayhit);

        if (rayhit.hit.geomID != RTC_INVALID_GEOMETRY_ID) {
            rtcInterpolate0(rtcGetGeometry(scene, rayhit.hit.geomID), rayhit.hit.primID, rayhit.hit.u, rayhit.hit.v,
                            RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 0,
                            reinterpret_cast<float *>(&best.albedo), 3);

//            rtcInterpolate0(rtcGetGeometry(scene, rayhit.hit.geomID), rayhit.hit.primID, rayhit.hit.u, rayhit.hit.v,
//                            RTC_BUFFER_TYPE_VERTEX_ATTRIBUTE, 1,
//                            reinterpret_cast<float *>(&best.emission), 3);
            best.albedo = Vec3(1, 1, 1);
            best.emission = best.albedo * 0;
            best.intersection_point = ray.point_at(rayhit.ray.tfar);
            best.distance = rayhit.ray.tfar;
            best.hit = true;
            best.normal = Vec3(rayhit.hit.Ng_x, rayhit.hit.Ng_y, rayhit.hit.Ng_z).to_unit_vector();
            best.reflectiveness = fmaxf(0.f, best.normal.dot(ray.direction));
        }

        return best;
    }

    void set_skybox(uint32_t image_handle) {
        skybox = image_handle;
    }

    Image::Image(int
                 width, int
                 height,
                 const uint8_t *data) :
            _width(width), _height(height) {
        _data.resize(_width * _height);
        for (size_t i = 0; i < _width * _height; i++) {
            _data[i] |= data[i * 3 + 0] << 0;
            _data[i] |= data[i * 3 + 1] << 8;
            _data[i] |= data[i * 3 + 2] << 16;
            _data[i] |= 0 << 24;
        }
    }

    auto Image::width() const noexcept -> uint32_t {
        return _width;
    }

    auto Image::height() const noexcept -> uint32_t {
        return _height;
    }

    auto Image::get(float u, float v) const noexcept -> uint32_t {
        return get(
                static_cast<uint32_t>(u * _width),
                static_cast<uint32_t>(v * _height));
    }

    auto Image::get(uint32_t x, uint32_t y) const noexcept -> uint32_t {
        return _data[x % _width + y % _height * _width];
    }
}