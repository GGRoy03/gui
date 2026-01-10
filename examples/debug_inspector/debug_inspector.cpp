// ====================================================
// Utilities & Inclues
// ====================================================

#include <stdint.h>

// Third-Party
#include "../../void.h"
#include "../../third_party/ntext.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <windowsx.h>

#define D3D11_NO_HELPERS
#include <d3d11.h>
#include <d3dcompiler.h>
#include <dxgi1_3.h>

#pragma comment (lib, "user32.lib")
#pragma comment (lib, "d3d11")
#pragma comment (lib, "dxgi")
#pragma comment (lib, "dxguid")
#pragma comment (lib, "d3dcompiler")

#define AlignPow2(x,b) (((x) + (b) - 1)&(~((b) - 1)))
#define KiB(n)         (((uint64_t)(n)) << 10)
#define MiB(n)         (((uint64_t)(n)) << 20)
#define GiB(n)         (((uint64_t)(n)) << 30)

struct memory_arena
{
    memory_arena *Prev;
    memory_arena *Current;

    uint64_t      CommitSize;
    uint64_t      ReserveSize;
    uint64_t      Committed;
    uint64_t      Reserved;

    uint64_t      BasePosition;
    uint64_t      Position;

    const char   *AllocatedFromFile;
    uint32_t      AllocatedFromLine;
};

struct memory_arena_params
{
    uint64_t    ReserveSize       = KiB(64);
    uint64_t    CommitSize        = KiB(16);
    const char *AllocatedFromFile = __FILE__;
    uint32_t    AllocatedFromLine = __LINE__;
};

static memory_arena *
AllocateArena(memory_arena_params Params)
{
    uint64_t ReserveSize = AlignPow2(Params.ReserveSize, KiB(4));
    uint64_t CommitSize  = AlignPow2(Params.CommitSize , KiB(4));

    if (CommitSize > ReserveSize)
    {
        CommitSize = ReserveSize;
    }

    void *HeapBase     = VirtualAlloc(0       , ReserveSize, MEM_RESERVE, PAGE_READWRITE);
    bool  CommitResult = VirtualAlloc(HeapBase, CommitSize , MEM_COMMIT , PAGE_READWRITE);
    if(!HeapBase || !CommitResult)
    {
        return 0;
    }

    memory_arena *Arena = (memory_arena*)HeapBase;
    Arena->Current           = Arena;
    Arena->CommitSize        = Params.CommitSize;
    Arena->ReserveSize       = Params.ReserveSize;
    Arena->Committed         = CommitSize;
    Arena->Reserved          = ReserveSize;
    Arena->BasePosition      = 0;
    Arena->Position          = sizeof(memory_arena);
    Arena->AllocatedFromFile = Params.AllocatedFromFile;
    Arena->AllocatedFromLine = Params.AllocatedFromLine;

    return Arena;
}

static void
ReleaseArena(memory_arena *Arena)
{
    memory_arena *Prev = 0;
    for (memory_arena *Node = Arena->Current; Node != 0; Node = Prev)
    {
        Prev = Node->Prev;
        VirtualFree(Node, 0, MEM_RELEASE);
    }
}

static void *
PushArena(memory_arena *Arena, uint64_t Size, uint64_t Alignment)
{
    memory_arena *Active       = Arena->Current;
    uint64_t      PrePosition  = AlignPow2(Active->Position, Alignment);
    uint64_t      PostPosition = PrePosition + Size;

    if(Active->Reserved < PostPosition)
    {
        memory_arena *New = 0;

        uint64_t ReserveSize = Active->ReserveSize;
        uint64_t CommitSize  = Active->CommitSize;
        if(Size + sizeof(memory_arena) > ReserveSize)
        {
            ReserveSize = AlignPow2(Size + sizeof(memory_arena), Alignment);
            CommitSize  = AlignPow2(Size + sizeof(memory_arena), Alignment);
        }

        memory_arena_params Params = {0};
        Params.CommitSize        = CommitSize;
        Params.ReserveSize       = ReserveSize;
        Params.AllocatedFromFile = Active->AllocatedFromFile;
        Params.AllocatedFromLine = Active->AllocatedFromLine;

        New = AllocateArena(Params);
        New->BasePosition = Active->BasePosition + Active->ReserveSize;
        New->Prev         = Active;

        Active         = New;
        Arena->Current = New;

        PrePosition  = AlignPow2(Active->Position, Alignment);
        PostPosition = PrePosition + Size;
    }

    if(Active->Committed < PostPosition)
    {
        uint64_t CommitPostAligned = PostPosition;
        AlignMultiple(CommitPostAligned, Active->CommitSize);

        uint64_t CommitPostClamped = ClampTop(CommitPostAligned, Active->Reserved);
        uint64_t CommitSize        = CommitPostClamped - Active->Committed;
        uint8_t *CommitPointer     = (uint8_t*)Active + Active->Committed;

        bool CommitResult = VirtualAlloc(CommitPointer, CommitSize, MEM_COMMIT, PAGE_READWRITE);
        if(!CommitResult)
        {
            return 0;
        }

        Active->Committed = CommitPostClamped;
    }

    void *Result = 0;
    if(Active->Committed >= PostPosition)
    {
        Result          = (uint8_t*)Active + PrePosition;
        Active->Position = PostPosition;
    }

    return Result;
}


static uint64_t
GetArenaPosition(memory_arena *Arena)
{
    memory_arena *Active = Arena->Current;

    uint64_t Result = Active->BasePosition + Active->Position;
    return Result;
}


static void
PopArenaTo(memory_arena *Arena, uint64_t Position)
{
    memory_arena *Active    = Arena->Current;
    uint64_t      PoppedPos = ClampBot(sizeof(memory_arena), Position);

    for(memory_arena *Prev = 0; Active->BasePosition >= PoppedPos; Active = Prev)
    {
        Prev = Active->Prev;
        VirtualFree(Active, 0, MEM_RELEASE);
    }

    Arena->Current           = Active;
    Arena->Current->Position = PoppedPos - Arena->Current->BasePosition;
}


static void
PopArena(memory_arena *Arena, uint64_t Amount)
{
    uint64_t OldPosition = GetArenaPosition(Arena);
    uint64_t NewPosition = OldPosition;

    if (Amount < OldPosition)
    {
        NewPosition = OldPosition - Amount;
    }

    PopArenaTo(Arena, NewPosition);
}


static void
ClearArena(memory_arena *Arena)
{
    PopArenaTo(Arena, 0);
    MemorySet((Arena + 1), 0, (Arena->Committed - Arena->Position));
}


template <typename T>
constexpr T* PushArrayNoZeroAligned(memory_arena* Arena, uint64_t Count, uint64_t Align)
{
    return static_cast<T*>(PushArena(Arena, sizeof(T) * Count, Align));
}

template <typename T>
constexpr T* PushArrayAligned(memory_arena* Arena, uint64_t Count, uint64_t Align)
{
    return PushArrayNoZeroAligned<T>(Arena, Count, Align);
}

template <typename T>
constexpr T* PushArray(memory_arena* Arena, uint64_t Count)
{
    return PushArrayAligned<T>(Arena, Count, max(8, alignof(T)));
}

template <typename T>
constexpr T* PushStruct(memory_arena* Arena)
{
    return PushArray<T>(Arena, 1);
}

// ====================================================
// Rendering
// ====================================================


enum class RenderPassType
{
    None = 0,

    UI   = 1,
};


struct render_handle
{
    uint64_t Value[0];
};


struct render_rect
{
    gui_bounding_box  RectBounds;
    gui_bounding_box  TextureSource;
    gui_color         ColorTL;
    gui_color         ColorBL;
    gui_color         ColorTR;
    gui_color         ColorBR;
    gui_corner_radius CornerRadius;
    float              BorderWidth;
    float              Softness;
    float              SampleTexture;
    float             _Padding0;
};


struct render_batch
{
    uint8_t *Memory;
    uint64_t ByteCount;
    uint64_t ByteCapacity;
};


struct render_batch_node
{
    render_batch_node *Next;
    render_batch       Value;
};


struct render_batch_list
{
    render_batch_node *First;
    render_batch_node *Last;

    uint64_t BatchCount;
    uint64_t ByteCount;
    uint64_t BytesPerInstance;
};


struct rect_group_params
{
    gui_bounding_box Clip;
};


struct rect_group_node
{
    rect_group_node  *Next;
    render_batch_list BatchList;
    rect_group_params Params;
};


struct render_pass_params_ui
{
    rect_group_node *First;
    rect_group_node *Last;
    uint32_t         Count;
};


struct render_pass
{
    RenderPassType Type;
    union
    {
        render_pass_params_ui UI;
    } Params;
};


struct render_pass_node
{
    render_pass_node *Next;
    render_pass       Value;
};


struct render_pass_list
{
    render_pass_node *First;
    render_pass_node *Last;
};


static void *
PushDataInBatchList(memory_arena *Arena, render_batch_list &BatchList)
{
    void *Result = 0;

    render_batch_node *Node = BatchList.Last;
    if (!Node || Node->Value.ByteCount + BatchList.BytesPerInstance > Node->Value.ByteCapacity)
    {
        Node = PushArray<render_batch_node>(Arena, 1);
        if(Node)
        {
            Node->Next               = 0;
            Node->Value.ByteCount    = 0;
            Node->Value.ByteCapacity = GUI_KILOBYTE(10);
            Node->Value.Memory       = PushArray<uint8_t>(Arena, Node->Value.ByteCapacity);

            if (!BatchList.First)
            {
                BatchList.First = Node;
                BatchList.Last  = Node;
            }
            else
            {
                BatchList.Last->Next = Node;
                BatchList.Last       = Node;
            }
        }
    }

    BatchList.ByteCount  += BatchList.BytesPerInstance;
    BatchList.BatchCount += 1;

    if(Node)
    {
        Result = Node->Value.Memory + Node->Value.ByteCount;

        Node->Value.ByteCount += BatchList.BytesPerInstance;
    }

    return Result;
}


// Unsafe Code!

static render_pass *
GetRenderPass(memory_arena *Arena, RenderPassType Type, render_pass_list &PassList)
{
    render_pass_node *Result = PassList.Last;

    if (!Result || Result->Value.Type != Type)
    {
        Result = PushStruct<render_pass_node>(Arena);
        Result->Value.Params.UI.First = 0;
        Result->Value.Params.UI.Last  = 0;
        Result->Value.Params.UI.Count = 0;
        Result->Value.Type = Type;

        if (!PassList.First)
        {
            PassList.First = Result;
        }

        if (PassList.Last)
        {
            PassList.Last->Next = Result;
        }

        PassList.Last = Result;
    }

    return &Result->Value;
}


static void
RenderUI(gui_layout_tree *Tree, memory_arena *Arena, render_pass_list &PassList)
{
    gui_memory_footprint Footprint = GuiGetRenderCommandsFootprint(Tree);
    gui_memory_block     Block     =
    {
        .SizeInBytes = Footprint.SizeInBytes,
        .Base        = PushArena(Arena, Footprint.SizeInBytes, Footprint.Alignment)
    };
        
    gui_render_command_list CommandList = GuiComputeRenderCommands(Tree, Block);

    render_pass *RenderPass = GetRenderPass(Arena, RenderPassType::UI, PassList);
    if(RenderPass)
    {
        render_pass_params_ui &UIParams    = RenderPass->Params.UI;
        rect_group_node       *GroupNode   = UIParams.Last;
        rect_group_params      GroupParams = {};

        if(!GroupNode)
        {
            GroupNode = PushStruct<rect_group_node>(Arena);
            GroupNode->BatchList.First            = nullptr;
            GroupNode->BatchList.Last             = nullptr;
            GroupNode->BatchList.BatchCount       = 0;
            GroupNode->BatchList.ByteCount        = 0;
            GroupNode->BatchList.BytesPerInstance = sizeof(render_rect);
            GroupNode->Params                     = GroupParams;

            if(!UIParams.First)
            {
                UIParams.First = GroupNode;
                UIParams.Last  = GroupNode;
            }
            else
            {
                UIParams.Last->Next = GroupNode;
                UIParams.Last       = GroupNode;
            }
        }

        render_batch_list &BatchList = GroupNode->BatchList;

        for(uint32_t Idx = 0; Idx < CommandList.Count; ++Idx)
        {
            const gui_render_command &Command = CommandList.Commands[Idx];

            switch(Command.Type)
            {

            case Gui_RenderCommandType_Rectangle:
            {
                auto *Rect = static_cast<render_rect *>(PushDataInBatchList(Arena, BatchList));
                Rect->RectBounds    = Command.Box;
                Rect->TextureSource = {};
                Rect->ColorTL       = Command.Rect.Color;
                Rect->ColorBL       = Command.Rect.Color;
                Rect->ColorTR       = Command.Rect.Color;
                Rect->ColorBR       = Command.Rect.Color;
                Rect->CornerRadius  = Command.Rect.CornerRadius;
                Rect->BorderWidth   = 0;
                Rect->Softness      = 2;
                Rect->SampleTexture = 0;
            } break;

            case Gui_RenderCommandType_Border:
            {
                auto *Rect = static_cast<render_rect *>(PushDataInBatchList(Arena, BatchList));
                Rect->RectBounds    = Command.Box;
                Rect->TextureSource = {};
                Rect->ColorTL       = Command.Border.Color;
                Rect->ColorBL       = Command.Border.Color;
                Rect->ColorTR       = Command.Border.Color;
                Rect->ColorBR       = Command.Border.Color;
                Rect->CornerRadius  = Command.Border.CornerRadius;
                Rect->BorderWidth   = Command.Border.Width;
                Rect->Softness      = 2;
                Rect->SampleTexture = 0;
            } break;

            case Gui_RenderCommandType_Text:
            {
                auto *Rect = static_cast<render_rect *>(PushDataInBatchList(Arena, BatchList));
                Rect->RectBounds    = Command.Box;
                Rect->TextureSource = {};
                Rect->ColorTL       = Command.Text.Color;
                Rect->ColorBL       = Command.Text.Color;
                Rect->ColorTR       = Command.Text.Color;
                Rect->ColorBR       = Command.Text.Color;
                Rect->CornerRadius  = {};
                Rect->BorderWidth   = 0;
                Rect->Softness      = 0;
                Rect->SampleTexture = 1;
            } break;

            default: break;

            }
        }
    }
}


// ====================================================
// D3D11 Renderer
// ====================================================

#include "d3d11_vertex_shader.h"
#include "d3d11_pixel_shader.h"

struct d3d11_renderer
{
    // Base Objects

    ID3D11Device           *Device;
    ID3D11DeviceContext    *DeviceContext;
    IDXGISwapChain1        *SwapChain;
    ID3D11RenderTargetView *RenderView;
    ID3D11BlendState       *BlendState;
    ID3D11SamplerState     *AtlasSamplerState;

    // UI Objects

    ID3D11InputLayout     *InputLayout;
    ID3D11VertexShader    *VtxShader;
    ID3D11PixelShader     *PxlShader;
    ID3D11Buffer          *UniformBuffer;
    ID3D11RasterizerState *RasterState;
    ID3D11Buffer          *VtxBuffer;

    // Frame Data

    render_pass_list  PassList;
    memory_arena     *FrameArena;
};


struct d3d11_rect_uniform_buffer
{
    float ScaleX   , _Padding0, _Padding1, _Padding2;
    float _Padding3, ScaleY   , _Padding4, _Padding5;
    float _Padding6, _Padding7, _Padding8, _Padding9;

    gui_dimensions ViewportSizeInPixel;
    gui_dimensions AtlasSizeInPixel;
};


struct d3d11_text_cache
{
    ID3D11Texture2D*          Cache;
    ID3D11ShaderResourceView* View;
    ID3D11Texture2D*          Transfer; 
};


static d3d11_renderer
D3D11Initialize(HWND HWindow)
{
    d3d11_renderer Result = {};
    Result.FrameArena = AllocateArena({});

    {
        UINT CreateFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
        #ifndef NDEBUG
        CreateFlags |= D3D11_CREATE_DEVICE_DEBUG;
        #endif
        D3D_FEATURE_LEVEL Levels[] = { D3D_FEATURE_LEVEL_11_0 };
        D3D11CreateDevice(NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
                          CreateFlags, Levels, ARRAYSIZE(Levels),
                          D3D11_SDK_VERSION, &Result.Device, NULL, &Result.DeviceContext);
    }

    {
        IDXGIDevice *DXGIDevice = nullptr;
        Result.Device->QueryInterface(__uuidof(IDXGIDevice), (void **)&DXGIDevice);
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

                    Factory->CreateSwapChainForHwnd((IUnknown *)Result.Device, Window, &Desc, 0, 0, &Result.SwapChain);
                    Factory->MakeWindowAssociation(Window, DXGI_MWA_NO_ALT_ENTER);
                    Factory->Release();
                }

                DXGIAdapter->Release();
            }

            DXGIDevice->Release();
        }
    }

    // Render Target View
    {
        ID3D11Texture2D *BackBuffer = nullptr;
        Result.SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void **)&BackBuffer);
        if(BackBuffer)
        {
            Result.Device->CreateRenderTargetView((ID3D11Resource*)BackBuffer, NULL, &Result.RenderView);

            BackBuffer->Release();
        }
    }

    // Default Buffers
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = KiB(64);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_VERTEX_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        Result.Device->CreateBuffer(&Desc, NULL, &Result.VtxBuffer);
    }

    // Default Shaders
    {
        Result.Device->CreateVertexShader(D3D11VtxShaderBytes, sizeof(D3D11VtxShaderBytes), 0, &Result.VtxShader);
        Result.Device->CreatePixelShader (D3D11PxlShaderBytes, sizeof(D3D11PxlShaderBytes), 0, &Result.PxlShader);

        D3D11_INPUT_ELEMENT_DESC InputLayout[] =
        {
            {"POS" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"FONT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"COL" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"COL" , 1, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"COL" , 2, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"COL" , 3, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"CORR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
            {"STY" , 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, D3D11_APPEND_ALIGNED_ELEMENT, D3D11_INPUT_PER_INSTANCE_DATA, 1},
        };

        Result.Device->CreateInputLayout(InputLayout, ARRAYSIZE(InputLayout), D3D11VtxShaderBytes, sizeof(D3D11VtxShaderBytes), &Result.InputLayout);
    }

    // Uniform Buffers
    {
        D3D11_BUFFER_DESC Desc = {};
        Desc.ByteWidth      = sizeof(d3d11_rect_uniform_buffer);
        Desc.Usage          = D3D11_USAGE_DYNAMIC;
        Desc.BindFlags      = D3D11_BIND_CONSTANT_BUFFER;
        Desc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;

        AlignMultiple(Desc.ByteWidth, 16);

        Result.Device->CreateBuffer(&Desc, 0, &Result.UniformBuffer);
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

        Result.Device->CreateBlendState(&BlendDesc, &Result.BlendState);
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

        Result.Device->CreateRasterizerState(&Desc, &Result.RasterState);
    }

    return Result;
}


static d3d11_text_cache 
D3D11CreateTextCache(uint32_t Width, uint32_t Height, d3d11_renderer *Renderer)
{
    d3d11_text_cache Result = {};

    D3D11_TEXTURE2D_DESC TextureDesc = {};
    TextureDesc.Width              = Width;
    TextureDesc.Height             = Height;
    TextureDesc.MipLevels          = 1;
    TextureDesc.ArraySize          = 1;
    TextureDesc.Format             = DXGI_FORMAT_R8G8B8A8_UNORM;
    TextureDesc.SampleDesc.Count   = 1;
    TextureDesc.SampleDesc.Quality = 0;
    TextureDesc.Usage              = D3D11_USAGE_DEFAULT;
    TextureDesc.BindFlags          = D3D11_BIND_SHADER_RESOURCE;
    TextureDesc.CPUAccessFlags     = 0;
    TextureDesc.MiscFlags          = 0;

    Renderer->Device->CreateTexture2D(&TextureDesc, nullptr, &Result. Cache);

    D3D11_SHADER_RESOURCE_VIEW_DESC SRVDesc = {};
    SRVDesc.Format                    = TextureDesc.Format;
    SRVDesc.ViewDimension             = D3D11_SRV_DIMENSION_TEXTURE2D;
    SRVDesc.Texture2D.MipLevels       = 1;
    SRVDesc.Texture2D.MostDetailedMip = 0;

    Renderer->Device->CreateShaderResourceView(Result.Cache, &SRVDesc, &Result.View);

    D3D11_TEXTURE2D_DESC StagingDesc = TextureDesc;
    StagingDesc.Usage          = D3D11_USAGE_STAGING;
    StagingDesc.BindFlags      = 0;
    StagingDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;

    Renderer->Device->CreateTexture2D(&StagingDesc, nullptr, &Result.Transfer);

    return Result;
}

// THIS CODE IS REALLY BAD AND EXPERIMENTAL !!!!

static void
D3D11UpdateTextCache(ntext::rasterized_glyph_list *List, d3d11_text_cache *TextCache, d3d11_renderer *Renderer)
{
    D3D11_MAPPED_SUBRESOURCE Mapped = {};
    Renderer->DeviceContext->Map(TextCache->Transfer, 0, D3D11_MAP_WRITE, 0, &Mapped);

    uint32_t AtlasBPP      = 4;
    uint8_t *AtlasBase     = (uint8_t *)Mapped.pData;
    uint32_t AtlasRowPitch = (uint32_t)Mapped.RowPitch;

    for (ntext::rasterized_glyph_node *Node = List->First; Node != 0; Node = Node->Next)
    {
        ntext::rasterized_glyph  &Glyph  = Node->Value;
        ntext::rasterized_buffer &Buffer = Glyph.Buffer;

        uint32_t DstLeft   = (uint32_t)Glyph.Source.Left;
        uint32_t DstTop    = (uint32_t)Glyph.Source.Top;
        uint32_t DstRight  = (uint32_t)Glyph.Source.Right;
        uint32_t DstBottom = (uint32_t)Glyph.Source.Bottom;

        uint32_t CopyWidth  = (DstRight  > DstLeft) ? (DstRight - DstLeft) : 0;
        uint32_t CopyHeight = (DstBottom > DstTop)  ? (DstBottom - DstTop) : 0;
        if (CopyWidth == 0 || CopyHeight == 0) continue;

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

    // Maybe heavy? Make sure to batch. Perhaps we accept a list of rasterized_glyphs.
    // NText doesn't really support that, but it's probably trivial to implement.

    Renderer->DeviceContext->Unmap(TextCache->Transfer, 0);
    Renderer->DeviceContext->CopyResource(TextCache->Cache, TextCache->Transfer);
}


static void
D3D11RenderFrame(d3d11_renderer &Renderer, float Width, float Height)
{
    const render_pass_list &PassList = Renderer.PassList;

    float ClearColor[4] = {0.f, 0.f, 0.f, 1.f};
    Renderer.DeviceContext->ClearRenderTargetView(Renderer.RenderView, ClearColor);

    for(render_pass_node *PassNode = PassList.First; PassNode != 0; PassNode = PassNode->Next)
    {
        render_pass Pass = PassNode->Value;

        switch(Pass.Type)
        {

        case RenderPassType::UI:
        {
            render_pass_params_ui Params = Pass.Params.UI;

            // Rasterizer
            D3D11_VIEWPORT Viewport = {0.f, 0.f, Width, Height, 0.f, 1.f};
            Renderer.DeviceContext->RSSetState(Renderer.RasterState);
            Renderer.DeviceContext->RSSetViewports(1, &Viewport);

            for(rect_group_node *RectNode = Params.First; RectNode != 0; RectNode = RectNode->Next)
            {
                render_batch_list BatchList  = RectNode->BatchList;
                rect_group_params RectParams = RectNode->Params;

                // Upload Data
                ID3D11Buffer *VtxBuffer = Renderer.VtxBuffer;
                {
                    D3D11_MAPPED_SUBRESOURCE Resource = {0};
                    Renderer.DeviceContext->Map(static_cast<ID3D11Resource *>(VtxBuffer), 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);

                    uint8_t *WritePointer = static_cast<uint8_t *>(Resource.pData);
                    uint64_t WriteOffset  = 0;

                    for(render_batch_node *BatchNode = BatchList.First; BatchNode != 0; BatchNode = BatchNode->Next)
                    {
                        render_batch Batch = BatchNode->Value;

                        uint64_t WriteSize = Batch.ByteCount;
                        MemoryCopy(WritePointer + WriteOffset, Batch.Memory, WriteSize);
                        WriteOffset += WriteSize;
                    }

                    Renderer.DeviceContext->Unmap(static_cast<ID3D11Resource *>(VtxBuffer), 0);
                }

                // Uniform Buffers
                ID3D11Buffer *UniformBuffer = Renderer.UniformBuffer;
                {
                    d3d11_rect_uniform_buffer Uniform = {};
                    Uniform.ScaleX              = 1.f;
                    Uniform.ScaleY              = 1.f;
                    Uniform.ViewportSizeInPixel = gui_dimensions(Width, Height);
                    Uniform.AtlasSizeInPixel    = gui_dimensions(0.f, 0.f);

                    D3D11_MAPPED_SUBRESOURCE Resource = {};
                    Renderer.DeviceContext->Map(static_cast<ID3D11Resource *>(UniformBuffer), 0, D3D11_MAP_WRITE_DISCARD, 0, &Resource);
                    MemoryCopy(Resource.pData, &Uniform, sizeof(Uniform));
                    Renderer.DeviceContext->Unmap(static_cast<ID3D11Resource *>(UniformBuffer), 0);
                }

                // Output Merger
                {
                    Renderer.DeviceContext->OMSetRenderTargets(1, &Renderer.RenderView, 0);
                    Renderer.DeviceContext->OMSetBlendState(Renderer.BlendState, 0, 0xFFFFFFFF);
                }

                // Input Assembler
                {
                    uint32_t Stride = BatchList.BytesPerInstance;
                    uint32_t Offset = 0;

                    Renderer.DeviceContext->IASetVertexBuffers(0, 1, &VtxBuffer, &Stride, &Offset);
                    Renderer.DeviceContext->IASetInputLayout(Renderer.InputLayout);
                    Renderer.DeviceContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
                }

                // Vertex Shader
                {
                    Renderer.DeviceContext->VSSetShader(Renderer.VtxShader, 0, 0);
                    Renderer.DeviceContext->VSSetConstantBuffers(0, 1, &UniformBuffer);
                }

                // Pixel Shader
                {
                    Renderer.DeviceContext->PSSetShader(Renderer.PxlShader, 0, 0);
                }

                // Scissor
                {
                    gui_bounding_box Clip = RectParams.Clip;
                    D3D11_RECT Rect = {};

                    if(Clip.Left == 0 && Clip.Top == 0 && Clip.Right == 0 && Clip.Bottom == 0)
                    {
                        Rect.left   = 0;
                        Rect.right  = Width;
                        Rect.top    = 0;
                        Rect.bottom = Height;
                    } else
                    if(Clip.Left > Clip.Right || Clip.Top > Clip.Bottom)
                    {
                        // No Op
                    }
                    else
                    {
                        Rect.left   = Clip.Left;
                        Rect.top    = Clip.Top;
                        Rect.right  = Clip.Right;
                        Rect.bottom = Clip.Bottom;
                    }

                    Renderer.DeviceContext->RSSetScissorRects(1, &Rect);
                }

                uint32_t InstanceCount = BatchList.ByteCount / BatchList.BytesPerInstance;
                Renderer.DeviceContext->DrawInstanced(4, InstanceCount, 0, 0);
            }

        } break;

        default: break;

        }
    }

    Renderer.SwapChain->Present(0, 0);

    Renderer.PassList.First = 0;
    Renderer.PassList.Last = 0;
}

// ====================================================
// Core UI Helpers
// ====================================================

// TODO: Text Experimentation

// ====================================================
// Inspector UI Logic
// ====================================================


struct inspector
{
    // Static State

    gui_layout_tree       *LayoutTree;
    gui_resource_table    *ResourceTable;

    ntext::glyph_generator GlyphGenerator;
    ntext::system_font     Font;
    ntext::backend_context Backend;

    d3d11_text_cache       TextCache;

    bool                   IsInitialized;
    memory_arena          *StateArena;

    // Frame State

    memory_arena          *FrameArena;
};


// constexpr gui_color MainOrange        = gui_color(0.9765f, 0.4510f, 0.0863f, 1.0f);
// constexpr gui_color WhiteOrange       = gui_color(1.0000f, 0.9686f, 0.9294f, 1.0f);
// constexpr gui_color SurfaceOrange     = gui_color(0.9961f, 0.8431f, 0.6667f, 1.0f);
constexpr gui_color HoverOrange       = gui_color(0.9176f, 0.3451f, 0.0471f, 1.0f);
// constexpr gui_color SubtleOrange      = gui_color(0.1686f, 0.1020f, 0.0627f, 1.0f);

constexpr gui_color Background        = gui_color(0.0588f, 0.0588f, 0.0627f, 1.0f);
constexpr gui_color SurfaceBackground = gui_color(0.1020f, 0.1098f, 0.1176f, 1.0f);
constexpr gui_color BorderOrDivider   = gui_color(0.1804f, 0.1961f, 0.2118f, 1.0f);

// constexpr gui_color TextPrimary       = gui_color(0.9765f, 0.9804f, 0.9843f, 1.0f);
// constexpr gui_color TextSecondary     = gui_color(0.6118f, 0.6392f, 0.6863f, 1.0f);

constexpr gui_color Success           = gui_color(0.1333f, 0.7725f, 0.3686f, 1.0f);
// constexpr gui_color Error             = gui_color(0.9373f, 0.2667f, 0.2667f, 1.0f);
// constexpr gui_color Warning           = gui_color(0.9608f, 0.6196f, 0.0431f, 1.0f);


static gui_cached_style WindowStyle =
{
    .Layout = 
    {
        .Size         = {gui_size({1000.f, Gui_LayoutSizing_Fixed}, {1000.f, Gui_LayoutSizing_Fixed})},
        .Direction    = {Gui_LayoutDirection_Horizontal},
        .XAlign       = {Gui_Alignment_Start},
        .YAlign       = {Gui_Alignment_Center},
        .Padding      = {gui_padding(20.f, 20.f, 20.f, 20.f)},
        .Spacing      = {10.f},
    },

    .Default =
    {
        .Color        = {Background},
        .BorderColor  = {BorderOrDivider},
        .BorderWidth  = {2.f},
        .Softness     = {2.f},
        .CornerRadius = {gui_corner_radius(4.f, 4.f, 4.f, 4.f)},
    },

    .Hovered =
    {
        .BorderColor = {HoverOrange},
    },

    .Focused =
    {
        .BorderColor = {Success},
    },
};


static gui_cached_style TreePanelStyle =
{
    .Layout = 
    {
        .Size         = {gui_size({50.f, Gui_LayoutSizing_Percent}, {100.f, Gui_LayoutSizing_Percent})},
        .Padding      = {gui_padding(20.f, 20.f, 20.f, 20.f)},
    },

    .Default =
    {
        .Color        = {SurfaceBackground},
        .BorderColor  = {BorderOrDivider},

        .BorderWidth  = {2.f},
        .Softness     = {2.f},
        .CornerRadius = {gui_corner_radius(4.f, 4.f, 4.f, 4.f)},
    },
};


// TODO: Make sure the initialization doesn't crash. Then continue fixing up the small patches to
// finally get something on screen.
static void
RenderInspectorUI(inspector *Inspector, d3d11_renderer *Renderer)
{
    if(!Inspector->IsInitialized)
    {
        Inspector->FrameArena = AllocateArena({});
        Inspector->StateArena = AllocateArena({.ReserveSize = KiB(512)});

        // Layout Tree
        {
            gui_memory_footprint Footprint = GuiGetLayoutTreeFootprint(128);
            gui_memory_block     Block     =
            {
                .SizeInBytes = Footprint.SizeInBytes,
                .Base        = PushArena(Inspector->StateArena, Footprint.SizeInBytes, Footprint.Alignment),
            };
            Inspector->LayoutTree = GuiPlaceLayoutTreeInMemory(128, Block);
        }

        // Resource Table
        {
            gui_resource_table_params Params =
            {
                .HashSlotCount = 64,
                .EntryCount    = 128,
            };

            gui_memory_footprint Footprint = GuiGetResourceTableFootprint(Params);
            gui_memory_block     Block     =
            {
                .SizeInBytes = Footprint.SizeInBytes,
                .Base        = PushArena(Inspector->StateArena, Footprint.SizeInBytes, Footprint.Alignment),
            };
            Inspector->ResourceTable = GuiPlaceResourceTableInMemory(Params, Block);
        }

        // Glyph Generator
        //{
        //    ntext::glyph_generator_params Params =
        //    {
        //        .TextStorage       = ntext::TextStorage::LazyAtlas,
        //        .FrameMemoryBudget = KiB(128),
        //        .FrameMemory       = PushArena(Inspector->FrameArena, KiB(64), 8),
        //        .CacheSizeX        = 1024,
        //        .CacheSizeY        = 1024,
        //    };

        //    Inspector->GlyphGenerator = ntext::CreateGlyphGenerator(Params);
        //}

        //// Fonts
        //{
        //    Inspector->Backend = ntext::InitializeBackendContext();
        //    Inspector->Font    = ntext::LoadSystemFont("Consolas", 16.f, Inspector->Backend);
        //}

        Inspector->IsInitialized = true;
    }

    uint32_t      WindowFlags = Gui_NodeFlags_ClipContent | Gui_NodeFlags_IsDraggable | Gui_NodeFlags_IsResizable;
    gui_component Window      = GuiCreateComponent(gui_str8_comp("window"), WindowFlags, &WindowStyle, Inspector->LayoutTree);

    if(GuiPushComponent(&Window, PushStruct<gui_parent_node>(Inspector->FrameArena)))
    {
        gui_component TreePanel = GuiCreateComponent(gui_str8_comp("tree_panel"), Gui_NodeFlags_None, &TreePanelStyle, Inspector->LayoutTree);
        if(GuiPushComponent(&TreePanel, PushStruct<gui_parent_node>(Inspector->FrameArena)))
        {
            // TODO: Text Experimentation.

            GuiPopComponent(&TreePanel);
        }

        gui_component OtherPanel = GuiCreateComponent(gui_str8_comp("other_panel"), Gui_NodeFlags_None, &TreePanelStyle, Inspector->LayoutTree);
        if(GuiPushComponent(&OtherPanel, PushStruct<gui_parent_node>(Inspector->FrameArena)))
        {
            GuiPopComponent(&OtherPanel);
        }
    }
    GuiPopComponent(&Window);

    GuiComputeTreeLayout(Inspector->LayoutTree);

    RenderUI(Inspector->LayoutTree, Renderer->FrameArena, Renderer->PassList);
}

// ====================================================
// Entry Point & Platform
// ====================================================


struct win32_state
{
    HWND                   WindowHandle;
    memory_arena          *FrameArena;
    gui_pointer_event_list PointerEventList;

    // Mouse

    int MouseX;
    int MouseY;
};

static win32_state Win32State;


LRESULT CALLBACK
WindowProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    gui_pointer_event_list *EventList = &Win32State.PointerEventList;

    switch(Message)
    {

    case WM_MOUSEMOVE:
    {
        float MouseX     = static_cast<float>(GET_X_LPARAM(LParam));
        float MouseY     = static_cast<float>(GET_Y_LPARAM(LParam));
        float LastMouseX = static_cast<float>(Win32State.MouseX);
        float LastMouseY = static_cast<float>(Win32State.MouseY);

        GuiPushPointerMoveEvent(gui_point(MouseX, MouseY), gui_point(LastMouseX, LastMouseY), PushStruct<gui_pointer_event_node>(Win32State.FrameArena), EventList);

        Win32State.MouseX = MouseX;
        Win32State.MouseY = MouseY;
    } break;

    case WM_LBUTTONDOWN:
    {
        float MouseX = static_cast<float>(GET_X_LPARAM(LParam));
        float MouseY = static_cast<float>(GET_Y_LPARAM(LParam));

        GuiPushPointerClickEvent(Gui_PointerButton_Primary, gui_point(MouseX, MouseY), PushStruct<gui_pointer_event_node>(Win32State.FrameArena), EventList);
    } break;

    case WM_LBUTTONUP:
    {
        float MouseX = static_cast<float>(GET_X_LPARAM(LParam));
        float MouseY = static_cast<float>(GET_Y_LPARAM(LParam));

        GuiPushPointerReleaseEvent(Gui_PointerButton_Primary, gui_point(MouseX, MouseY), PushStruct<gui_pointer_event_node>(Win32State.FrameArena), EventList);
    } break;

    case WM_DESTROY:
    {
        PostQuitMessage(0);
        return 0;
    } break;

    default: break;

    }

    return DefWindowProcA(Hwnd, Message, WParam, LParam);
}

int WINAPI
WinMain(HINSTANCE HInstance, HINSTANCE PrevInstance, LPSTR CmdLine, int CmdShow)
{
    WNDCLASSA WindowClass = {};
    WindowClass.hCursor       = LoadCursor(NULL, IDC_ARROW);
    WindowClass.lpfnWndProc   = WindowProc;
    WindowClass.hInstance     = HInstance;
    WindowClass.lpszClassName = "Debug Inspector";

    RegisterClassA(&WindowClass);

    Win32State.FrameArena   = AllocateArena({});
    Win32State.WindowHandle = CreateWindowExA(0, WindowClass.lpszClassName, "Minimal Window",
                                              WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                              1920, 1080, 0, 0, HInstance, 0);

    ShowWindow(Win32State.WindowHandle, CmdShow);

    BOOL           Running   = true;
    inspector      Inspector = {};
    d3d11_renderer Renderer  = D3D11Initialize(Win32State.WindowHandle);

    while (Running)
    {
        if (Inspector.FrameArena)
        {
            PopArenaTo(Inspector.FrameArena, 0);
        }

        if (Renderer.FrameArena)
        {
            PopArenaTo(Renderer.FrameArena, 0);
        }

        if(Win32State.FrameArena)
        {
            PopArenaTo(Win32State.FrameArena, 0);
        }

        MSG Message;
        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                Running = FALSE;
            }

            TranslateMessage(&Message);
            DispatchMessageA(&Message);
        }

        GuiBeginFrame(1901.f, 1041.f, &Win32State.PointerEventList, Inspector.LayoutTree);

        RenderInspectorUI(&Inspector, &Renderer);
        D3D11RenderFrame(Renderer, 1901.f, 1041.f);

        GuiClearPointerEvents(&Win32State.PointerEventList);
        GuiEndFrame();

        Sleep(10);
    }

    return 0;
}
