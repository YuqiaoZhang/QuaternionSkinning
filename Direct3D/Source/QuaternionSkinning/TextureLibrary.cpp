#include "DXUT.h"
#include "sdkmisc.h"
#include "TextureLibray.h"

TextureLibrary *TextureLibrary::m_Singleton = NULL;

TextureLibrary::TextureLibrary()
{
}

int TextureLibrary::AddTexture(std::string name,ID3D10Texture2D*tex,ID3D10ShaderResourceView *srv)
{
    int index = GetTextureIndex(name);
    if(index != -1) 
        return index;

    tex->AddRef();
    srv->AddRef();

    // all is good?  Then add to our list
    m_Textures.insert(TextureMap::value_type(name,tex));
    m_ShaderResourceViews.insert(SRVMap::value_type(name,srv));

    return GetTextureIndex(name);
}

ID3D10Texture2D *TextureLibrary::GetTexture(std::string id)
{
    for(TextureMap::iterator it = m_Textures.begin();it!=m_Textures.end();it++)
    {
        if(strcmp(it->first.c_str(),id.c_str()) == 0)
        {
            return it->second;
        }
    }
    return NULL;
}

int TextureLibrary::GetTextureIndex(std::string id)
{
    int i=0;
    for(TextureMap::iterator it = m_Textures.begin();it!=m_Textures.end();i++,it++)
    {
        if(strcmp(it->first.c_str(),id.c_str()) == 0)
        {
            return i;
        }
    }
    return -1;
}

ID3D10Texture2D *TextureLibrary::GetTexture(int id)
{
    if(id < 0) return NULL;
    int i=0;
    for(TextureMap::iterator it = m_Textures.begin();i!= id || it!=m_Textures.end();i++,it++)
    {
        if(i==id)
        {
            return it->second;
        }
    }
    return NULL;
}

ID3D10ShaderResourceView *TextureLibrary::GetShaderResourceView(std::string id)
{
    for(SRVMap::iterator it = m_ShaderResourceViews.begin();it!=m_ShaderResourceViews.end();it++)
    {
        if(strcmp(it->first.c_str(),id.c_str()) == 0)
        {
            return it->second;
        }
    }
    return NULL;
}

ID3D10ShaderResourceView *TextureLibrary::GetShaderResourceView(int id)
{
    if(id < 0) return NULL;
    int i=0;
    for(SRVMap::iterator it = m_ShaderResourceViews.begin();i!= id || it!=m_ShaderResourceViews.end();i++,it++)
    {
        if(i==id)
        {
            return it->second;
        }
    }
    return NULL;
}

void TextureLibrary::Release()
{
    for(TextureMap::iterator it = m_Textures.begin();it!= m_Textures.end();it++)
    {
        if(it->second)    SAFE_RELEASE(it->second);
    }
    m_Textures.clear();

    for(SRVMap::iterator it = m_ShaderResourceViews.begin();it!= m_ShaderResourceViews.end();it++)
    {
        if(it->second)    SAFE_RELEASE(it->second);
    }
    m_ShaderResourceViews.clear();
}

HRESULT TextureLibrary::LoadTexSRV(WCHAR *strName,ID3D10Device *pDev10,ID3D10Texture2D **ppTexture10, ID3D10ShaderResourceView **ppSRV10)
{
    HRESULT hr;
    WCHAR str[MAX_PATH];
    if( SUCCEEDED(DXUTFindDXSDKMediaFileCch( str, MAX_PATH, strName )) )
    {
        ID3D10Resource *pRes = NULL;
        hr = D3DX10CreateTextureFromFile( pDev10, str, NULL, NULL, &pRes, &hr );
        if( SUCCEEDED(hr) && pRes )
        {
            pRes->QueryInterface( __uuidof( ID3D10Texture2D ), (LPVOID*)ppTexture10 );
            D3D10_TEXTURE2D_DESC desc;
            (*ppTexture10)->GetDesc( &desc );
            D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            ZeroMemory( &SRVDesc, sizeof(SRVDesc) );
            SRVDesc.Format = desc.Format;
            if(desc.ArraySize == 6)
            {
                SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURECUBE;
                SRVDesc.TextureCube.MipLevels = desc.MipLevels;
            }
            else
            {
                SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
                SRVDesc.Texture2D.MipLevels = desc.MipLevels;
            }
            pDev10->CreateShaderResourceView( *ppTexture10, &SRVDesc, ppSRV10 );
            SAFE_RELEASE( pRes );
        }
    }
    else
    {
        return E_FAIL;
    }
    return S_OK;
}