//--------------------------------------------------------------------------------------
// File:   QuaternionSkinning.fx 
// Author: Konstantin Kolchin. Note tha a big part of Bryan Dudash's SkinnedInstancing.fx
// is reused here.
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

#ifndef MATRIX_PALETTE_SIZE_DEFAULT
#define MATRIX_PALETTE_SIZE_DEFAULT 50
#endif

//--------------------------------------------------------------------------------------
// Global variables
//--------------------------------------------------------------------------------------

float4x4    g_mWorld : World;                  // World matrix for object
float4x4    g_mWorldViewProjection : WorldViewProjection;    // World * View * Projection matrix
float3      g_EyePos : CAMERAPOS;

cbuffer animationvars
{
    matrix    g_matrices[MATRIX_PALETTE_SIZE_DEFAULT];
    float2x4  g_dualquat[MATRIX_PALETTE_SIZE_DEFAULT];
    float3      g_instanceColor = float3(1,1,1);
}

cbuffer cLighting
{
    float3      g_lightPos = float3(-13.f, 25.0f, -48.0f);
    float       Diffuse = 3.39 ;
    float       Specular = 0.506;
    float       Rb = 0.252;
    float       Roughness = 0.072 ;
   
    float       AmbientScale = 0.08;
    float4      AmbiColor = float4(0.5,0.5,0.5,0);
    float       Bumps = 0.55;
};


//--------------------------------------------------------------------------------------
// RenderStates
//--------------------------------------------------------------------------------------
RasterizerState Wire
{
    FillMode=Wireframe;
    CullMode=None;
};

RasterizerState NoCull
{
    CullMode = None;
};

RasterizerState Cull
{
    CullMode = Back;
};

BlendState Opaque
{
    BlendEnable[0] = false;
};

DepthStencilState DisableDepthTestWrite
{
    DepthEnable = FALSE;
    DepthWriteMask = 0;
};

DepthStencilState EnableDepthTestWrite
{
    DepthEnable = TRUE;
    DepthWriteMask = ALL;
};


//--------------------------------------------------------------------------------------
// Texture samplers
//--------------------------------------------------------------------------------------
Texture2D g_txDiffuse       : DIFFUSE;
Texture2D g_txNormals       : NORMAL;
TextureCube g_txEnvMap      : ENVMAP;   // the environmap

Texture2D g_txAOPositions     : POSITIONS;    // the position of the geometry rendered to each screen pixel
Texture2D g_txAONormals     : NORMALS;    // the normal of the geometry rendered to each screen pixel
Texture2D g_txAOColors     : COLORS;    // the color of the geometry rendered to each screen pixel

SamplerState g_samLinear
{
    Filter = MIN_MAG_MIP_LINEAR;
    AddressU = Wrap;
    AddressV = Wrap;
};
SamplerState g_samAniso
{
    Filter = ANISOTROPIC;
    AddressU = Wrap;
    AddressV = Wrap;
};
SamplerState g_samPoint
{
    Filter = MIN_MAG_MIP_POINT;
    AddressU = Clamp;
    AddressV = Clamp;
};

//--------------------------------------------------------------------------------------
// Vertex shader output structure
//--------------------------------------------------------------------------------------

struct A_to_VS
{
    float4 vPos         : POSITION;
    float3 vNormal      : NORMAL;
    float3 vTangent     : TANGENT;
    float2 vTexCoord0   : TEXCOORD0;
    float4 vBones       : BONES;
    float4 vWeights     : WEIGHTS;
};

struct VS_to_PS
{
    float4 pos      : SV_Position;
    float3 color    : COLOR;
    float3 tex      : TEXTURE0;
    float3 norm     : NORMAL;
    float3 tangent  : TANGENT;
    float3 binorm   : BINORMAL;
    float3 worldPos : WORLDPOS;
};

struct VS_to_PS_FSQuad
{
    float4 pos      : SV_Position;
    float2 tex      : TEXTURE0;
};

VS_to_PS_FSQuad VSLine( uint VertexID : SV_VertexID )
{
    VS_to_PS_FSQuad output;
    
    float4 position;

    float isODD = VertexID & 0x1;
    position.y = 1 - 2 * isODD;

    position.x = 0;
    position.z = 0.5;
    position.w = 1.0;

    output.pos = position;
    output.tex = 0;

    return output;
}

float4 PSLine( VS_to_PS_FSQuad input ) : SV_Target
{
    return float4( 1, 1, 0, 1 );
}

VS_to_PS CharacterAnimatedVS( A_to_VS input )
{
    VS_to_PS output;
    float3 vNormalWorldSpace;
                
    matrix finalMatrix;
    finalMatrix = input.vWeights.x * g_matrices[input.vBones.x];
    finalMatrix += input.vWeights.y * g_matrices[input.vBones.y];
    finalMatrix += input.vWeights.z * g_matrices[input.vBones.z];
    finalMatrix += input.vWeights.w * g_matrices[input.vBones.w];    
    
    float4 vAnimatedPos = mul(float4(input.vPos.xyz,1),finalMatrix);
    float4 vAnimatedNormal = mul(float4(input.vNormal.xyz,0),finalMatrix);
    float4 vAnimatedTangent = mul(float4(input.vTangent.xyz,0),finalMatrix);
        
    // Transform the position from object space to homogeneous projection space
    output.pos = mul(vAnimatedPos, g_mWorldViewProjection);

    // Transform the normal from object space to world space    
    output.norm = normalize(mul(vAnimatedNormal.xyz, (float3x3)g_mWorld)); // normal to world space
    output.tangent = normalize(mul(vAnimatedTangent.xyz, (float3x3)g_mWorld));
    output.binorm = cross(output.norm,output.tangent);
    
    // Do the position too for lighting
    float4 vWorldAnimatedPos = mul(float4(vAnimatedPos.xyz,1), g_mWorld);
    output.worldPos = float4(vWorldAnimatedPos.xyz,1);
    output.tex.xy = float2(input.vTexCoord0.x,-input.vTexCoord0.y); 
    output.tex.z = 0;
    
    output.color = g_instanceColor;
    
    return output;    
}

// convert unit dual quaternion to rotation matrix
matrix UDQtoRM(float2x4 dual)
{
    matrix m ;
    float length = dot(dual[0], dual[0]);
    float x = dual[0].x, y = dual[0].y, z = dual[0].z, w = dual[0].w;
    float t1 = dual[1].x, t2 = dual[1].y, t3 = dual[1].z, t0 = dual[1].w;
        
    m[0][0] = w*w + x*x - y*y - z*z; 
    m[1][0] = 2*x*y - 2*w*z; 
    m[2][0] = 2*x*z + 2*w*y;
    m[0][1] = 2*x*y + 2*w*z; 
    m[1][1] = w*w + y*y - x*x - z*z; 
    m[2][1] = 2*y*z - 2*w*x; 
    m[0][2] = 2*x*z - 2*w*y; 
    m[1][2] = 2*y*z + 2*w*x; 
    m[2][2] = w*w + z*z - x*x - y*y;
    
    m[3][0] = -2*t0*x + 2*t1*w - 2*t2*z + 2*t3*y ;
    m[3][1] = -2*t0*y + 2*t1*z + 2*t2*w - 2*t3*x ;
    m[3][2] = -2*t0*z - 2*t1*y + 2*t2*x + 2*t3*w ;
    
    m[0][3] = 0.0 ;
    m[1][3] = 0.0 ;
    m[2][3] = 0.0 ;
    m[3][3] = 1.0 ;            
    
    m /= length ;
    
    return m ;      
} 

//--------------------------------------------------------------------------------------
// Vertex shader used for dual quaternion skinning of the mesh for immediate render
// The algorithm is borrowed from "Skinning by Dual Quaternions" by L. Kavan et al. 
// After blending the resulting dual quaternion is converted to a rigid transformation 
// matrix, and it is used for calculating the new vertex position. 
//--------------------------------------------------------------------------------------

VS_to_PS CharacterAnimatedVS_dq( A_to_VS input )
{
    VS_to_PS output;
    float3 vNormalWorldSpace;
                
    float2x4 finalDualquat = (float2x4)0;
    float2x4 m = g_dualquat[input.vBones.x];
    float4 dq0 = (float1x4)m ;     
    
    finalDualquat = input.vWeights.x * m ;

    m = g_dualquat[input.vBones.y] ;
    float4 dq = (float1x4)m ;   
    if (dot( dq0, dq ) < 0)        
      finalDualquat -= input.vWeights.y * m;
    else 
    finalDualquat += input.vWeights.y * m;        

        
    m = g_dualquat[input.vBones.z] ;
    dq = (float1x4)m ;          
    if (dot( dq0, dq ) < 0)        
      finalDualquat -= input.vWeights.z * m;
    else 
    finalDualquat += input.vWeights.z * m ;
            
            
    m = g_dualquat[input.vBones.w] ;
    dq = (float1x4)m ;              
    if (dot( dq0, dq ) < 0)        
      finalDualquat -= input.vWeights.w * m;
    else                
    finalDualquat += input.vWeights.w * m;    
                    
    matrix finalMatrix = UDQtoRM(finalDualquat) ; 
        
    float3 Pos = mul(float4(input.vPos.xyz,1), (float4x3)finalMatrix);
    float4 vAnimatedNormal = mul(float4(input.vNormal.xyz,0),finalMatrix);
    float4 vAnimatedTangent = mul(float4(input.vTangent.xyz,0),finalMatrix);
        
        
    // Transform the position from object space to homogeneous projection space
    float4 vAnimatedPos = float4(Pos, 1) ;
    output.pos = mul( vAnimatedPos, g_mWorldViewProjection);

    // Transform the normal from object space to world space    
    output.norm = normalize(mul(vAnimatedNormal.xyz, (float3x3)g_mWorld)); // normal to world space
    output.tangent = normalize(mul(vAnimatedTangent.xyz, (float3x3)g_mWorld));
    output.binorm = cross(output.norm,output.tangent);
    
    // Do the position too for lighting
    float4 vWorldAnimatedPos = mul(float4(vAnimatedPos.xyz,1), g_mWorld);
    output.worldPos = float4(vWorldAnimatedPos.xyz,1);
    output.tex.xy = float2(input.vTexCoord0.x,-input.vTexCoord0.y); 
    output.tex.z = 0;
    
    output.color = g_instanceColor;
    
    return output;    
}

//--------------------------------------------------------------------------------------
// Vertex shader used for the FAST dual quaternion skinning of the mesh for immediate render
// The algorithm is borrowed from the paper "Geometric Skinning with Dual Skinning" by L. Kavan et al.
// The new vertex position is computed directly from the dual quaternion obtained by blending,
// without calculating a rigid transformation matrix. Because of this, this function requires
// fewer instructions than CharacterAnimatedVS_dq(). 
//--------------------------------------------------------------------------------------
VS_to_PS CharacterAnimatedVS_fast( A_to_VS input )
{
    VS_to_PS output;
    float3 vNormalWorldSpace;
                
    float2x4 dual = (float2x4)0;
    float2x4 m = g_dualquat[input.vBones.x];
    float4 dq0 = (float1x4)m ;     
    
    dual = input.vWeights.x * m ;

    m = g_dualquat[input.vBones.y] ;
    float4 dq = (float1x4)m ;   
    if (dot( dq0, dq ) < 0)        
      dual -= input.vWeights.y * m;
    else 
    dual += input.vWeights.y * m;        

        
    m = g_dualquat[input.vBones.z] ;
    dq = (float1x4)m ;          
    if (dot( dq0, dq ) < 0)        
      dual -= input.vWeights.z * m;
    else 
    dual += input.vWeights.z * m ;
            
            
    m = g_dualquat[input.vBones.w] ;
    dq = (float1x4)m ;              
    if (dot( dq0, dq ) < 0)        
      dual -= input.vWeights.w * m;
    else                
    dual += input.vWeights.w * m;    
   
    float4 Pos;
    float3 Norm, Tan, position, translation ; 

    // fast dqs 
    float length = sqrt(dual[0].w * dual[0].w + dual[0].x * dual[0].x + dual[0].y * dual[0].y + dual[0].z * dual[0].z) ;
    dual = dual / length ; 
    position = input.vPos.xyz + 2.0 * cross(dual[0].xyz, cross(dual[0].xyz, input.vPos.xyz) + dual[0].w * input.vPos.xyz) ;
    translation = 2.0 * (dual[0].w * dual[1].xyz - dual[1].w * dual[0].xyz + cross(dual[0].xyz, dual[1].xyz)) ; 
    position += translation ; 
    
    Pos = float4(position,1);
    Norm = input.vNormal.xyz + 2.0 * cross(dual[0].xyz, cross(dual[0].xyz,input.vNormal.xyz) + dual[0].w * input.vNormal.xyz) ; 
    Tan = input.vTangent.xyz + 2.0 * cross(dual[0].xyz, cross(dual[0].xyz,input.vTangent.xyz) + dual[0].w * input.vTangent.xyz) ;
    float4 vAnimatedPos = Pos ; 
    float4 vAnimatedNormal = float4(Norm, 0) ; 
    float4 vAnimatedTangent = float4(Tan, 0) ;  
       
    // Transform the position from object space to homogeneous projection space
    output.pos = mul(vAnimatedPos, g_mWorldViewProjection);

    // Transform the normal from object space to world space    
    output.norm = normalize(mul(vAnimatedNormal.xyz, (float3x3)g_mWorld)); // normal to world space
    output.tangent = normalize(mul(vAnimatedTangent.xyz, (float3x3)g_mWorld));
    output.binorm = cross(output.norm,output.tangent);
    
    // Do the position too for lighting
    float4 vWorldAnimatedPos = mul(float4(vAnimatedPos.xyz,1), g_mWorld);
    output.worldPos = float4(vWorldAnimatedPos.xyz,1);
    output.tex.xy = float2(input.vTexCoord0.x,-input.vTexCoord0.y); 
    output.tex.z = 0;   
    output.color = g_instanceColor;
    
    return output;    
}

// Schlick's approximation to Fresnel formula
//--------------------------------------------------------------------------------------
float fresnel(float3 eyeVec, float3 normal, float R0)
{
    float kOneMinusEdotN  = 1-abs(dot(eyeVec, normal));    // note: abs() makes 2-sided materials work

    // raise to 5th power
    float result = kOneMinusEdotN * kOneMinusEdotN;
    result = result * result;
    result = result * kOneMinusEdotN;
    result = R0 + (1-R0) * result;

    return result;
}

// Kelemen and Szirmay-Kalos' approximation to Cook-Torrance model 
//--------------------------------------------------------------------------------------
float SimpleCookTorrance( float3 N, float3 L, float3 V, float m)
{
    float result = 0 ; 
    float LN = dot(L, N) ; 
    if (LN > 0.0) 
    {
        float3 h = L + V ; // unnormalized half vector 
        float3 H = normalize( h ) ; 
        float NH = dot ( N, H ) ; 
        NH = NH * NH ; 
        float PH = exp( (1. - 1. / NH) / (m * m) ) * 1.0 / (m * m * NH * NH) ; 
        float F = fresnel( V, H, 0.028) ; 
        float frSpec = max( PH * F / dot( h, h), 0.0) ; 
        result = LN * frSpec ;
    }
    return result ; 
}

//--------------------------------------------------------------------------------------

float4 CharacterPS( VS_to_PS input) : SV_Target
{
    float4 bumps = g_txNormals.Sample(g_samAniso,input.tex.xyz);    
    bumps = float4(2*bumps.xyz - float3(1,1,1),0);  
    float3 lightVec = normalize(g_lightPos - input.worldPos);    
    
    // lighting
    float3 Nn = normalize(input.norm);
    float3 Tn = normalize(input.tangent);
    float3 Bn = normalize(input.binorm);
    
    float3x3 TangentToWorld = float3x3(Tn,Bn,Nn);
    bumps.xy *= Bumps ; 
    float3 Nb = mul(bumps.xyz,TangentToWorld);
    Nb = normalize(Nb);
    
    float3 Vn = normalize(g_EyePos - input.worldPos.xyz);
    float3 Ln = normalize(g_lightPos - input.worldPos);    
    float3 Hn = normalize(Vn + Ln);

    float4 txcolor = g_txDiffuse.Sample(g_samLinear,input.tex.xyz) ;
    return txcolor * (Diffuse * max(dot(Ln,Nb), 0) + Specular * SimpleCookTorrance( Nb, Ln, Vn, Roughness )) + AmbientScale * AmbiColor ;
}

float Kd_t = 0.4f;
float4 AmbiColor_t = float4(0.9,0.9,0.9,0.9);

float WriteDepthValue( VS_to_PS input) : SV_Target
{
    return input.pos.z;
}

//--------------------------------------------------------------------------------------
// Renders scene to Render target using D3D10 Techniques
//--------------------------------------------------------------------------------------

technique10 RenderAnimation
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, CharacterAnimatedVS() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, CharacterPS() ) );
        SetDepthStencilState( EnableDepthTestWrite, 1 );
        SetBlendState(Opaque,float4(0,0,0,0),0xffffffff);
        SetRasterizerState(Cull);
    }
}

technique10 RenderAnimation_dq
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, CharacterAnimatedVS_dq() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, CharacterPS() ) );
        SetDepthStencilState( EnableDepthTestWrite, 1 );
        SetBlendState(Opaque,float4(0,0,0,0),0xffffffff);
        SetRasterizerState(Cull);
    }
}

technique10 RenderAnimation_fast
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, CharacterAnimatedVS_fast() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, CharacterPS() ) );
        SetDepthStencilState( EnableDepthTestWrite, 1 );
        SetBlendState(Opaque,float4(0,0,0,0),0xffffffff);
        SetRasterizerState(Cull);
    }
}

technique10 DrawLine
{
    pass P0
    {
        SetVertexShader( CompileShader( vs_4_0, VSLine() ) );
        SetGeometryShader( NULL );
        SetPixelShader( CompileShader( ps_4_0, PSLine() ) );
        
        SetDepthStencilState( DisableDepthTestWrite, 1 );
        SetBlendState(Opaque,float4(0,0,0,0),0xffffffff);
        SetRasterizerState(NoCull);
    }
}
