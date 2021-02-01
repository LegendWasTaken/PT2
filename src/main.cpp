#include <pt2/pt2.h>

int main()
{

    auto renderer = PT2::Renderer();
    renderer.load_model("./assets/models/stanford-dragon.obj", PT2::ModelType::OBJ);
        renderer.start_gui();

/*
    //    pt2::load_model("../assets/models/1967-shelby-ford-mustang.obj");
//    pt2::load_model("../assets/models/mustang/", "1967-shelby-ford-mustang.obj");
    pt2::load_model("../assets/models/showcase-mustang/", "mustang.obj");
//    pt2::load_model("../assets/models/cup/", "Cup_Made_By_Tyro_Smith.obj");
//    pt2::load_model("../assets/models/lambo/", "lambo-dp2020.obj");
    //    pt2::load_model("../assets/models/dump-truck/", "mining-dump-truck.obj");
//        pt2::load_model("../assets/models/", "tridel-interior-test.obj");
//        pt2::load_model("../assets/models/audi/", "audi-r8-red.obj");
//        pt2::load_model("../assets/models/interior/", "interior.obj");
    //        pt2::load_model("../assets/models/sponza/", "crytek-sponza-huge-vray.obj");
//            pt2::load_model("../assets/models/", "stanford-dragon.obj");

    auto skybox_handle = pt2::load_image("../assets/images/abandoned_parking.jpg");
    if (skybox_handle != pt2::INVALID_HANDLE) pt2::set_skybox(skybox_handle);

    pt2::SceneRenderDetail detail {};
    detail.width         = 512;
    detail.height        = 512;
    detail.max_bounces   = 1024;
    detail.spp           = 128;
    detail.thread_count  = pt2::MAX_THREAD_COUNT;
    detail.camera.origin = { -3, 3, -7 };
    detail.camera.lookat = { 0, 2, 0 };
    detail.camera.fov    = 90;

    // Good for Sponza
    //    detail.camera.origin = { -600.f, 500.f, -5.f };
    //    detail.camera.lookat = { 0.f, 200.f, 0.f };

    // Good for tridel interior test scene
    //    detail.camera.origin = {-3.5f, 1.5f, -2.f};
    //    detail.camera.lookat = {0.f, .8f, -3.f};

//        detail.camera.origin = {0.4f, .7f, -2.26f};
//        detail.camera.lookat = {-3.f, .6f, 1.6f};

    pt2::render_and_display_scene(detail);
*/
    return 0;
}
