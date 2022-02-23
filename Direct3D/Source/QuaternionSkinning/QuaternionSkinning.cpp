//--------------------------------------------------------------------------------------
// File: QuaternionSkinning.cpp
// Demonstrates the dual quaternion vs. the standard ("linear-blend") skinning.
// Author: Konstantin Kolchin (big part of Bryan Dudash's SkinnedInstancing.cpp
// is reused here)
// Email:  sdkfeedback@nvidia.com
//
// Copyright (c) 2008 NVIDIA Corporation. All rights reserved.
//
// TO  THE MAXIMUM  EXTENT PERMITTED  BY APPLICABLE  LAW, THIS SOFTWARE  IS PROVIDED
// *AS IS*  AND NVIDIA AND  ITS SUPPLIERS DISCLAIM  ALL WARRANTIES,  EITHER  EXPRESS
// OR IMPLIED, INCLUDING, BUT NOT LIMITED  TO, IMPLIED WARRANTIES OF MERCHANTABILITY
// AND FITNESS FOR A PARTICULAR PURPOSE.  IN NO EVENT SHALL  NVIDIA OR ITS SUPPLIERS
// BE  LIABLE  FOR  ANY  SPECIAL,  INCIDENTAL,  INDIRECT,  OR  CONSEQUENTIAL DAMAGES
// WHATSOEVER (INCLUDING, WITHOUT LIMITATION,  DAMAGES FOR LOSS OF BUSINESS PROFITS,
// BUSINESS INTERRUPTION, LOSS OF BUSINESS INFORMATION, OR ANY OTHER PECUNIARY LOSS)
// ARISING OUT OF THE  USE OF OR INABILITY  TO USE THIS SOFTWARE, EVEN IF NVIDIA HAS
// BEEN ADVISED OF THE POSSIBILITY OF SUCH DAMAGES.
//
//
//----------------------------------------------------------------------------------
//--------------------------------------------------------------------------------------
#include "DXUT.h"

#include "DXUTcamera.h"
#include "DXUTgui.h"
#include "DXUTSettingsDlg.h"
#include "nvutmesh.h"

#include <vector>

#include "AnimatedCharacter.h"
#include "TextureLibray.h"
#include "DX9AnimationLoader.h"

#include "SDKmisc.h"

#include "CharacterAnimatedVS.hlsl.h"
#include "CharacterAnimatedVS_dq.hlsl.h"
#include "CharacterAnimatedVS_fast.hlsl.h"
#include "CharacterPS.hlsl.h"

#define INTERNAL_MAX_INSTANCES_PER_BUFFER 682

int g_NumInstances = 100;
int g_TargetLoad = 0;
float g_Time = 0.0f;
unsigned int g_Frame = 0;
unsigned int g_width, g_height;

D3D10_VIEWPORT g_Viewport;

CFirstPersonCamera g_Camera; // A model viewing camera
ID3DX10Font *g_pFont10 = NULL;
ID3DX10Sprite *g_pSprite10 = NULL;
CDXUTTextHelper *g_pTxtHelper = NULL;

CDXUTDialogResourceManager g_DialogResourceManager; // manager for shared resources of dialogs
CDXUTDialog g_SampleUI;                             // dialog for sample specific controls
CDXUTDialog g_LightingParamsUI;                     // dialog for sample specific controls
CDXUTDialog g_HUD;
CD3DSettingsDlg g_SettingsDlg;

AnimatedCharacter *g_ReferenceCharacter;
ID3D10Effect *g_pEffect10;
ID3D10EffectTechnique *g_pTechAnimationInstanced;
ID3D10EffectTechnique *g_pTechAnimation;
ID3D10VertexShader* g_pAnimationVS;
ID3D10EffectTechnique *g_pTechAnimation_dq;
ID3D10EffectTechnique *g_pTechAnimation_fast;

ID3D10EffectMatrixVariable *g_pWorldVar;
ID3D10EffectMatrixVariable *g_pWorldViewProjectionVar;
ID3D10EffectVectorVariable *g_pViewportSizeVar;
ID3D10EffectShaderResourceVariable *g_pAnimationsVar;
ID3D10EffectShaderResourceVariable *g_pDiffuseTexVar;
ID3D10EffectShaderResourceVariable *g_pNormalTexVar;
ID3D10EffectShaderResourceVariable *g_pEnvMapTexVar;
ID3D10InputLayout *g_pInputLayout;
ID3D10Texture2D *g_pAnimationTexture;
std::vector<AnimatedCharacterInstance *> g_Characters;

ID3D10RasterizerState *g_pRasterState; // Rasterizer states for non-FX
int g_MaxCharacters = 100;
int g_NumToDraw = 0;

bool g_bShowHelp = false;
bool g_bShowLighting = false;
bool g_bDumpFrame = false;
bool g_bNoUpdates = false;

#define IDC_TOGGLEFULLSCREEN 1
#define IDC_TOGGLEREF 2
#define IDC_CHANGEDEVICE 3

#define IDC_NUM_INSTANCES_STATIC 5
#define IDC_NUM_INSTANCES 6
#define IDC_SINGLEDRAW 7
#define IDC_HYBRID_INSTANCING 8
#define IDC_CPU_LOAD_STATIC 10
#define IDC_CPU_LOAD 11
#define IDC_ALLOWFREEDOM 13
#define IDC_NOUPDATES 14

#define IDC_AMBIENT 101
#define IDC_AMBIENT_STATIC 102
#define IDC_DIFFUSE 103
#define IDC_DIFFUSE_STATIC 104
#define IDC_SPECULAR 105
#define IDC_SPECULAR_STATIC 106
#define IDC_ROUGHNESS 109
#define IDC_ROUGHNESS_STATIC 110
#define IDC_BUMPS 115
#define IDC_BUMPS_STATIC 116

struct PerFrameUBO
{
    D3DXVECTOR3 g_EyePos;
    float _padding_EyePos;
    D3DXVECTOR3 g_lightPos;
    float _padding_lightPos;
    D3DXVECTOR3 AmbiColor;
    float _padding_AmbiColor;
    float Diffuse;
    float Specular;
    float Rb;
    float Roughness;
    float AmbientScale;
    float Bumps;
};

struct PerDrawcallUBO
{
    D3DXMATRIX g_mWorld;
    D3DXMATRIX g_mWorldViewProjection;
};

void InitGUI();
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl *pControl, void *pUserContext);
HRESULT SceneInitialize(ID3D10Device *pd3dDevice, int bbWidth, int bbHeight);
HRESULT SceneRender(ID3D10Device *pd3dDevice, D3DXMATRIX &mView, D3DXMATRIX &mProj, SKINNING_TYPE fSkinningType);
HRESULT DrawScene(ID3D10Device *pd3dDevice, D3DXMATRIX &mView, D3DXMATRIX &mProj, SKINNING_TYPE fSkinningType);
void SetNumCharacters(int num);
void Release();

//--------------------------------------------------------------------------------------
// Reject any D3D10 devices that aren't acceptable by returning false
//--------------------------------------------------------------------------------------
bool CALLBACK IsD3D10DeviceAcceptable(UINT Adapter, UINT Output, D3D10_DRIVER_TYPE DeviceType, DXGI_FORMAT BackBufferFormat, bool bWindowed, void *pUserContext)
{
    return true;
}

//--------------------------------------------------------------------------------------
// Called right before creating a D3D9 or D3D10 device, allowing the app to modify the device settings as needed
//--------------------------------------------------------------------------------------
bool CALLBACK ModifyDeviceSettings(DXUTDeviceSettings *pDeviceSettings, void *pUserContext)
{
#ifdef _DEBUG
    pDeviceSettings->d3d10.CreateFlags |= D3D10_CREATE_DEVICE_DEBUG;
#endif

    //pDeviceSettings->d3d10.SyncInterval = 0;
    pDeviceSettings->d3d10.sd.SampleDesc.Count = 1;
    pDeviceSettings->d3d10.sd.SampleDesc.Quality = 0;
    return true;
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that aren't dependant on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10CreateDevice(ID3D10Device *pd3dDevice, const DXGI_SURFACE_DESC *pBackBufferSurfaceDesc, void *pUserContext)
{
    InitGUI();

    HRESULT hr;
    V_RETURN(g_DialogResourceManager.OnD3D10CreateDevice(pd3dDevice));
    V_RETURN(g_SettingsDlg.OnD3D10CreateDevice(pd3dDevice));
    V_RETURN(D3DX10CreateFont(pd3dDevice, 15, 0, FW_BOLD, 1, FALSE, DEFAULT_CHARSET,
                              OUT_DEFAULT_PRECIS, DEFAULT_QUALITY, DEFAULT_PITCH | FF_DONTCARE,
                              L"Arial", &g_pFont10));
    V_RETURN(D3DX10CreateSprite(pd3dDevice, 512, &g_pSprite10));
    g_pTxtHelper = new CDXUTTextHelper(NULL, NULL, g_pFont10, g_pSprite10, 15);

    // Setup the camera's view parameters
    D3DXVECTOR3 vecEye(-13.f, 12.0f, -48.0f);
    D3DXVECTOR3 vecAt(-550.f, 12.0f, -500.0f);
    g_Camera.SetViewParams(&vecEye, &vecAt);
    g_Camera.SetEnablePositionMovement(true);
    g_Camera.SetEnableYAxisMovement(true);
    g_Camera.SetScalers(0.01f, 25.f);

    // Now load and process all that data create the d3d objects.
    V_RETURN(SceneInitialize(pd3dDevice, pBackBufferSurfaceDesc->Width, pBackBufferSurfaceDesc->Height));

    SetNumCharacters(g_NumInstances);

    // Now that we have made our manager, update lighting params
    g_LightingParamsUI.SendEvent(0, true, g_LightingParamsUI.GetControl(IDC_AMBIENT));
    g_LightingParamsUI.SendEvent(0, true, g_LightingParamsUI.GetControl(IDC_DIFFUSE));
    g_LightingParamsUI.SendEvent(0, true, g_LightingParamsUI.GetControl(IDC_SPECULAR));
    g_LightingParamsUI.SendEvent(0, true, g_LightingParamsUI.GetControl(IDC_ROUGHNESS));
    g_LightingParamsUI.SendEvent(0, true, g_LightingParamsUI.GetControl(IDC_BUMPS));

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Create any D3D10 resources that depend on the back buffer
//--------------------------------------------------------------------------------------
HRESULT CALLBACK OnD3D10ResizedSwapChain(ID3D10Device *pd3dDevice, IDXGISwapChain *pSwapChain, const DXGI_SURFACE_DESC *pBackBufferSurfaceDesc, void *pUserContext)
{
    HRESULT hr;
    V_RETURN(g_DialogResourceManager.OnD3D10ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));
    V_RETURN(g_SettingsDlg.OnD3D10ResizedSwapChain(pd3dDevice, pBackBufferSurfaceDesc));

    g_width = pBackBufferSurfaceDesc->Width;
    g_height = pBackBufferSurfaceDesc->Height;

    // Create a viewport to match the screen size
    // It will be split into parts before rendering
    g_Viewport.TopLeftX = 0;
    g_Viewport.TopLeftY = 0;
    g_Viewport.MinDepth = 0;
    g_Viewport.MaxDepth = 1;
    g_Viewport.Width = pBackBufferSurfaceDesc->Width;
    g_Viewport.Height = pBackBufferSurfaceDesc->Height;

    g_HUD.SetLocation(pBackBufferSurfaceDesc->Width - 170, 0);
    g_HUD.SetSize(170, 170);

    // Setup the camera's projection parameters
    // The 0.5 multiplier is needed because we split the viewport into two parts
    float fAspectRatio = 0.5f * pBackBufferSurfaceDesc->Width / (FLOAT)pBackBufferSurfaceDesc->Height;
    g_Camera.SetProjParams(D3DX_PI / 4, fAspectRatio, 0.1f, 2400.0f);

    g_SampleUI.SetLocation(pBackBufferSurfaceDesc->Width - 250, pBackBufferSurfaceDesc->Height - 200);
    g_SampleUI.SetSize(250, 300);

    g_LightingParamsUI.SetLocation(10, pBackBufferSurfaceDesc->Height - 400);
    g_LightingParamsUI.SetSize(250, 400);

    D3DXVECTOR4 v;
    v.x = (float)pBackBufferSurfaceDesc->Width;
    v.y = (float)pBackBufferSurfaceDesc->Height;
    v.z = 0;
    v.w = 0;
    g_pViewportSizeVar->SetFloatVector((float *)&v);

    return S_OK;
}

//--------------------------------------------------------------------------------------
// Handle updates to the scene.  This is called regardless of which D3D API is used
//--------------------------------------------------------------------------------------
void CALLBACK OnFrameMove(double fTime, float fElapsedTime, void *pUserContext)
{
    g_Camera.FrameMove(fElapsedTime);
    if (!g_bNoUpdates)
    {
        g_pEffect10->GetVariableByName("g_EyePos")->AsVector()->SetFloatVector((float *)g_Camera.GetEyePt());

        for (int i = 0; i < (int)g_Characters.size(); i++)
        {
            g_Characters[i]->Update(fElapsedTime, *g_Camera.GetEyePt(), *g_Camera.GetWorldAhead());
        }
    }

    // Simulate other game related load
    if (g_TargetLoad > 0)
    {
        DWORD start = GetTickCount();
        while (GetTickCount() - start < (DWORD)g_TargetLoad)
            ;
    }
}

//--------------------------------------------------------------------------------------
// Render the help and statistics text
//--------------------------------------------------------------------------------------
void RenderText()
{
    g_pTxtHelper->Begin();
    g_pTxtHelper->SetInsertionPos(2, 0);
    g_pTxtHelper->SetForegroundColor(D3DXCOLOR(1.0f, 1.0f, 0.0f, 1.0f));
    g_pTxtHelper->DrawTextLine(DXUTGetFrameStats(true));
    g_pTxtHelper->DrawTextLine(DXUTGetDeviceStats());
    if (g_bShowHelp)
    {
        g_pTxtHelper->DrawTextLine(L"Arrow and PgUp, PgDn (or 'q,' 'w,' 'e,' 'a,' 's,' 'd') keys move camera.");
        g_pTxtHelper->DrawTextLine(L"Press 'l' to access lighting parameters.");
        g_pTxtHelper->DrawTextLine(L"Mouse Left Click and Drag rotates camera.");
    }
    else
    {
        g_pTxtHelper->DrawTextLine(L"Hit F1 for Help.");
    }

    g_pTxtHelper->End();
}

//--------------------------------------------------------------------------------------
// Render the scene using the D3D10 device
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10FrameRender(ID3D10Device *pd3dDevice, double fTime, float fElapsedTime, void *pUserContext)
{
    // Clear Render target and the depth stencil
    float ClearColor[4] = {0.4f, 0.8f, 1.f, 0.0f};
    pd3dDevice->ClearRenderTargetView(DXUTGetD3D10RenderTargetView(), ClearColor);
    pd3dDevice->ClearDepthStencilView(DXUTGetD3D10DepthStencilView(), D3D10_CLEAR_DEPTH, 1.0, 0);

    D3DXMATRIX mView = *g_Camera.GetViewMatrix();
    D3DXMATRIX mProj = *g_Camera.GetProjMatrix();

    UINT tempWidth;
    tempWidth = g_Viewport.Width;

    g_Viewport.Width = tempWidth / 2;
    pd3dDevice->RSSetViewports(1, &g_Viewport);

    // This will set its own effect technique and everything....
    SceneRender(pd3dDevice, mView, mProj, ST_LINEAR);

    g_Viewport.TopLeftX = tempWidth / 2;
    pd3dDevice->RSSetViewports(1, &g_Viewport);

    // This will set its own effect technique and everything....
    SceneRender(pd3dDevice, mView, mProj, ST_FAST_DUALQUAT);

    g_Viewport.Width = tempWidth;
    g_Viewport.TopLeftX = 0;
    pd3dDevice->RSSetViewports(1, &g_Viewport);

    g_HUD.OnRender(fElapsedTime);
    if (g_SettingsDlg.IsActive())
    {
        g_SettingsDlg.OnRender(fElapsedTime);
        return;
    }

    if (g_bDumpFrame)
    {
        ID3D10Resource *pResource = NULL;
        DXUTGetD3D10RenderTargetView()->GetResource(&pResource);
        D3DX10SaveTextureToFile(pResource, D3DX10_IFF_DDS, L"Screenshot.png");
        SAFE_RELEASE(pResource);
        g_bDumpFrame = false;
        return;
    }

    RenderText();
    g_SampleUI.OnRender(fElapsedTime);
    if (g_bShowLighting)
        g_LightingParamsUI.OnRender(fElapsedTime);

    pd3dDevice->IASetInputLayout(NULL);
    pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_LINELIST);
    g_pEffect10->GetTechniqueByName("DrawLine")->GetPassByIndex(0)->Apply(0);
    pd3dDevice->Draw(2, 0);
}

//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10ResizedSwapChain
//------------------ --------------------------------------------------------------------
void CALLBACK OnD3D10ReleasingSwapChain(void *pUserContext)
{
    g_DialogResourceManager.OnD3D10ReleasingSwapChain();
}

//--------------------------------------------------------------------------------------
// Release D3D10 resources created in OnD3D10CreateDevice
//--------------------------------------------------------------------------------------
void CALLBACK OnD3D10DestroyDevice(void *pUserContext)
{
    g_SampleUI.RemoveAllControls();
    g_LightingParamsUI.RemoveAllControls();
    g_HUD.RemoveAllControls();
    g_SettingsDlg.GetDialogControl()->RemoveAllControls();
    g_SettingsDlg.OnD3D10DestroyDevice();

    g_DialogResourceManager.OnD3D10DestroyDevice();

    TextureLibrary::singleton()->Release();
    Release();

    TextureLibrary::destroy();
    SAFE_RELEASE(g_pFont10);
    SAFE_RELEASE(g_pSprite10);
    SAFE_DELETE(g_pTxtHelper);
}

//--------------------------------------------------------------------------------------
// Handle messages to the application
//--------------------------------------------------------------------------------------
LRESULT CALLBACK MsgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam,
                         bool *pbNoFurtherProcessing, void *pUserContext)
{

    if (g_SettingsDlg.IsActive())
    {
        g_SettingsDlg.MsgProc(hWnd, uMsg, wParam, lParam);
        return 0;
    }

    if (g_SampleUI.MsgProc(hWnd, uMsg, wParam, lParam))
    {
        return 1;
    }

    if (g_bShowLighting && g_LightingParamsUI.MsgProc(hWnd, uMsg, wParam, lParam))
    {
        return 1;
    }

    if (g_HUD.MsgProc(hWnd, uMsg, wParam, lParam))
    {
        return 1;
    }

    if (g_Camera.HandleMessages(hWnd, uMsg, wParam, lParam))
    {
        return 1;
    }

    if (uMsg == WM_KEYDOWN)
    {
        switch ((char)wParam)
        {
        case 'l':
        case 'L':
            g_bShowLighting = !g_bShowLighting;
            break;

        case 'k':
        case 'K':
            g_bDumpFrame = true;
            break;

        case VK_F1:
            g_bShowHelp = !g_bShowHelp;
            break;
        }
        return 1;
    }

    return 0;
}

//--------------------------------------------------------------------------------------
// Handle key presses
//--------------------------------------------------------------------------------------
void CALLBACK OnKeyboard(UINT nChar, bool bKeyDown, bool bAltDown, void *pUserContext)
{
}

//--------------------------------------------------------------------------------------
// Handle mouse button presses
//--------------------------------------------------------------------------------------
void CALLBACK OnMouse(bool bLeftButtonDown, bool bRightButtonDown, bool bMiddleButtonDown,
                      bool bSideButton1Down, bool bSideButton2Down, int nMouseWheelDelta,
                      int xPos, int yPos, void *pUserContext)
{
}

//--------------------------------------------------------------------------------------
// Call if device was removed.  Return true to find a new device, false to quit
//--------------------------------------------------------------------------------------
bool CALLBACK OnDeviceRemoved(void *pUserContext)
{
    return true;
}

//--------------------------------------------------------------------------------------
// Initialize everything and go into a Render loop
//--------------------------------------------------------------------------------------
int WINAPI wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPWSTR lpCmdLine, int nCmdShow)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    // DXUT will create and use the best device (either D3D9 or D3D10)
    // that is available on the system depending on which D3D callbacks are set below

    // Set general DXUT callbacks
    DXUTSetCallbackFrameMove(OnFrameMove);
    DXUTSetCallbackKeyboard(OnKeyboard);
    DXUTSetCallbackMouse(OnMouse);
    DXUTSetCallbackMsgProc(MsgProc);
    DXUTSetCallbackDeviceChanging(ModifyDeviceSettings);
    DXUTSetCallbackDeviceRemoved(OnDeviceRemoved);

    // Set the D3D10 DXUT callbacks. Remove these sets if the app doesn't need to support D3D10
    DXUTSetCallbackD3D10DeviceAcceptable(IsD3D10DeviceAcceptable);
    DXUTSetCallbackD3D10DeviceCreated(OnD3D10CreateDevice);
    DXUTSetCallbackD3D10SwapChainResized(OnD3D10ResizedSwapChain);
    DXUTSetCallbackD3D10FrameRender(OnD3D10FrameRender);
    DXUTSetCallbackD3D10SwapChainReleasing(OnD3D10ReleasingSwapChain);
    DXUTSetCallbackD3D10DeviceDestroyed(OnD3D10DestroyDevice);

    HRESULT hr;
    V_RETURN(DXUTSetMediaSearchPath(L"..\\Source\\QuaternionSkinning"));

    // Perform any application-level initialization here
    DXUTInit(true, true, NULL);        // Parse the command line, show msgboxes on error, no extra command line params
    DXUTSetCursorSettings(true, true); // Show the cursor and clip it when in full screen
    DXUTCreateWindow(L"QuaternionSkinning");
    DXUTCreateDevice(true, 1024, 768);
    DXUTMainLoop(); // Enter into the DXUT Render loop

    // Perform any application-level cleanup here

    return DXUTGetExitCode();
}

void InitGUI()
{
    int iX = 0;
    int iY = 0;
    g_SampleUI.Init(&g_DialogResourceManager);
    g_SampleUI.SetCallback(OnGUIEvent);

    WCHAR sCPULoad[MAX_PATH];
    wsprintf(sCPULoad, L"Reserved CPU(Non-Graphics): %d ms", g_TargetLoad);

#if 1
    WCHAR sNumInstances[MAX_PATH];
    wsprintf(sNumInstances, L"Instances: %d", g_NumInstances);
    g_SampleUI.AddStatic(IDC_NUM_INSTANCES_STATIC, sNumInstances, iX, iY, 220, 15);
    g_SampleUI.AddSlider(IDC_NUM_INSTANCES, iX, iY += 15, 220, 15, 1, 100, g_NumInstances);
#endif

    g_SampleUI.EnableNonUserEvents(false);

    g_LightingParamsUI.Init(&g_DialogResourceManager);
    g_LightingParamsUI.SetCallback(OnGUIEvent);
    iX = 0;
    iY = 0;
    g_LightingParamsUI.AddStatic(IDC_AMBIENT_STATIC, L"Ambient", iX, iY += 15, 220, 15);
    g_LightingParamsUI.AddSlider(IDC_AMBIENT, iX, iY += 15, 220, 15, 1, 500, 80);
    g_LightingParamsUI.AddStatic(IDC_DIFFUSE_STATIC, L"Diffuse", iX, iY += 15, 220, 15);
    g_LightingParamsUI.AddSlider(IDC_DIFFUSE, iX, iY += 15, 220, 15, 1, 3000, 1500);
    g_LightingParamsUI.AddStatic(IDC_SPECULAR_STATIC, L"Specular", iX, iY += 15, 220, 15);
    g_LightingParamsUI.AddSlider(IDC_SPECULAR, iX, iY += 15, 220, 15, 1, 3000, 1500);
    g_LightingParamsUI.AddStatic(IDC_ROUGHNESS_STATIC, L"Roughness", iX, iY += 15, 220, 15);
    g_LightingParamsUI.AddSlider(IDC_ROUGHNESS, iX, iY += 15, 220, 15, 20, 1000, 250);
    g_LightingParamsUI.AddStatic(IDC_BUMPS_STATIC, L"Bumps", iX, iY += 15, 220, 15);
    g_LightingParamsUI.AddSlider(IDC_BUMPS, iX, iY += 15, 220, 15, 1, 1500, 750);

    g_SettingsDlg.Init(&g_DialogResourceManager);
    g_HUD.Init(&g_DialogResourceManager);

    g_HUD.SetCallback(OnGUIEvent);
    iY = 10;
    g_HUD.AddButton(IDC_TOGGLEFULLSCREEN, L"Toggle full screen", 35, iY, 125, 22);
    g_HUD.AddButton(IDC_TOGGLEREF, L"Toggle REF (F3)", 35, iY += 24, 125, 22, VK_F3);
    g_HUD.AddButton(IDC_CHANGEDEVICE, L"Change device (F2)", 35, iY += 24, 125, 22, VK_F2);
}

//--------------------------------------------------------------------------------------
// Handles the GUI events
//--------------------------------------------------------------------------------------
void CALLBACK OnGUIEvent(UINT nEvent, int nControlID, CDXUTControl *pControl, void *pUserContext)
{
    switch (nControlID)
    {
    case IDC_TOGGLEFULLSCREEN:
        DXUTToggleFullScreen();
        break;
    case IDC_TOGGLEREF:
        DXUTToggleREF();
        break;
    case IDC_CHANGEDEVICE:
        g_SettingsDlg.SetActive(!g_SettingsDlg.IsActive());
        break;

    case IDC_NUM_INSTANCES:
    {
        g_NumInstances = g_SampleUI.GetSlider(IDC_NUM_INSTANCES)->GetValue();
        SetNumCharacters(g_NumInstances);

        WCHAR sNumInstances[MAX_PATH];
        wsprintf(sNumInstances, L"Instances: %d", g_NumInstances);
        g_SampleUI.GetStatic(IDC_NUM_INSTANCES_STATIC)->SetText(sNumInstances);
    }
    break;
    case IDC_CPU_LOAD:
    {
        g_TargetLoad = g_SampleUI.GetSlider(IDC_CPU_LOAD)->GetValue();

        WCHAR sCPULoad[MAX_PATH];
        wsprintf(sCPULoad, L"Reserved CPU(Non-Graphics): %d ms", g_TargetLoad);
        g_SampleUI.GetStatic(IDC_CPU_LOAD_STATIC)->SetText(sCPULoad);
    }
    break;

    case IDC_AMBIENT:
    {
        float value = (float)((CDXUTSlider *)pControl)->GetValue() / 1000.f;
        g_pEffect10->GetVariableByName("AmbientScale")->AsScalar()->SetFloat(value);
        WCHAR sText[MAX_PATH];
        wsprintf(sText, L"Ambient: %d ", (int)(value * 1000));
        g_LightingParamsUI.GetStatic(IDC_AMBIENT_STATIC)->SetText(sText);
    }
    break;
    case IDC_DIFFUSE:
    {
        float value = (float)((CDXUTSlider *)pControl)->GetValue() / 1000.f;
        g_pEffect10->GetVariableByName("Diffuse")->AsScalar()->SetFloat(value);
        WCHAR sText[MAX_PATH];
        wsprintf(sText, L"Diffuse: %d ", (int)(value * 1000));
        g_LightingParamsUI.GetStatic(IDC_DIFFUSE_STATIC)->SetText(sText);
    }
    break;
    case IDC_SPECULAR:
    {
        float value = (float)((CDXUTSlider *)pControl)->GetValue() / 100.f;
        g_pEffect10->GetVariableByName("Specular")->AsScalar()->SetFloat(value);
        WCHAR sText[MAX_PATH];
        wsprintf(sText, L"Specular: %d ", (int)(value * 100));
        g_LightingParamsUI.GetStatic(IDC_SPECULAR_STATIC)->SetText(sText);
    }
    break;
    case IDC_ROUGHNESS:
    {
        float value = (float)((CDXUTSlider *)pControl)->GetValue() / 1000.f;
        g_pEffect10->GetVariableByName("Roughness")->AsScalar()->SetFloat(value);
        WCHAR sText[MAX_PATH];
        wsprintf(sText, L"Roughness: %d ", (int)(value * 1000));
        g_LightingParamsUI.GetStatic(IDC_ROUGHNESS_STATIC)->SetText(sText);
    }
    break;
    case IDC_BUMPS:
    {
        float value = (float)((CDXUTSlider *)pControl)->GetValue() / 100.f;
        g_pEffect10->GetVariableByName("Bumps")->AsScalar()->SetFloat(value);
        WCHAR sText[MAX_PATH];
        wsprintf(sText, L"Bumps: %d ", (int)(value * 100));
        g_LightingParamsUI.GetStatic(IDC_BUMPS_STATIC)->SetText(sText);
    }
    break;
    }
}

HRESULT SceneInitialize(ID3D10Device *pd3dDevice, int bbWidth, int bbHeight)
{
    HRESULT hr;
    WCHAR str[MAX_PATH];

    D3D10_BUFFER_DESC UpdatedPerFrameDesc =
    {
        sizeof(struct UpdatedPerFrame),
         D3D10_USAGE_DYNAMIC,
         D3D10_BIND_CONSTANT_BUFFER,
         D3D10_CPU_ACCESS_WRITE,
    };
    V(device->CreateBuffer(&UpdatedPerFrameDesc, NULL, &CbufUpdatedPerFrame));

    const D3D10_INPUT_ELEMENT_DESC meshonlylayout[] =
        {
            // Normal character mesh data, setup for GPU skinning
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D10_INPUT_PER_VERTEX_DATA, 0},
            {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D10_INPUT_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D10_INPUT_PER_VERTEX_DATA, 0},
            {"TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D10_INPUT_PER_VERTEX_DATA, 0},
            {"BONES", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 44, D3D10_INPUT_PER_VERTEX_DATA, 0},
            {"WEIGHTS", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 60, D3D10_INPUT_PER_VERTEX_DATA, 0},
        };

    // First load meshes
    DX9AnimationLoader loader;
    V_RETURN(loader.load(pd3dDevice, L"Dwarves\\dwarf-lod0_rotating_hand.X", meshonlylayout, sizeof(meshonlylayout) / sizeof(D3D10_INPUT_ELEMENT_DESC), true, 1 / 30.f));
    V(loader.pAnimatedCharacter->Initialize(pd3dDevice));
    g_ReferenceCharacter = loader.pAnimatedCharacter;

    // the matrix palette
    LPCSTR sNumBones = new CHAR[MAX_PATH];
    sprintf_s((char *)sNumBones, MAX_PATH, "%d", max(1, g_ReferenceCharacter->numBones));
    LPCSTR sMaxInstanceConsts = new CHAR[MAX_PATH];
    sprintf_s((char *)sMaxInstanceConsts, MAX_PATH, "%d", INTERNAL_MAX_INSTANCES_PER_BUFFER);

    D3D10_SHADER_MACRO mac[2] =
        {
            {"MATRIX_PALETTE_SIZE_DEFAULT", sNumBones},
            {NULL, NULL}};

    // Read the D3DX effect file
    V_RETURN(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, L"QuaternionSkinning.hlsli"));
    V_RETURN(D3DX10CreateEffectFromFile(str, mac, NULL, "fx_4_0", D3D10_SHADER_ENABLE_STRICTNESS, 0, pd3dDevice, NULL, NULL, &g_pEffect10, NULL, &hr));

    delete[] sNumBones;
    delete[] sMaxInstanceConsts;

    // Grab some pointers to techniques
    g_pTechAnimation = g_pEffect10->GetTechniqueByName("RenderAnimation");
    g_pTechAnimation_dq = g_pEffect10->GetTechniqueByName("RenderAnimation_dq");
    g_pTechAnimation_fast = g_pEffect10->GetTechniqueByName("RenderAnimation_fast");

    V_RETURN(pd3dDevice->CreateVertexShader(g_CharacterAnimatedVS, sizeof(g_CharacterAnimatedVS), &g_pAnimationVS));

    // Grab some pointers to effects variables
    g_pWorldViewProjectionVar = g_pEffect10->GetVariableByName("g_mWorldViewProjection")->AsMatrix();
    g_pWorldVar = g_pEffect10->GetVariableByName("g_mWorld")->AsMatrix();
    g_pViewportSizeVar = g_pEffect10->GetVariableBySemantic("ViewportSizePixels")->AsVector();

    g_pAnimationsVar = g_pEffect10->GetVariableBySemantic("ANIMATIONS")->AsShaderResource();
    g_pDiffuseTexVar = g_pEffect10->GetVariableBySemantic("DIFFUSE")->AsShaderResource();
    g_pNormalTexVar = g_pEffect10->GetVariableBySemantic("NORMAL")->AsShaderResource();
    g_pEnvMapTexVar = g_pEffect10->GetVariableBySemantic("ENVMAP")->AsShaderResource();

    // Create our vertex input layouts
    V_RETURN(pd3dDevice->CreateInputLayout(meshonlylayout, sizeof(meshonlylayout) / sizeof(D3D10_INPUT_ELEMENT_DESC), g_CharacterAnimatedVS, sizeof(g_CharacterAnimatedVS), &g_pInputLayout));

    return S_OK;
}

HRESULT SceneRender(ID3D10Device *pd3dDevice, D3DXMATRIX &mView, D3DXMATRIX &mProj, SKINNING_TYPE fSkinningType)
{
    HRESULT hr;
    if (!g_Characters.size())
        return S_OK;

    pd3dDevice->IASetPrimitiveTopology(D3D10_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    V(DrawScene(pd3dDevice, mView, mProj, fSkinningType));

    return S_OK;
}

HRESULT DrawScene(ID3D10Device *pd3dDevice, D3DXMATRIX &mView, D3DXMATRIX &mProj, SKINNING_TYPE fSkinningType)
{
    pd3dDevice->IASetInputLayout(g_pInputLayout);

    // iterate over the # of charactes to draw
    int realNumToDraw = min((int)g_Characters.size(), g_NumToDraw); //(int)g_Characters.size() ;
    for (int currentCharacter = 0; currentCharacter < realNumToDraw; currentCharacter++)
    {
        AnimatedCharacterInstance *pCharacter = g_Characters[currentCharacter];

        D3DXMATRIX mWorldViewProj = pCharacter->mWorld * mView * mProj;
        g_pWorldViewProjectionVar->AsMatrix()->SetMatrix((float *)&mWorldViewProj);
        g_pWorldVar->SetMatrix((float *)&pCharacter->mWorld);

        // Grab the current frame of animation and set it to constants
        ID3D10EffectTechnique *pRenderTechnique = NULL;
        switch (fSkinningType)
        {
        case ST_LINEAR:
            pRenderTechnique = g_pTechAnimation;
            break;
        case ST_DUALQUAT:
            pRenderTechnique = g_pTechAnimation_dq;
            break;
        case ST_FAST_DUALQUAT:
            pRenderTechnique = g_pTechAnimation_fast;
            break;
        };

        if (g_ReferenceCharacter->m_animations.size())
        {
            CharacterAnimation *pAnimation = g_ReferenceCharacter->m_animations[pCharacter->animation];
            D3DXMATRIX *frameMatrices = NULL;
            switch (fSkinningType)
            {
            case ST_LINEAR:
            {
                frameMatrices = pAnimation->GetFrameAt(pCharacter->animationTime);
                g_pEffect10->GetVariableByName("g_matrices")->AsMatrix()->SetMatrixArray(*frameMatrices, 0, g_ReferenceCharacter->numBones);
            }
            break;
            case ST_DUALQUAT:
            case ST_FAST_DUALQUAT:
            {
                frameMatrices = pAnimation->GetQuatFrameAt(pCharacter->animationTime);
                g_pEffect10->GetVariableByName("g_dualquat")->AsMatrix()->SetMatrixArray(*frameMatrices, 0, g_ReferenceCharacter->numBones);
            }
            break;
            };
        }

        g_pEffect10->GetVariableByName("g_instanceColor")->AsVector()->SetFloatVector((float *)&pCharacter->color);

        AnimatedCharacter *referenceCharacter = g_ReferenceCharacter;

        // draw each submesh on each character, optionally skipping if we dont want to draw.
        std::string previousTexture = "null";
        for (int iSubMesh = 0; iSubMesh < (int)referenceCharacter->m_characterMeshes.size(); iSubMesh++)
        {
            // m_characterMeshes are the list of submeshes for the character.
            AnimatedCharacter::SubMesh &subMesh = referenceCharacter->m_characterMeshes[iSubMesh];

            // skip sub meshes if they don't match our submesh set. $$ Note this logic should match GS logic.
            //  if(subMesh.meshSet != 0 && !(subMesh.meshSet & pCharacter->meshSet)) continue;

            // Set our texture if we need to.
            if (previousTexture.compare(subMesh.texture) != 0)
            {
                g_pDiffuseTexVar->SetResource(TextureLibrary::singleton()->GetShaderResourceView(subMesh.texture));
                g_pNormalTexVar->SetResource(TextureLibrary::singleton()->GetShaderResourceView(subMesh.normal));
                previousTexture = subMesh.texture;
            }

            // Ensure no second buffer left over from instancing...
            UINT uiVertexStride = sizeof(AnimatedCharacter::Vertex);
            UINT uOffset = 0;

            ID3D10Buffer *vertBuffer = subMesh.pVB;
            ID3D10Buffer *indexBuffer = subMesh.pIB;

            pd3dDevice->IASetVertexBuffers(0, 1, &vertBuffer, &uiVertexStride, &uOffset);
            if (indexBuffer)
                pd3dDevice->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);

            // Apply the technique contained in the effect
            D3D10_TECHNIQUE_DESC techDesc;
            memset(&techDesc, 0, sizeof(D3D10_TECHNIQUE_DESC));
            pRenderTechnique->GetDesc(&techDesc);
            for (UINT iPass = 0; iPass < techDesc.Passes; iPass++)
            {
                ID3D10EffectPass *pCurrentPass = pRenderTechnique->GetPassByIndex(iPass);
                pCurrentPass->Apply(0);

                if (indexBuffer)
                    pd3dDevice->DrawIndexed(subMesh.numIndices, 0, 0);
                else
                    pd3dDevice->Draw(subMesh.numVertices, 0);
            }
        }
    }
    return S_OK;
}

void SetNumCharacters(int num)
{
    if (num > g_MaxCharacters)
        num = g_MaxCharacters;

    // assign the # we want to Draw
    g_NumToDraw = num;

    //  If the character object list is empty then we need to make our characters and initalize positions
    //  NOTE, this is pretty suboptimal design, however to handle the case where the max characters changes this is here
    //      if the max drawable characters changes, then the characters list is emptied and thus remade here.
    if (g_Characters.size() == 0)
    {
        D3DXMATRIX mScale;
        D3DXMATRIX mTranslation;
        D3DXMATRIX mRotation;
        D3DXMATRIX mWorld;
        AnimatedCharacterInstance *character = NULL;

        // a bunch of character instances. sort of fit to the arena...
        int charactersLeft = g_MaxCharacters - ((int)g_Characters.size());

        // Add some full LOD dead guys and a fighter
        D3DXMATRIX fPosition;

        D3DXMATRIX position, rotation, yrotation;
        int iSide = (UINT)sqrtf((float)charactersLeft);
        //D3DXMatrixRotationX( &rotation, -D3DX_PI * 0.5f ) ;
        D3DXMatrixRotationY(&rotation, -D3DX_PI * 0.75f);
        //D3DXMatrixMultiply( &rotation, &rotation, &yrotation ) ;

        for (int iInstance = 0; iInstance < charactersLeft; iInstance++)
        {
            int iX = iInstance % iSide;
            int iZ = iInstance / iSide;
            float xStart = -iSide * 35.25f;
            float zStart = -iSide * 35.25f;
            D3DXMatrixTranslation(&position, xStart + iX * 35.5f, 4.5f, zStart + iZ * 35.5f);
            D3DXMatrixMultiply(&position, &rotation, &position);

            character = new AnimatedCharacterInstance();
            character->MeshSet(g_ReferenceCharacter);
            character->Initialize(position);
            g_Characters.push_back(character);
        }
    }
}

void Release()
{
    if (g_ReferenceCharacter)
        g_ReferenceCharacter->Release();
    delete g_ReferenceCharacter;
    SAFE_RELEASE(g_pAnimationTexture);
    SAFE_RELEASE(g_pInputLayout);
    SAFE_RELEASE(g_pEffect10);

    // SAFE_RELEASE( g_pRasterState );

    while (g_Characters.size() > 0)
    {
        AnimatedCharacterInstance *c = g_Characters.back();
        delete c;
        g_Characters.pop_back();
    }
}
