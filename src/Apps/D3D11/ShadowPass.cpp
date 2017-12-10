#include "ShadowPass.h"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/transform.hpp>
#include <Utilities/State.h>

ShadowPass::ShadowPass(Context& context, const Input& input, int width, int height)
    : m_context(context)
    , m_input(input)
    , m_width(width)
    , m_height(height)
    , m_program(context.device)
{
    CreateTextureDsv();
    CreateViewPort();
}

void ShadowPass::OnUpdate()
{
    glm::vec3 Up = glm::vec3(0.0f, 1.0f, 0.0f);
    glm::vec3 Down = glm::vec3(0.0f, -1.0f, 0.0f);
    glm::vec3 Left = glm::vec3(-1.0f, 0.0f, 0.0f);
    glm::vec3 Right = glm::vec3(1.0f, 0.0f, 0.0f);
    glm::vec3 ForwardRH = glm::vec3(0.0f, 0.0f, -1.0f);
    glm::vec3 ForwardLH = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 BackwardRH = glm::vec3(0.0f, 0.0f, 1.0f);
    glm::vec3 BackwardLH = glm::vec3(0.0f, 0.0f, -1.0f);

    glm::vec3 position = m_input.light_pos;

    m_program.gs.cbuffer.Params.Projection = glm::transpose(glm::perspective(glm::radians(90.0f), 1.0f, m_settings.s_near, m_settings.s_far));

    std::array<glm::mat4, 6>& view = m_program.gs.cbuffer.Params.View;
    view[0] = glm::transpose(glm::lookAt(position, position + Right, Up));
    view[1] = glm::transpose(glm::lookAt(position, position + Left, Up));
    view[2] = glm::transpose(glm::lookAt(position, position + Up, BackwardRH));
    view[3] = glm::transpose(glm::lookAt(position, position + Down, ForwardRH));
    view[4] = glm::transpose(glm::lookAt(position, position + BackwardLH, Up));
    view[5] = glm::transpose(glm::lookAt(position, position + ForwardLH, Up));

    glm::mat4 model = m_input.camera.GetModelMatrix();
    float model_scale = 0.01f;
    model = glm::scale(glm::vec3(model_scale)) * model;

    m_program.vs.cbuffer.Params.World = glm::transpose(model);
}

void ShadowPass::OnRender()
{
    m_context.device_context->RSSetViewports(1, &m_viewport);

    m_program.UseProgram(m_context.device_context);
    m_context.device_context->IASetInputLayout(m_program.vs.input_layout.Get());

    m_context.device_context->ClearDepthStencilView(m_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1.0f, 0);
    m_context.device_context->OMSetRenderTargets(0, nullptr, m_depth_stencil_view.Get());

    auto& state = CurState<bool>::Instance().state;
    for (DX11Mesh& cur_mesh : m_input.model.meshes)
    {
        if (cur_mesh.material.name == "16___Default")
            continue;

        cur_mesh.SetIndexBuffer();
        cur_mesh.SetVertexBuffer(m_program.vs.geometry.SV_POSITION, VertexType::kPosition);
        cur_mesh.SetVertexBuffer(m_program.vs.geometry.TEXCOORD, VertexType::kTexcoord);

        if (!state["no_shadow_discard"])
            cur_mesh.SetTexture(aiTextureType_OPACITY, m_program.ps.texture.alphaMap);
        else
            cur_mesh.UnsetTexture(m_program.ps.texture.alphaMap);

        m_context.device_context->DrawIndexed(cur_mesh.indices.size(), 0, 0);
    }
    m_context.device_context->GSSetShader(nullptr, nullptr, 0);
}

void ShadowPass::OnResize(int width, int height)
{
}

void ShadowPass::OnModifySettings(const Settings& settings)
{
    Settings prev = m_settings;
    m_settings = settings;
    if (prev.s_size != settings.s_size)
    {
        CreateTextureDsv();
        CreateViewPort();
    }
}

void ShadowPass::CreateTextureDsv()
{
    D3D11_TEXTURE2D_DESC texture_desc = {};
    texture_desc.Width = m_settings.s_size;
    texture_desc.Height = m_settings.s_size;
    texture_desc.MipLevels = 1;
    texture_desc.ArraySize = 6;
    texture_desc.Format = DXGI_FORMAT_R32_TYPELESS;
    texture_desc.SampleDesc.Count = 1;
    texture_desc.Usage = D3D11_USAGE_DEFAULT;
    texture_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL | D3D11_BIND_SHADER_RESOURCE;
    texture_desc.MiscFlags = D3D11_RESOURCE_MISC_TEXTURECUBE;

    D3D11_DEPTH_STENCIL_VIEW_DESC dsv_desc = {};
    dsv_desc.Format = DXGI_FORMAT_D32_FLOAT;
    dsv_desc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2DARRAY;
    dsv_desc.Texture2DArray.ArraySize = texture_desc.ArraySize;

    D3D11_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
    srv_desc.Format = DXGI_FORMAT_R32_FLOAT;
    srv_desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURECUBE;
    srv_desc.TextureCube.MipLevels = 1;

    ComPtr<ID3D11Texture2D> cube_texture;
    ASSERT_SUCCEEDED(m_context.device->CreateTexture2D(&texture_desc, nullptr, &cube_texture));
    ASSERT_SUCCEEDED(m_context.device->CreateDepthStencilView(cube_texture.Get(), &dsv_desc, &m_depth_stencil_view));
    ASSERT_SUCCEEDED(m_context.device->CreateShaderResourceView(cube_texture.Get(), &srv_desc, &output.srv));
}

void ShadowPass::CreateViewPort()
{
    ZeroMemory(&m_viewport, sizeof(D3D11_VIEWPORT));
    m_viewport.TopLeftX = 0;
    m_viewport.TopLeftY = 0;
    m_viewport.Width = m_settings.s_size;
    m_viewport.Height = m_settings.s_size;
    m_viewport.MinDepth = 0.0f;
    m_viewport.MaxDepth = 1.0f;
}
