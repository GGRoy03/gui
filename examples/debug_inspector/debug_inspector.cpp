// ====================================================
// Utilities & Inclues
// ====================================================

#include <stdint.h>
#include "../../void.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

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
    matrix_3x3 Transform;
    rect_float Clip;
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
            Node->Value.ByteCount    = 0;
            Node->Value.ByteCapacity = VOID_KILOBYTE(10);
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


            BatchList.ByteCount  += BatchList.BytesPerInstance;
            BatchList.BatchCount += 1;
        }
    }

    if(Node)
    {
        Result = Node->Value.Memory + Node->Value.ByteCount;

        Node->Value.ByteCount += BatchList.BytesPerInstance;
    }

    return Result;
}

static render_pass *
GetRenderPass(memory_arena *Arena, RenderPassType Type, render_pass_list &BatchList)
{
    render_pass_node *Result = BatchList.Last;

    if (!Result || Result->Value.Type != Type)
    {
        Result = PushStruct<render_pass_node>(Arena);
        Result->Value.Type = Type;

        if (!BatchList.First)
        {
            BatchList.First = Result;
        }

        if (BatchList.Last)
        {
            BatchList.Last->Next = Result;
        }

        BatchList.Last = Result;
    }

    return &Result->Value;
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
    ID3D11BlendState       *DefaultBlendState;
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
    vec4_float Transform[3];
    vec2_float ViewportSizeInPixel;
    vec2_float AtlasSizeInPixel;
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

// ====================================================
// Core UI Logic
// ====================================================


struct inspector
{
    // UI

    layout::layout_tree *LayoutTree;

    // Static State

    bool          IsInitialized;
    memory_arena *StateArena;

    // Frame State

    memory_arena *FrameArena;
};


constexpr core::color MainOrange        = core::color(0.9765f, 0.4510f, 0.0863f, 1.0f);
constexpr core::color WhiteOrange       = core::color(1.0000f, 0.9686f, 0.9294f, 1.0f);
constexpr core::color SurfaceOrange     = core::color(0.9961f, 0.8431f, 0.6667f, 1.0f);
constexpr core::color HoverOrange       = core::color(0.9176f, 0.3451f, 0.0471f, 1.0f);
constexpr core::color SubtleOrange      = core::color(0.1686f, 0.1020f, 0.0627f, 1.0f);

constexpr core::color Background        = core::color(0.0588f, 0.0588f, 0.0627f, 1.0f);
constexpr core::color SurfaceBackground = core::color(0.1020f, 0.1098f, 0.1176f, 1.0f);
constexpr core::color BorderOrDivider   = core::color(0.1804f, 0.1961f, 0.2118f, 1.0f);

constexpr core::color TextPrimary       = core::color(0.9765f, 0.9804f, 0.9843f, 1.0f);
constexpr core::color TextSecondary     = core::color(0.6118f, 0.6392f, 0.6863f, 1.0f);

constexpr core::color Success           = core::color(0.1333f, 0.7725f, 0.3686f, 1.0f);
constexpr core::color Error             = core::color(0.9373f, 0.2667f, 0.2667f, 1.0f);
constexpr core::color Warning           = core::color(0.9608f, 0.6196f, 0.0431f, 1.0f);


static cached_style WindowStyle =
{
    .Default =
    {
        .Size         = size({1000.f, LayoutSizing::Fixed}, {1000.f, LayoutSizing::Fixed}),
        .Direction    = LayoutDirection::Horizontal,
        .XAlign       = Alignment::Start,
        .YAlign       = Alignment::Center,

        .Padding      = core::padding(20.f, 20.f, 20.f, 20.f),
        .Spacing      = 10.f,

        .Color        = Background,
        .BorderColor  = BorderOrDivider,

        .BorderWidth  = 2.f,
        .Softness     = 2.f,
        .CornerRadius = core::corner_radius(4.f, 4.f, 4.f, 4.f),
    },

    .Hovered =
    {
        .BorderColor = HoverOrange,
    },

    .Focused =
    {
        .BorderColor = Success,
    },
};


static cached_style TreePanelStyle =
{
    .Default =
    {
        .Size         = size({50.f, LayoutSizing::Percent}, {100.f, LayoutSizing::Percent}),

        .Padding      = core::padding(20.f, 20.f, 20.f, 20.f),

        .Color        = SurfaceBackground,
        .BorderColor  = BorderOrDivider,

        .BorderWidth  = 2.f,
        .Softness     = 2.f,
        .CornerRadius = core::corner_radius(4.f, 4.f, 4.f, 4.f),
    },
};


// When I look at this, I wonder if the component type is even useful?


static void
RenderInspectorUI(inspector &Inspector, d3d11_renderer &Renderer)
{
    if(!Inspector.IsInitialized)
    {
        Inspector.FrameArena  = AllocateArena({});
        Inspector.StateArena  = AllocateArena({});
        Inspector.LayoutTree  = layout::PlaceLayoutTreeInMemory(128, PushArena(Inspector.StateArena, layout::GetLayoutTreeFootprint(128), AlignOf(layout::layout_node)));

        Inspector.IsInitialized = true;
    }

    PopArenaTo(Inspector.FrameArena, 0);

    layout::NodeFlags WindowFlags = layout::NodeFlags::ClipContent |
                                    layout::NodeFlags::IsDraggable |
                                    layout::NodeFlags::IsResizable;

    core::component Window("window", WindowFlags, &WindowStyle, Inspector.LayoutTree);
    if(Window.Push(PushStruct<layout::parent_node>(Inspector.FrameArena)))
    {
        core::component TreePanel("tree_panel", layout::NodeFlags::None, &TreePanelStyle, Inspector.LayoutTree);
        if(TreePanel.Push(PushStruct<layout::parent_node>(Inspector.FrameArena)))
        {
            TreePanel.Pop();
        }

        core::component OtherPanel("other_panel", layout::NodeFlags::None, &TreePanelStyle, Inspector.LayoutTree);
        if(OtherPanel.Push(PushStruct<layout::parent_node>(Inspector.FrameArena)))
        {
            OtherPanel.Pop();
        }
    }
    Window.Pop();

    // Then we can do this stuff I guess? And ask for a draw list...
    // Which means we need a renderer, so let's do that. Alright.
    // ComputeTreeLayout(Inspector.LayoutTree);
}

// ====================================================
// Entry Point / UI Loop
// ====================================================


struct window_state
{
    HWND Handle;
    BOOL Running;
};

LRESULT CALLBACK
WindowProc(HWND Hwnd, UINT Message, WPARAM WParam, LPARAM LParam)
{
    if (Message == WM_DESTROY)
    {
        PostQuitMessage(0);
        return 0;
    }

    return DefWindowProcA(Hwnd, Message, WParam, LParam);
}

int WINAPI
WinMain(HINSTANCE HInstance, HINSTANCE PrevInstance, LPSTR CmdLine, int CmdShow)
{
    WNDCLASSA WindowClass = {};
    WindowClass.lpfnWndProc   = WindowProc;
    WindowClass.hInstance     = HInstance;
    WindowClass.lpszClassName = "Debug Inspector";

    RegisterClassA(&WindowClass);

    window_state WindowState = {};
    WindowState.Running = TRUE;

    WindowState.Handle = CreateWindowExA(0, WindowClass.lpszClassName, "Minimal Window",
                                         WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT,
                                         1920, 1080, 0, 0, HInstance, 0);

    ShowWindow(WindowState.Handle, CmdShow);

    inspector                Inspector = {};
    core::pointer_event_list EventList = {};
    d3d11_renderer           Renderer  = D3D11Initialize(WindowState.Handle);

    while (WindowState.Running)
    {
        MSG Message;
        while (PeekMessageA(&Message, 0, 0, 0, PM_REMOVE))
        {
            if (Message.message == WM_QUIT)
            {
                WindowState.Running = FALSE;
            }

            TranslateMessage(&Message);
            DispatchMessageA(&Message);
        }

        core::BeginFrame(1901.f, 1041.f, EventList, Inspector.LayoutTree);

        float Color[4] = {0.f, 0.f, 0.f, 1.f};
        Renderer.DeviceContext->ClearRenderTargetView(Renderer.RenderView, Color);

        RenderInspectorUI(Inspector, Renderer);

        Renderer.SwapChain->Present(1, 0);

        core::EndFrame();

        Sleep(5);
    }

    return 0;
}
