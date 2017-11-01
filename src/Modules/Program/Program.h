﻿#pragma once

#include "Program/ShaderType.h"
#include <Utilities/DXUtility.h>
#include <Utilities/FileUtility.h>
#include <d3dcompiler.h>
#include <d3d11.h>
#include <wrl.h>
#include <cstddef>
#include <map>
#include <string>

using namespace Microsoft::WRL;

struct BufferLayout
{
    BufferLayout(
        ComPtr<ID3D11Device>& device,
        size_t buffer_size, 
        UINT slot,
        std::vector<size_t>&& data_size,
        std::vector<size_t>&& data_offset,
        std::vector<size_t>&& buf_offset
    )
        : buffer(CreateBuffer(device, buffer_size))
        , slot(slot)
        , data(buffer_size)
        , data_size(std::move(data_size))
        , data_offset(std::move(data_offset))
        , buf_offset(std::move(buf_offset))
    {
    }

    ComPtr<ID3D11Buffer> buffer;
    UINT slot;

    void UpdateCBuffer(ComPtr<ID3D11DeviceContext>& device_context, const char* src_data)
    {
        bool dirty = false;
        for (size_t i = 0; i < data_size.size(); ++i)
        {
            const char* ptr_src = src_data + data_offset[i];
            char* ptr_dst = data.data() + buf_offset[i];
            if (std::memcmp(ptr_dst, ptr_src, data_size[i]) != 0)
            {
                std::memcpy(ptr_dst, ptr_src, data_size[i]);
                dirty = true;
            }
        }

        if (dirty)
            device_context->UpdateSubresource(buffer.Get(), 0, nullptr, data.data(), 0, 0);
    }

    template<ShaderType>
    void BindCBuffer(ComPtr<ID3D11DeviceContext>& device_context);

    template<>
    void BindCBuffer<ShaderType::kPixel>(ComPtr<ID3D11DeviceContext>& device_context)
    {
        device_context->PSSetConstantBuffers(slot, 1, buffer.GetAddressOf());
    }

    template<>
    void BindCBuffer<ShaderType::kVertex>(ComPtr<ID3D11DeviceContext>& device_context)
    {
        device_context->VSSetConstantBuffers(slot, 1, buffer.GetAddressOf());
    }

private:
    std::vector<char> data;
    std::vector<size_t> data_size;
    std::vector<size_t> data_offset;
    std::vector<size_t> buf_offset;

    ComPtr<ID3D11Buffer> CreateBuffer(ComPtr<ID3D11Device>& device, UINT buffer_size)
    {
        ComPtr<ID3D11Buffer> buffer;
        D3D11_BUFFER_DESC cbbd;
        ZeroMemory(&cbbd, sizeof(D3D11_BUFFER_DESC));
        cbbd.Usage = D3D11_USAGE_DEFAULT;
        cbbd.ByteWidth = buffer_size;
        cbbd.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
        cbbd.CPUAccessFlags = 0;
        cbbd.MiscFlags = 0;
        ASSERT_SUCCEEDED(device->CreateBuffer(&cbbd, nullptr, &buffer));
        return buffer;
    }
};

class ShaderBase
{
protected:
    
public:
    virtual ~ShaderBase() = default;

    virtual void UseShader(ComPtr<ID3D11DeviceContext>& device_context) = 0;
    virtual void BindCBuffers(ComPtr<ID3D11DeviceContext>& device_context) = 0;
    virtual void UpdateCBuffers(ComPtr<ID3D11DeviceContext>& device_context) = 0;

    ShaderBase(const std::string& shader_path, const std::string& entrypoint, const std::string& target)
    {
        ComPtr<ID3DBlob> errors;
        ASSERT_SUCCEEDED(D3DCompileFromFile(
            GetAssetFullPathW(shader_path).c_str(),
            nullptr,
            nullptr,
            entrypoint.c_str(),
            target.c_str(),
            0,
            0,
            &m_shader_buffer,
            &errors));
    }

protected:
    ComPtr<ID3DBlob> m_shader_buffer;
};

template<ShaderType> class Shader : public ShaderBase {};

template<>
class Shader<ShaderType::kPixel> : public ShaderBase
{
public:
    static const ShaderType type = ShaderType::kPixel;

    ComPtr<ID3D11PixelShader> shader;

    Shader(ComPtr<ID3D11Device>& device, const std::string& shader_path, const std::string& entrypoint, const std::string& target)
        : ShaderBase(shader_path, entrypoint, target)
    {
        ASSERT_SUCCEEDED(device->CreatePixelShader(m_shader_buffer->GetBufferPointer(), m_shader_buffer->GetBufferSize(), nullptr, &shader));
    }

    virtual void UseShader(ComPtr<ID3D11DeviceContext>& device_context) override
    {
        device_context->PSSetShader(shader.Get(), nullptr, 0);
    }
};

template<>
class Shader<ShaderType::kVertex> : public ShaderBase
{
public:
    static const ShaderType type = ShaderType::kVertex;

    ComPtr<ID3D11VertexShader> shader;
    ComPtr<ID3D11InputLayout> input_layout;

    Shader(ComPtr<ID3D11Device>& device, const std::string& shader_path, const std::string& entrypoint, const std::string& target)
        : ShaderBase(shader_path, entrypoint, target)
    {
        ASSERT_SUCCEEDED(device->CreateVertexShader(m_shader_buffer->GetBufferPointer(), m_shader_buffer->GetBufferSize(), nullptr, &shader));

        D3D11_INPUT_ELEMENT_DESC layout[] =
        {
            { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DX11Mesh::Vertex, position), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DX11Mesh::Vertex, normal), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, offsetof(DX11Mesh::Vertex, tangent), D3D11_INPUT_PER_VERTEX_DATA, 0 },
            { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, offsetof(DX11Mesh::Vertex, texCoords), D3D11_INPUT_PER_VERTEX_DATA, 0 },
        };

        ASSERT_SUCCEEDED(device->CreateInputLayout(layout, ARRAYSIZE(layout), m_shader_buffer->GetBufferPointer(),
            m_shader_buffer->GetBufferSize(), &input_layout));
    }

    virtual void UseShader(ComPtr<ID3D11DeviceContext>& device_context) override
    {
        device_context->VSSetShader(shader.Get(), nullptr, 0);
    }
};

template<ShaderType, typename T> class ShaderHolderImpl {};

template<typename T>
class ShaderHolderImpl<ShaderType::kPixel, T>
{
public:
    struct Api : public T
    {
        using T::T;
    } ps;

    ShaderBase& GetApi()
    {
        return ps;
    }

    ShaderHolderImpl(ComPtr<ID3D11Device>& device)
        : ps(device)
    {
    }
};

template<typename T>
class ShaderHolderImpl<ShaderType::kVertex, T>
{
public:
    struct Api : public T
    {
        using T::T;
    } vs;

    ShaderBase& GetApi()
    {
        return vs;
    }

    ShaderHolderImpl(ComPtr<ID3D11Device>& device)
        : vs(device)
    {
    }
};

template<typename T> class ShaderHolder : public ShaderHolderImpl<T::type, T> { using ShaderHolderImpl::ShaderHolderImpl; };

template<typename ... Args>
class Program : public ShaderHolder<Args>...
{
public:
    Program(ComPtr<ID3D11Device>& device)
        : ShaderHolder<Args>(device)...
        , m_shaders({ ShaderHolder<Args>::GetApi()... })
    {
    }

    void UseProgram(ComPtr<ID3D11DeviceContext>& device_context)
    {
        for (auto& shader : m_shaders)
        {
            shader.get().UseShader(device_context);
            shader.get().UpdateCBuffers(device_context);
            shader.get().BindCBuffers(device_context);
        }
    }

private:
    std::vector<std::reference_wrapper<ShaderBase>> m_shaders;
};