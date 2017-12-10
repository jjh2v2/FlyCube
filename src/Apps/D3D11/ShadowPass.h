#pragma once

#include <Scene/SceneBase.h>
#include <Context/Context.h>
#include <Geometry/DX11Geometry.h>
#include <ProgramRef/ShadowPassVS.h>
#include <ProgramRef/ShadowPassGS.h>
#include <ProgramRef/ShadowPassPS.h>
#include <d3d11.h>
#include <wrl.h>

using namespace Microsoft::WRL;

class ShadowPass : public IPass, public IModifySettings
{
public:
    struct Input
    {
        Model<DX11Mesh>& model;
        Camera& camera;
        glm::vec3& light_pos;
    };

    struct Output
    {
        ComPtr<ID3D11ShaderResourceView> srv;
    } output;

    ShadowPass(Context& context, const Input& input, int width, int height);

    virtual void OnUpdate() override;
    virtual void OnRender() override;
    virtual void OnResize(int width, int height) override;
    virtual void OnModifySettings(const Settings & settings) override;

    void CreateTextureDsv();
    void CreateViewPort();

private:
    Settings m_settings;
    Context& m_context;
    Input m_input;
    int m_width;
    int m_height;
    ComPtr<ID3D11DepthStencilView> m_depth_stencil_view;
    Program<ShadowPassVS, ShadowPassGS, ShadowPassPS> m_program;
    D3D11_VIEWPORT m_viewport;
};

