#pragma once

#include <string>
#include <map>

typedef std::map<std::string, ID3D10Texture2D *> TextureMap;
typedef std::map<std::string, ID3D10ShaderResourceView *> SRVMap;

/*
    This is just a cache from the scene containing all textures, and resource views for them.  
*/
class TextureLibrary
{
public:
    static TextureLibrary *singleton()
    {
        if (m_Singleton == NULL)
            m_Singleton = new TextureLibrary();
        return m_Singleton;
    }

    static void destroy()
    {
        delete m_Singleton;
        m_Singleton = NULL;
    }

    static HRESULT LoadTexSRV(WCHAR *strName, ID3D10Device *pDev10, ID3D10Texture2D **ppTexture10, ID3D10ShaderResourceView **ppSRV10);

    ID3D10Texture2D *GetTexture(std::string id);
    ID3D10Texture2D *GetTexture(int id);

    int GetTextureIndex(std::string id);

    ID3D10ShaderResourceView *GetShaderResourceView(std::string id);
    ID3D10ShaderResourceView *GetShaderResourceView(int id);

    int AddTexture(std::string name, ID3D10Texture2D *tex, ID3D10ShaderResourceView *srv);
    int NumTextures() { return (int)m_Textures.size(); }
    void Release();

    TextureMap m_Textures;
    SRVMap m_ShaderResourceViews;

private:
    TextureLibrary();
    static TextureLibrary *m_Singleton;
};