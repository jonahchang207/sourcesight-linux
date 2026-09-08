#include "common.hpp"
#include "core/engine/classes/MapRaytrace.hpp"
#include "config/Current.hpp"
#include <fstream>
#include <thread>
#include <limits>
#include <iostream>

static void require(bool ok, const char* message) {
    if (!ok) throw std::runtime_error(message);
}

int main(int argc, char** argv) {
    if (argc != 2) return 2;
    LogHelper::Init();
    const auto dir = std::filesystem::current_path();
    using namespace MapRaytrace;
    Init(dir.string());
    auto write = [&](const char* name, std::vector<Triangle> tris) {
        std::ofstream file(dir / (std::string(name)+".tri"), std::ios::binary);
        file.write(reinterpret_cast<const char*>(tris.data()), tris.size()*sizeof(Triangle));
    };
    const Triangle front{{-30,-5,10},{30,-5,10},{0,30,10}};
    write("front",{front});
    require(LoadMap("front"),"load fixture");
    require(!IsVisible({0,0,0},{0,0,20}),"wall must block ray");
    require(IsVisible({0,0,0},{0,0,5}),"wall beyond endpoint must not block");
    require(IsVisible({100,100,0},{100,100,20}),"miss must remain visible");
    require(!LoadMap("../front"),"reject path traversal");

    ImGui::CreateContext();
    auto& io=ImGui::GetIO();
    io.IniFilename=nullptr;io.DisplaySize=ImVec2(800,600);io.DeltaTime=1.f/60;
    io.Fonts->AddFontDefault();
    unsigned char* pixels;int w,h;
    io.Fonts->GetTexDataAsRGBA32(&pixels,&w,&h);io.Fonts->SetTexID(1);
    view_matrix_t m{};
    m[0][0]=1;m[1][1]=1;m[3][2]=1; // Row 2 deliberately zero.
    cfg::esp::wireframe_max_dist=3000;
    auto render=[&](Vec3_t eye=Vec3_t(0,0,0)) {
        ImGui::NewFrame();
        auto* d=ImGui::GetBackgroundDrawList();
        RenderWireframe(m,io,d,eye);
        for (const auto& v:d->VtxBuffer)
            require(std::isfinite(v.pos.x)&&std::isfinite(v.pos.y),"finite clipped vertex");
        int count=d->VtxBuffer.Size;
        require(count<=32000,"wireframe draw budget");
        ImGui::Render();
        return count;
    };
    require(render()>0,"off-screen endpoints must produce clipped visible edges");
    cfg::esp::wireframe_opacity=0;
    require(render()==0,"zero opacity must skip rendering");
    cfg::esp::wireframe_opacity=.65f;
    write("behind",{{{-3,-5,-10},{3,-5,-10},{0,3,-10}}});
    require(LoadMap("behind"),"load behind fixture");
    require(render()==0,"behind-camera mesh must be culled");
    write("crossing",{{{-2,-1,-1},{2,-1,10},{0,2,10}}});
    require(LoadMap("crossing"),"load near-plane fixture");
    require(render()>0,"near-plane crossing must remain visible");
    auto invalid=front;invalid.p1.x=std::numeric_limits<float>::quiet_NaN();
    write("mixed",{invalid,front,{{0,0,0},{0,0,0},{0,0,0}}});
    require(LoadMap("mixed")&&TriangleCount()==1,"filter corrupt and degenerate geometry");
    {std::ofstream f(dir/"broken.tri");f<<"broken";}
    require(!LoadMap("broken")&&!IsReady()&&CurrentMap().empty(),"bad mesh clears old state");
    SetDesiredMap("front");
    require(!LoadMap("behind"),"obsolete map cannot publish");
    require(LoadMap("front"),"desired map can publish");
    std::thread worker([&] {for(int i=0;i<30;++i){Unload();LoadMap("front");}});
    for(int i=0;i<100;++i) {IsVisible({0,0,0},{0,0,20});render();}
    worker.join();
    SetDesiredMap("de_dust2");
    Init(argv[1]);
    require(LoadMap("de_dust2"),"bundled map load");
    cfg::esp::wireframe_budget=500;
    require(render()<=2000,"low detail must enforce its edge budget");
    cfg::esp::wireframe_budget=6000;
    auto start=std::chrono::steady_clock::now();
    int vertices=0;
    for(int i=0;i<100;++i) vertices=render();
    std::cout<<"PASS: clipping, ray queries, invalid files, cancellation and concurrent snapshots\n";
    std::cout<<TriangleCount()<<" triangles; "<<vertices<<" wireframe vertices; "
             <<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count()/100
             <<" ms/frame (synthetic camera)\n";
    Unload();ImGui::DestroyContext();LogHelper::Destroy();
}
