// ------------------------------------------------------------------------------------
// Private Helpers

typedef struct d3d11_format
{
    DXGI_FORMAT Native;
    uint32_t    BytesPerPixel;
} d3d11_format;

static d3d11_renderer *
D3D11GetRenderer(render_handle HRenderer)
{
    d3d11_renderer *Result = (d3d11_renderer *)HRenderer.Value[0];
    return Result;
}

static ID3D11Texture2D *
D3D11GetTexture2D(render_handle Handle)
{
    ID3D11Texture2D *Result = (ID3D11Texture2D *)Handle.Value[0];
    return Result;
}

static ID3D11ShaderResourceView *
D3D11GetShaderView(render_handle Handle)
{
    ID3D11ShaderResourceView *Result = (ID3D11ShaderResourceView *)Handle.Value[0];
    return Result;
}

static ID3D11Buffer *
D3D11GetVertexBuffer(uint64_t Size, d3d11_renderer *Renderer)
{
    ID3D11Buffer *Result = Renderer->VBuffer64KB;

    if(Size > VOID_KILOBYTE(64))
    {
        VOID_ASSERT(!"Implement");
    }

    return Result;
}

static void
D3D11Release(d3d11_renderer *Renderer)
{
    VOID_ASSERT(Renderer);

    if (Renderer->Device)
    {
        Renderer->Device->Release();
        Renderer->Device = nullptr;
    }

    if (Renderer->DeviceContext)
    {
        Renderer->DeviceContext->Release();
        Renderer->DeviceContext = nullptr;
    }

    if (Renderer->RenderView)
    {
        Renderer->RenderView->Release();
        Renderer->RenderView = nullptr;
    }

    if (Renderer->SwapChain)
    {
       Renderer->SwapChain->Release();
       Renderer->SwapChain = nullptr;
    }
}

static d3d11_format
D3D11GetFormat(RenderTextureFormat Format)
{
    d3d11_format Result = {};

    switch(Format)
    {
        case RenderTextureFormat::RGBA:
        {
            Result.Native        = DXGI_FORMAT_R8G8B8A8_UNORM;
            Result.BytesPerPixel = 4;
        } break;

        default:
        {
            VOID_ASSERT(!"Invalid Format Type");
        } break;
    }

    return Result;
}

static bool
D3D11IsValidFormat(d3d11_format Format)
{
    bool Result = (Format.BytesPerPixel > 0) && (Format.Native != DXGI_FORMAT_UNKNOWN);
    return Result;
}

// [PER-RENDERER API]

static render_handle
InitializeRenderer(void *HWindow, vec2_int Resolution, memory_arena *Arena)
{
    render_handle   Result   = { 0 };
    d3d11_renderer *Renderer = PushArray(Arena, d3d11_renderer, 1); 
    HRESULT         Error    = S_OK;

    {
        UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        #ifndef NDEBUG
        CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif
        D3D_FEATURE_LEVEL Levels[] = { D3D_FEATURE_LEVEL_11_0 };
        Error = D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                                  CreateFlags, Levels, ARRAYSIZE(Levels),
                                  D3D11_SDK_VERSION, &Renderer->Device, NULL, &Renderer->DeviceContext);
        if (FAILED(Error))
        {
            return Result;
        }
    }

    {
        IDXGIDevice *DXGIDevice = nullptr;
        Error = Renderer->Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&DXGIDevice);
        if (DXGIDevice)
        {
            IDXGIAdapter *DXGIAdapter = nullptr;
            DXGIDevice->GetAdapter(&DXGIAdapter);
            if (DXGIAdapter)
            {
                IDXGIFactory2 *Factory = nullptr;
                DXGIAdapter->GetParent(__uuidof(IDXGIFactory2), (void **)&Factory);
                if (Factory)
                {
                    HWND Window = (HWND)HWindow;

                    DXGI_SWAP_CHAIN_DESC1 Desc = { 0 };
                    Desc.Format           = DXGI_FORMAT_R8G8B8A8_UNORM;
                    Desc.SampleDesc.Count = 1;
                    Desc.BufferUsage      = DXGI_USAGE_RENDER_TARGET_OUTPUT;
                    Desc.BufferCount      = 2;
                    Desc.Scaling          = DXGI_SCALING_NONE;
                    Desc.SwapEffect       = DXGI_SWAP_EFFECT_FLIP_DISCARD;

                    Error = Factory->CreateSwapChainForHwnd((IUnknown *)Renderer->Device,
                                                            Window, &Desc, 0, 0,
                                                            &Renderer->SwapChain);

                    Factory->MakeWindowAssociation(Window, DXGI_MWA_NO_ALT_ENTER);
                    Factory->Release();
                }

                DXGIAdapter->Release();
            }

            DXGIDevice->Release();
        }

        if (FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Render Target View
    {
        ID3D11Texture2D *BackBuffer = nullptr;
        Error = Renderer->SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&BackBuffer);
        if(BackBuffer)
        {
            Error = Renderer->Device->CreateRenderTargetView((ID3D11Resource*)BackBuffer, NULL, &Renderer->RenderView);

            BackBuffer->Release();
        }

        if(FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Default Buffers
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = VOID_KILOBYTE(64);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Error = Renderer->Device->CreateBuffer(&Desc, NULL, &Renderer->VBuffer64KB);
        if(FAILED(Error))
        {
            D3D11Release(Renderer);
            return Result;
        }
    }

    // Default Shaders
    {
        ID3D11Device       *Device  = Renderer->Device;
        ID3D11InputLayout  *ILayout = nullptr;
        ID3D11VertexShader *VShader = nullptr;
        ID3D11PixelShader  *PShader = nullptr;

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            byte_string Source = D3D11ShaderSourceTable[Type];

            ID3DBlob *VShaderSrcBlob = nullptr;
            ID3DBlob *VShaderErrBlob = nullptr;
            Error = D3DCompile(Source.String, Source.Size,
                               0, 0, 0, "VSMain", "vs_5_0",
                               0, 0, &VShaderSrcBlob, &VShaderErrBlob);
            if(FAILED(Error))
            {
            }

            void *ByteCode = VShaderSrcBlob->GetBufferPointer();
            uint64_t   ByteSize = VShaderSrcBlob->GetBufferSize();
            Error = Device->CreateVertexShader(ByteCode, ByteSize, 0, &VShader);
            if(FAILED(Error))
            {
            }

            d3d11_input_layout Layout = D3D11ILayoutTable[Type];
            Error = Device->CreateInputLayout(Layout.Desc, Layout.Count, ByteCode, ByteSize, &ILayout);
            if(FAILED(Error))
            {
            }

            VShaderSrcBlob->Release();

            Renderer->VShaders[Type] = VShader;
            Renderer->ILayouts[Type] = ILayout;
        }

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            byte_string Source = D3D11ShaderSourceTable[Type];

            ID3DBlob *PShaderSrcBlob = nullptr;
            ID3DBlob *PShaderErrBlob = nullptr;
            Error = D3DCompile(Source.String, Source.Size,
                               0, 0, 0, "PSMain", "ps_5_0",
                               0, 0, &PShaderSrcBlob, &PShaderErrBlob);
            if(FAILED(Error))
            {
            }

            void *ByteCode = PShaderSrcBlob->GetBufferPointer();
            uint64_t   ByteSize = PShaderSrcBlob->GetBufferSize();
            Error = Device->CreatePixelShader(ByteCode, ByteSize, 0, &PShader);
            if (FAILED(Error))
            {
            }

            PShaderSrcBlob->Release();

            Renderer->PShaders[Type] = PShader;
        }
    }

    // Uniform Buffers
    {
        ID3D11Device *Device = Renderer->Device;

        ForEachEnum(RenderPass_Type, RenderPass_Count, Type)
        {
            ID3D11Buffer *Buffer = nullptr;

            D3D11_BUFFER_DESC Desc = {};
            Desc.ByteWidth      = D3D11UniformBufferSizeTable[Type];
            Desc.Usage          = D3D11_USAGE_DYNAMIC;
            Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
            Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

            AlignMultiple(Desc.ByteWidth, 16);

            Device->CreateBuffer(&Desc, 0, &Buffer);

            Renderer->UBuffers[Type] = Buffer;
        }
    }

    // Blending
    {
        D3D11_BLEND_DESC BlendDesc = {};
        BlendDesc.RenderTarget[0].BlendEnable           = TRUE;
        BlendDesc.RenderTarget[0].SrcBlend              = D3D11_BLEND_SRC_ALPHA;
        BlendDesc.RenderTarget[0].DestBlend             = D3D11_BLEND_INV_SRC_ALPHA;
        BlendDesc.RenderTarget[0].BlendOp               = D3D11_BLEND_OP_ADD;
        BlendDesc.RenderTarget[0].SrcBlendAlpha         = D3D11_BLEND_ONE;
        BlendDesc.RenderTarget[0].DestBlendAlpha        = D3D11_BLEND_ZERO;
        BlendDesc.RenderTarget[0].BlendOpAlpha          = D3D11_BLEND_OP_ADD;
        BlendDesc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;

        if (FAILED(Renderer->Device->CreateBlendState(&BlendDesc, &Renderer->DefaultBlendState)))
        {
        }
    }

    // Samplers
    {
        D3D11_SAMPLER_DESC Desc = {};
        Desc.Filter         = D3D11_FILTER_MIN_MAG_MIP_POINT;
        Desc.AddressU       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressV       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.AddressW       = D3D11_TEXTURE_ADDRESS_CLAMP;
        Desc.MaxAnisotropy  = 1;
        Desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
        Desc.MaxLOD         = D3D11_FLOAT32_MAX;

        Error = Renderer->Device->CreateSamplerState(&Desc, &Renderer->AtlasSamplerState);
        if (FAILED(Error))
        {
        }
    }

    // Raster State
    {
        D3D11_RASTERIZER_DESC Desc = {};
        Desc.FillMode              = D3D11_FILL_SOLID;
        Desc.CullMode              = D3D11_CULL_BACK;
        Desc.FrontCounterClockwise = FALSE;
        Desc.DepthClipEnable       = TRUE;
        Desc.ScissorEnable         = TRUE; 
        Desc.MultisampleEnable     = FALSE;
        Desc.AntialiasedLineEnable = FALSE;

        Error = Renderer->Device->CreateRasterizerState(&Desc, &Renderer->RasterSt[RenderPass_UI]);
        if(FAILED(Error))
        {
        }
    }

    // Default State
    {
        Renderer->LastResolution = Resolution;
    }

    Result.Value[0] = (uint64_t)Renderer;
    return Result;
}

static void 
SubmitRenderCommands(render_handle HRenderer, vec2_int Resolution, render_pass_list *RenderPassList)
{
    d3d11_renderer *Renderer = D3D11GetRenderer(HRenderer);

    if(!Renderer || !RenderPassList)
    {
        return;
    }

    ID3D11DeviceContext    *DeviceContext = Renderer->DeviceContext;
    ID3D11RenderTargetView *RenderView    = Renderer->RenderView; 
    IDXGISwapChain1        *SwapChain     = Renderer->SwapChain;

    // Update State
    vec2_int CurrentResolution = Renderer->LastResolution;
    {
        if (!(Resolution == CurrentResolution))
        {
            // TODO: Resize swap chain and whatever else we need.

            Resolution = CurrentResolution;
        }
    }

    // Temporary Clear Screen
    {
        const FLOAT ClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
        DeviceContext->ClearRenderTargetView(RenderView, ClearColor);
    }

    for (render_pass_node *PassNode = RenderPassList->First; PassNode != 0; PassNode = PassNode->Next)
    {
        render_pass Pass = PassNode->Value;

        switch (Pass.Type)
        {

        case RenderPass_UI:
        {
            render_pass_params_ui Params = Pass.Params.UI.Params;

            // Rasterizer
            D3D11_VIEWPORT Viewport = { 0.0f, 0.0f, (float)Resolution.X, (float)Resolution.Y, 0.0f, 1.0f };
            DeviceContext->RSSetState(Renderer->RasterSt[RenderPass_UI]);
            DeviceContext->RSSetViewports(1, &Viewport);

            for (rect_group_node *Node = Params.First; Node != 0; Node = Node->Next)
            {
                render_batch_list BatchList  = Node->BatchList;
                rect_group_params NodeParams = Node->Params;

                // Vertex Buffer
                ID3D11Buffer *VBuffer = D3D11GetVertexBuffer(BatchList.ByteCount, Renderer);
                {
                    D3D11_MAPPED_SUBRESOURCE Resource = { 0 };
                    DeviceContext->Map((ID3D11Resource *)VBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);

                    uint8_t *WritePointer = static_cast<uint8_t*>(Resource.pData);
                    uint64_t WriteOffset  = 0;
                    for (render_batch_node *Batch = BatchList.First; Batch != 0; Batch = Batch->Next)
                    {
                        uint64_t WriteSize = Batch->Value.ByteCount;
                        MemoryCopy(WritePointer + WriteOffset, Batch->Value.Memory, WriteSize);
                        WriteOffset += WriteSize;
                    }

                    DeviceContext->Unmap((ID3D11Resource *)VBuffer, 0);
                }

                // Uniform Buffers
                ID3D11Buffer *UniformBuffer = Renderer->UBuffers[RenderPass_UI];
                {
                    d3d11_rect_uniform_buffer Uniform = {};
                    Uniform.Transform[0]        = Vec4float(NodeParams.Transform.c0r0, NodeParams.Transform.c0r1, NodeParams.Transform.c0r2, 0);
                    Uniform.Transform[1]        = Vec4float(NodeParams.Transform.c1r0, NodeParams.Transform.c1r1, NodeParams.Transform.c1r2, 0);
                    Uniform.Transform[2]        = Vec4float(NodeParams.Transform.c2r0, NodeParams.Transform.c2r1, NodeParams.Transform.c2r2, 0);
                    Uniform.ViewportSizeInPixel = vec2_float((float)Resolution.X, (float)Resolution.Y);
                    Uniform.AtlasSizeInPixel    = vec2_float((float)NodeParams.TextureSize.X, (float)NodeParams.TextureSize.Y);

                    D3D11_MAPPED_SUBRESOURCE Resource = {};
                    DeviceContext->Map((ID3D11Resource *)UniformBuffer, 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
                    MemoryCopy(Resource.pData, &Uniform, sizeof(Uniform));
                    DeviceContext->Unmap((ID3D11Resource *)UniformBuffer, 0);
                }

                // Pipeline Info
                ID3D11InputLayout        *ILayout   = Renderer->ILayouts[RenderPass_UI];
                ID3D11VertexShader       *VShader   = Renderer->VShaders[RenderPass_UI];
                ID3D11PixelShader        *PShader   = Renderer->PShaders[RenderPass_UI];
                ID3D11ShaderResourceView *AtlasView = D3D11GetShaderView(NodeParams.Texture);

                // OM
                DeviceContext->OMSetRenderTargets(1, &Renderer->RenderView, 0);
                DeviceContext->OMSetBlendState(Renderer->DefaultBlendState, 0, 0xFFFFFFFF);

                // IA
                uint32_t Stride = (uint32_t)BatchList.BytesPerInstance;
                uint32_t Offset = 0;
                DeviceContext->IASetVertexBuffers(0, 1, &VBuffer, &Stride, &Offset);
                DeviceContext->IASetInputLayout(ILayout);
                DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);

                // Shaders
                DeviceContext->VSSetShader(VShader, 0, 0);
                DeviceContext->VSSetConstantBuffers(0, 1, &UniformBuffer);
                DeviceContext->PSSetShader(PShader, 0, 0);
                DeviceContext->PSSetShaderResources(0, 1, &AtlasView);
                DeviceContext->PSSetSamplers(0, 1, &Renderer->AtlasSamplerState);

                // Scissor
                D3D11_RECT Rect = {};
                {
                    rect_float Clip = NodeParams.Clip;

                    if (Clip.Left == 0 && Clip.Top == 0 && Clip.Right == 0 && Clip.Bottom == 0)
                    {
                        Rect.left   = 0;
                        Rect.right  = Resolution.X;
                        Rect.top    = 0;
                        Rect.bottom = Resolution.Y;
                    } else
                    if (Clip.Left > Clip.Right || Clip.Top > Clip.Bottom)
                    {
                        Rect.left   = 0;
                        Rect.right  = 0;
                        Rect.top    = 0;
                        Rect.bottom = 0;
                    }
                    else
                    {
                        Rect.left   = Clip.Left;
                        Rect.right  = Clip.Right;
                        Rect.top    = Clip.Top;
                        Rect.bottom = Clip.Bottom;
                    }

                    DeviceContext->RSSetScissorRects(1, &Rect);
                }

                // Draw
                uint32_t InstanceCount = (uint32_t)(BatchList.ByteCount / BatchList.BytesPerInstance);
                DeviceContext->DrawInstanced(4, InstanceCount, 0, 0);
            }
        } break;

        default: break;

        }
    }

    // Present
    HRESULT Error = SwapChain->Present(0, 0);
    if (FAILED(Error))
    {
        VOID_ASSERT(!"Failed I guess");
    }
    DeviceContext->ClearState();

    // Clear
    RenderPassList->First = 0;
    RenderPassList->Last  = 0;
}

 
// -----------------------------------------------------------------------------------
// @Internal: Texture Helpers

struct d3d11_texture_flags
{
    D3D11_USAGE Usage;
    UINT        BindFlags;
    UINT        CPUAccessFlags;
};

static d3d11_texture_flags
D3D11GetTextureFlags(RenderTextureType Type)
{
    d3d11_texture_flags Result = {};
    
    switch(Type)
    {
        case RenderTextureType::Default:
        {
            Result.Usage = D3D11_USAGE_DEFAULT;
            Result.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            Result.CPUAccessFlags = 0;
        } break;
        
        case RenderTextureType::Dynamic:
        {
            Result.Usage = D3D11_USAGE_DYNAMIC;
            Result.BindFlags = D3D11_BIND_SHADER_RESOURCE;
            Result.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
        } break;
        
        case RenderTextureType::Staging:
        {
            Result.Usage = D3D11_USAGE_STAGING;
            Result.BindFlags = 0;
            Result.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE | D3D11_CPU_ACCESS_READ;
        } break;
        
        default:
        {
            VOID_ASSERT(!"Invalid RenderTextureType");
        } break;
    }
    
    return Result;
}

// -----------------------------------------------------------------------------------
// @Public: Texture API


static render_handle
CreateRenderTexture(render_texture_params Params)
{
    VOID_ASSERT(Params.SizeX > 0 && Params.SizeY > 0);
    VOID_ASSERT(Params.Format != RenderTextureFormat::None);

    render_handle Result = RenderHandle(0);

    d3d11_format        Format  = D3D11GetFormat(Params.Format);
    d3d11_texture_flags Flags   = D3D11GetTextureFlags(Params.Type);
    d3d11_renderer     *Backend = D3D11GetRenderer(RenderState.Renderer);
    VOID_ASSERT(Backend);

    if(D3D11IsValidFormat(Format) && Backend)
    {
        D3D11_TEXTURE2D_DESC TextureDesc =
        {
            .Width          = Params.SizeX,
            .Height         = Params.SizeY,
            .MipLevels      = 1,
            .ArraySize      = 1,
            .Format         = Format.Native,
            .SampleDesc     = {.Count   = 1, .Quality = 0,},
            .Usage          = Flags.Usage,
            .BindFlags      = Flags.BindFlags,
            .CPUAccessFlags = Flags.CPUAccessFlags,
        };

        Backend->Device->CreateTexture2D(&TextureDesc, nullptr, reinterpret_cast<ID3D11Texture2D **>(&Result.Value));
    }

    return Result;
}


static render_handle
CreateRenderTextureView(render_handle TextureHandle, RenderTextureFormat TextureFormat)
{
    render_handle Result = {};

    d3d11_renderer *Backend = D3D11GetRenderer(RenderState.Renderer);
    VOID_ASSERT(Backend);

    if(IsValidRenderHandle(TextureHandle) && Backend)
    {
        ID3D11Texture2D *Texture = D3D11GetTexture2D(TextureHandle);

        d3d11_format Format = D3D11GetFormat(TextureFormat);
        if(D3D11IsValidFormat(Format))
        {
            D3D11_SHADER_RESOURCE_VIEW_DESC ViewDesc =
            {
                .Format        = Format.Native,
                .ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D,
                .Texture2D     =
                {
                    .MostDetailedMip = 0,
                    .MipLevels       = 1,
                },
            };

            Backend->Device->CreateShaderResourceView(static_cast<ID3D11Resource *>(Texture), &ViewDesc, reinterpret_cast<ID3D11ShaderResourceView **>(&Result.Value));
        }
    }

    return Result;
}

// TODO: Safer code

// This is still too involved I think. It has to be simpler. It's really rough. But it works.

static void
UpdateGlyphCache(render_handle CacheTransferHandle, render_handle CacheHandle, const ntext::rasterized_glyph_list &List)
{
    d3d11_renderer *Backend = D3D11GetRenderer(RenderState.Renderer);
    VOID_ASSERT(Backend);

    ID3D11Texture2D *StagingTexture = D3D11GetTexture2D(CacheTransferHandle);
    ID3D11Texture2D *CacheTexture  = D3D11GetTexture2D(CacheHandle);
    VOID_ASSERT(StagingTexture && CacheTexture);

    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    HRESULT HR = Backend->DeviceContext->Map(StagingTexture, 0, D3D11_MAP_WRITE, 0, &Mapped);
    VOID_ASSERT(SUCCEEDED(HR) && Mapped.pData);

    // Atlas is forced to RGBA8 for now.
    uint32_t AtlasBPP      = 4;
    uint8_t *AtlasBase     = (uint8_t *)Mapped.pData;
    uint32_t AtlasRowPitch = (uint32_t)Mapped.RowPitch;

    for (ntext::rasterized_glyph_node *Node = List.First; Node != 0; Node = Node->Next)
    {
        ntext::rasterized_glyph  &Glyph  = Node->Value;
        ntext::rasterized_buffer &Buffer = Glyph.Buffer;

        VOID_ASSERT(Buffer.BytesPerPixel == 1);

        uint32_t DstLeft   = (uint32_t)Glyph.Source.Left;
        uint32_t DstTop    = (uint32_t)Glyph.Source.Top;
        uint32_t DstRight  = (uint32_t)Glyph.Source.Right;
        uint32_t DstBottom = (uint32_t)Glyph.Source.Bottom;

        uint32_t CopyWidth  = (DstRight  > DstLeft) ? (DstRight - DstLeft) : 0;
        uint32_t CopyHeight = (DstBottom > DstTop)  ? (DstBottom - DstTop) : 0;

        if (CopyWidth == 0 || CopyHeight == 0) continue;

        VOID_ASSERT(AtlasRowPitch >= CopyWidth * AtlasBPP);

        uint8_t *SrcBase   = (uint8_t *)Buffer.Data;
        uint32_t SrcStride = Buffer.Stride ? Buffer.Stride : CopyWidth;

        for (uint32_t y = 0; y < Buffer.Height; ++y)
        {
            uint8_t *SrcRow = SrcBase   + (size_t)y * SrcStride;
            uint8_t *DstRow = AtlasBase + (size_t)(DstTop + y) * AtlasRowPitch + (size_t)DstLeft * AtlasBPP;

            for (uint32_t x = 0; x < Buffer.Width; ++x)
            {
                uint8_t  Alpha    = SrcRow[x];
                uint8_t *DstPixel = DstRow + (size_t)x * AtlasBPP;

                DstPixel[0] = 0xFF;
                DstPixel[1] = 0xFF;
                DstPixel[2] = 0xFF;
                DstPixel[3] = Alpha;
            }
        }
    }

    Backend->DeviceContext->Unmap(StagingTexture, 0);

    // So the easiest way is to copy the content of the staging texture to the cache. Uhm.
    // I guess one problem is that this might get called multiple times per frame which we could avoid.
    // This will move, but fine for now.

    Backend->DeviceContext->CopyResource(CacheTexture, StagingTexture);
}
