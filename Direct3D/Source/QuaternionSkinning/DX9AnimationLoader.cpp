#include "DXUT.h"
#include "SDKmisc.h"
#include "DX9AnimationLoader.h"
#include "TextureLibray.h"

/*
    PLEASE read the header for information on this class.  It is not meant to demonstrate DX10 features.
*/
DX9AnimationLoader::DX9AnimationLoader()
{
    multiAnim = NULL;
    AH = NULL;
    pMats = NULL;
    pAnimatedCharacter = NULL;
    pD3D9 = NULL;
    pDev9 = NULL;
    pDev10 = NULL;
    pBoneMatrixPtrs = NULL;
    pBoneOffsetMatrices = NULL;
}

HRESULT DX9AnimationLoader::load(ID3D10Device *pDev10, LPWSTR filename, CONST D3D10_INPUT_ELEMENT_DESC *playout, int cElements, bool bLoadAnimations, float timeStep, SKINNING_TYPE eSkinningType)
{
    HRESULT hr = S_OK;
    this->pDev10 = pDev10;
    this->pDev10->AddRef();

    prepareDevice();

    hr = loadDX9Data(filename);
    if (hr != S_OK)
        return hr;

    std::wstring searchPathF = fullFilename; // This member var is filled in in loadDX9Data() method, icky, yeah, once you get messy...
    std::string::size_type pos = searchPathF.find_last_of('\\') + 1;
    if (pos == 0)
        pos = searchPathF.find_last_of('/') + 1;
    std::wstring searchPath = searchPathF.substr(0, pos);

    std::wstring oldSearch = DXUTGetMediaSearchPath();
    V_RETURN(DXUTSetMediaSearchPath(searchPath.c_str()));

    // Effectively collapse the whole frame hierarchy to each mesh container (will subtract out bone parents combined matrix to get a relative transform to bone....
    D3DXMATRIX mIdentity;
    D3DXMatrixIdentity(&mIdentity);
    MultiAnimFrame *pFrame = NULL;
    UpdateFrames(pFrame ? (MultiAnimFrame *)pFrame : multiAnim->m_pFrameRoot, &mIdentity);

    fillMeshList((D3DXFRAME *)multiAnim->m_pFrameRoot, NULL, NULL);

    generateDX10Buffer(playout, cElements);

    if (bLoadAnimations && pSkinInfos.size() > 0)
    {
        extractSkeleton();

        processAnimations(timeStep, eSkinningType);
    }
    cleanup();
    this->pDev10->Release();

    V_RETURN(DXUTSetMediaSearchPath(oldSearch.c_str()));
    return hr;
}

void DX9AnimationLoader::prepareDevice()
{
    HRESULT hr;

    pD3D9 = Direct3DCreate9(D3D_SDK_VERSION);
    assert(pD3D9);

    D3DPRESENT_PARAMETERS pp;
    pp.BackBufferWidth = 320;
    pp.BackBufferHeight = 240;
    pp.BackBufferFormat = D3DFMT_X8R8G8B8;
    pp.BackBufferCount = 1;
    pp.MultiSampleType = D3DMULTISAMPLE_NONE;
    pp.MultiSampleQuality = 0;
    pp.SwapEffect = D3DSWAPEFFECT_DISCARD;
    pp.hDeviceWindow = GetShellWindow();
    pp.Windowed = true;
    pp.Flags = 0;
    pp.FullScreen_RefreshRateInHz = 0;
    pp.PresentationInterval = D3DPRESENT_INTERVAL_DEFAULT;
    pp.EnableAutoDepthStencil = false;

    hr = pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pDev9);
    if (FAILED(hr))
    {
        V(pD3D9->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_REF, NULL, D3DCREATE_SOFTWARE_VERTEXPROCESSING, &pp, &pDev9));
    }

    pAnimatedCharacter = new AnimatedCharacter();
    AH = new CMultiAnimAllocateHierarchy();
    multiAnim = new CMultiAnim();
}

HRESULT DX9AnimationLoader::loadDX9Data(LPWSTR filename)
{
    HRESULT hr;

    V(DXUTFindDXSDKMediaFileCch(fullFilename, MAX_PATH, filename));

    AH->SetMA(multiAnim);

    // This is taken from the MultiAnim DX9 sample
    V(multiAnim->Setup(pDev9, fullFilename, AH));

    return hr;
}

// simple recursive to extract all mesh containers.
void DX9AnimationLoader::fillMeshList(D3DXFRAME *pFrame, D3DXFRAME *pFrameParent, const CHAR *pLastNameFound)
{
    CHAR *pToChildLastName = NULL;

    if (pFrame->Name && pFrame->Name[0])
    {
        pToChildLastName = new CHAR[MAX_PATH];
        StringCchCopyA(pToChildLastName, strlen(pFrame->Name) + 1, pFrame->Name);
    }
    else if (pLastNameFound)
    {
        pToChildLastName = new CHAR[MAX_PATH];
        StringCchCopyA(pToChildLastName, strlen(pLastNameFound) + 1, pLastNameFound);
    }

    // if we have a mesh container, add it to the list
    if (pFrame->pMeshContainer && pFrame->pMeshContainer->MeshData.pMesh)
    {
        MultiAnimMC *pMCFrame = (MultiAnimMC *)pFrame->pMeshContainer;
        //if(!this->pSkinInfo && pMCFrame->pSkinInfo) this->pSkinInfo = pMCFrame->pSkinInfo;

        // Copy over parent's name if we have none of our own... push up till we find something...
        if ((!pMCFrame->Name || pMCFrame->Name[0] == 0))
        {
            delete[] pMCFrame->Name; // safe for NULL

            if (pToChildLastName)
            {
                DWORD dwLen = (DWORD)strlen(pToChildLastName) + 1;
                pMCFrame->Name = new CHAR[dwLen];
                if (pMCFrame->Name)
                    StringCchCopyA(pMCFrame->Name, dwLen, pToChildLastName);
            }
            else
            {
                pMCFrame->Name = new CHAR[strlen("<no_name_mesh>")];
                StringCchCopyA(pMCFrame->Name, strlen("<no_name_mesh>"), "<no_name_mesh>");
            }
        }
        meshList.insert(meshList.begin(), pMCFrame);
        //meshList.push_back(pMCFrame);
    }
    else
    {
        if (!pFrame->Name || !pFrame->Name[0])
        {
            CHAR *name = new CHAR[MAX_PATH];
#pragma warning(disable : 4311)
            sprintf_s(name, MAX_PATH, "<no_name_%d>", reinterpret_cast<unsigned int>(pFrame->Name));
#pragma warning(default : 4311)
            delete[] pFrame->Name;
            DWORD dwLen = (DWORD)strlen(name) + 1;
            pFrame->Name = new CHAR[dwLen];
            if (pFrame->Name)
                StringCchCopyA(pFrame->Name, dwLen, name);
            delete[] name;
        }
        frameNames.push_back(pFrame->Name);
    }

    // then process siblings and children
    if (pFrame->pFrameSibling)
        fillMeshList(pFrame->pFrameSibling, pFrameParent, pLastNameFound);

    // override last name found for our children only
    if (pFrame->pFrameFirstChild)
        fillMeshList(pFrame->pFrameFirstChild, pFrame, pToChildLastName);

    delete[] pToChildLastName;
}

void AddTextureToPool(LPCSTR filename, ID3D10Device *pDev10)
{
    HRESULT hr;
    ID3D10Texture2D *pTexture10 = NULL;
    ID3D10ShaderResourceView *pSRV10 = NULL;

    WCHAR strName[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, filename, -1, strName, MAX_PATH);

    WCHAR str[MAX_PATH];
    if (SUCCEEDED(DXUTFindDXSDKMediaFileCch(str, MAX_PATH, strName)))
    {
        ID3D10Resource *pRes = NULL;
        hr = D3DX10CreateTextureFromFile(pDev10, str, NULL, NULL, &pRes, &hr);
        if (SUCCEEDED(hr) && pRes)
        {
            pRes->QueryInterface(__uuidof(ID3D10Texture2D), (LPVOID *)&pTexture10);
            D3D10_TEXTURE2D_DESC desc;
            pTexture10->GetDesc(&desc);
            D3D10_SHADER_RESOURCE_VIEW_DESC SRVDesc;
            ZeroMemory(&SRVDesc, sizeof(SRVDesc));
            SRVDesc.Format = desc.Format;
            SRVDesc.ViewDimension = D3D10_SRV_DIMENSION_TEXTURE2D;
            SRVDesc.Texture2D.MipLevels = desc.MipLevels;
            pDev10->CreateShaderResourceView(pTexture10, &SRVDesc, &pSRV10);
            SAFE_RELEASE(pRes);

            TextureLibrary::singleton()->AddTexture(filename, pTexture10, pSRV10);

            SAFE_RELEASE(pTexture10);
            SAFE_RELEASE(pSRV10);
        }
    }
}

void DX9AnimationLoader::generateDX10Buffer(CONST D3D10_INPUT_ELEMENT_DESC *playout, int cElements, bool bAddSkeleton)
{
    HRESULT hr;
    this->playout = playout;
    this->cElements = cElements;

    pAnimatedCharacter->numAttachments = (int)attachmentNames.size();

    // First pass adding all textures to fix in the ordering
    for (int iCurrentMeshContainer = 0; iCurrentMeshContainer < (int)meshList.size(); iCurrentMeshContainer++)
    {
        MultiAnimMC *pMeshContainer = (MultiAnimMC *)meshList[iCurrentMeshContainer];
        std::string name = pMeshContainer->m_pTextureFilename;
        std::string normalMap = "" + name;

        // Process material...  should only be one per mesh
        if (pMeshContainer->m_pTextureFilename && pMeshContainer->m_pTextureFilename[0] != 0)
        {
            // find a filename to load for base text
            std::string::size_type pos = name.find_last_of('\\') + 1;
            if (pos == 0)
                pos = name.find_last_of('/') + 1;
            name = name.substr(pos, name.length() - pos);

            // Now construct a normals filename implicitly... ugh.
            normalMap = name;
            pos = normalMap.find_last_of('_');
            if (pos == -1)
                pos = normalMap.find_last_of('.');
            normalMap = normalMap.substr(0, pos) + "_Normal.dds";

            // reassign to be the shortened version of the name
            strcpy_s(pMeshContainer->m_pTextureFilename, name.c_str());

            if (TextureLibrary::singleton()->GetTexture(name.c_str()) == NULL)
            {
                AddTextureToPool(name.c_str(), pDev10);
                AddTextureToPool(normalMap.c_str(), pDev10);
            }
        }
    }

    // Go thru all mesh containers and extract geometry into our buffer
    for (int iCurrentMeshContainer = 0; iCurrentMeshContainer < (int)meshList.size(); iCurrentMeshContainer++)
    {
        MultiAnimMC *pMeshContainer = (MultiAnimMC *)meshList[iCurrentMeshContainer];
        std::string textureName = pMeshContainer->m_pTextureFilename;
        std::string normalMap = "" + textureName;

        // find a filename to load for base text

        // reconstruct a normals filename implicitly... ugh.
        normalMap = textureName;
        std::string::size_type pos = normalMap.find_last_of('_');
        if (pos == -1)
            pos = normalMap.find_last_of('.');
        normalMap = normalMap.substr(0, pos) + "_Normal.dds";

        //int textureIndex = TextureLibrary::singleton()->GetTextureIndex(textureName);    // texture map index
        //int normalIndex = TextureLibrary::singleton()->GetTextureIndex(normalMap);    // texture map index

        // and now the mesh
        LPD3DXMESH pDX9Mesh = pMeshContainer->MeshData.pMesh;
        LPD3DXMESH pMesh = NULL;
        DWORD dwNumVerts = 0;
        DWORD dwNumIndices = 0;
        DWORD uStride = 0;

        // Now extract data info DX10
        D3DVERTEXELEMENT9 pdecl9[] =
            {
                {0, 0, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_POSITION, 0},
                {0, 12, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_NORMAL, 0},
                {0, 24, D3DDECLTYPE_FLOAT2, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TEXCOORD, 0},
                {0, 32, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_TANGENT, 0},
                {0, 44, D3DDECLTYPE_FLOAT3, D3DDECLMETHOD_DEFAULT, D3DDECLUSAGE_BINORMAL, 0},
                D3DDECL_END()};

        // Make a clone with the desired vertex format.
        if (SUCCEEDED(pDX9Mesh->CloneMesh(D3DXMESH_32BIT | D3DXMESH_DYNAMIC, pdecl9, pDev9, &pMesh)))
        {
            DWORD dwFlags = 0;
            dwFlags |= D3DXTANGENT_GENERATE_IN_PLACE;
            dwFlags |= D3DXTANGENT_DONT_NORMALIZE_PARTIALS;
            hr = D3DXComputeTangentFrameEx(pMesh,
                                           D3DDECLUSAGE_TEXCOORD,
                                           0,
                                           D3DDECLUSAGE_BINORMAL,
                                           0,
                                           D3DDECLUSAGE_TANGENT, 0,
                                           D3DDECLUSAGE_NORMAL, 0,
                                           dwFlags,
                                           NULL, 0.01f, 0.25f, 0.01f, NULL, NULL);

            DWORD *adjacency = new DWORD[3 * pMesh->GetNumFaces()];
            pMesh->GenerateAdjacency(0.01f, adjacency);
            //hr = D3DXComputeTangent(pMesh,D3DDECLUSAGE_TEXCOORD,D3DDECLUSAGE_TANGENT,0,0,adjacency);
            delete[] adjacency;
            if (FAILED(hr))
            {
                DXUTTRACE(L"Failed to compute tangents for the mesh.\n");
            }

            // get some basic data about the mesh
            dwNumVerts = pMesh->GetNumVertices();
            uStride = pMesh->GetNumBytesPerVertex();
            dwNumIndices = pMesh->GetNumFaces() * 3;

            //set the VB
            struct _LocalVertElement
            {
                D3DXVECTOR3 pos;
                D3DXVECTOR3 normal;
                D3DXVECTOR2 tex;
                D3DXVECTOR3 tangent;
                D3DXVECTOR3 binormal;
            };

            _LocalVertElement *sourceVerts = NULL;

            AnimatedCharacter::Vertex *vertices = new AnimatedCharacter::Vertex[dwNumVerts];
            pMesh->LockVertexBuffer(0, (void **)&sourceVerts);

            // scan for meshes that have been flagged as "attachments", these can be selectively killed.
            float attachmentValue = 0.f;

            if (pMeshContainer->Name)
            {
                std::string meshName = pMeshContainer->Name;
                std::string::size_type pos = meshName.find("attachment_");
                std::string::size_type bipPos = meshName.find("Bip01");
                if (bipPos != -1) // ignore the meshes on the skeleton...
                {
                    continue;
                }

                if (pos != -1)
                {
                    // strip off an "LOD#_" name...
                    std::string::size_type lodPos = meshName.find("LOD");
                    if (lodPos != -1)
                    {
                        std::string::size_type tPos = meshName.find_first_of("_");
                        if (tPos != -1)
                            meshName = meshName.substr(tPos + 1, meshName.length());
                    }

                    // find attachment if it already exists...
                    int attachIndex = -1;
                    for (int i = 0; i < (int)attachmentNames.size(); i++)
                    {
                        if (strcmp(attachmentNames[i].c_str(), meshName.c_str()) == 0)
                        {
                            attachIndex = i;
                            break;
                        }
                    }
                    if (attachIndex == -1)
                    {
                        attachIndex = (int)attachmentNames.size();
                        attachmentNames.push_back(meshName);
                        pAnimatedCharacter->numAttachments++;
                    }

                    // attachment flag(0 = base, >0 = bit flag for each attach)
                    attachmentValue = (float)(1 << (attachIndex));
                }
            }

            //D3DXMatrixIdentity(&pMeshContainer->m_pStaticCombinedMatrix);
            if (pMeshContainer->pSkinInfo)
            {
                if (bAddSkeleton)
                {
                    // Update our list of aggregate bones
                    pSkinInfos.push_back(pMeshContainer->pSkinInfo);
                }

                for (int iCurrentVert = 0; iCurrentVert < (int)dwNumVerts; iCurrentVert++)
                {
                    vertices[iCurrentVert].bones = D3DXVECTOR4(0, 0, 0, 0);
                    vertices[iCurrentVert].weights = D3DXVECTOR4(0, 0, 0, 0);
                }

                DWORD dwNumBones = pMeshContainer->pSkinInfo->GetNumBones();

                // Now go through the skin and set the bone indices and weights, max 4 per vert
                for (int iCurrentBone = 0; iCurrentBone < (int)dwNumBones; iCurrentBone++)
                {
                    LPCSTR boneName = pMeshContainer->pSkinInfo->GetBoneName(iCurrentBone);
                    int numInfl = pMeshContainer->pSkinInfo->GetNumBoneInfluences(iCurrentBone);
                    DWORD *verts = new DWORD[numInfl];
                    FLOAT *weights = new FLOAT[numInfl];
                    pMeshContainer->pSkinInfo->GetBoneInfluence(iCurrentBone, verts, weights);

                    // $ BED find bone index and update bones used array
                    int boneIndex = -1;
                    for (int i = 0; i < (int)boneNames.size(); i++)
                    {
                        if (strcmp(boneNames[i].c_str(), boneName) == 0)
                        {
                            boneIndex = i;
                            break;
                        }
                    }
                    if (boneIndex == -1)
                    {
                        if (bAddSkeleton)
                        {
                            boneIndex = (int)boneNames.size();
                            boneNames.push_back(boneName);
                        }
                        else
                        {
                            boneIndex = 0;
                            OutputDebugString(L"LOD Mesh Has bone not found in already loaded list!!!! This is not good.\n");
                        }
                    }

                    // find the top 4 influences

                    for (int iInfluence = 0; iInfluence < numInfl; iInfluence++)
                    {
                        if (weights[iInfluence] <= 0.05f)
                            continue;
                        if (vertices[verts[iInfluence]].weights.x <= 0.f)
                        {
                            vertices[verts[iInfluence]].bones.x = (float)boneIndex;
                            vertices[verts[iInfluence]].weights.x = weights[iInfluence];
                        }
                        else if (vertices[verts[iInfluence]].weights.y <= 0.f)
                        {
                            vertices[verts[iInfluence]].bones.y = (float)boneIndex;
                            vertices[verts[iInfluence]].weights.y = weights[iInfluence];
                        }
                        else if (vertices[verts[iInfluence]].weights.z <= 0.f)
                        {
                            vertices[verts[iInfluence]].bones.z = (float)boneIndex;
                            vertices[verts[iInfluence]].weights.z = weights[iInfluence];
                        }
                        else if (vertices[verts[iInfluence]].weights.w <= 0.f)
                        {
                            vertices[verts[iInfluence]].bones.w = (float)boneIndex;
                            vertices[verts[iInfluence]].weights.w = weights[iInfluence];
                        }
                    }
                    delete[] verts;
                    delete[] weights;
                }

                // if the mesh is skinned, then the skin info will have it's bone relative matrix...
                //D3DXMatrixIdentity(&pMeshContainer->m_pStaticCombinedMatrix);
            }
            // no skin info, the effectively parent this mesh with single bone weights if possible...
            // $$ This is a hack since is relies on Max naming for the bones... Bip*
            else
            {
                LPCSTR boneName = FindLastBoneParentOf(multiAnim->m_pFrameRoot, pMeshContainer, NULL);

                const CHAR szConstTemp[] = "Bip01";
                if (boneName == NULL)
                    boneName = szConstTemp;

                int boneIndex = -1;
                for (int i = 0; i < (int)boneNames.size(); i++)
                {
                    if (strcmp(boneNames[i].c_str(), boneName) == 0)
                    {
                        boneIndex = i;
                        break;
                    }
                }
                if (boneIndex == -1)
                {
                    if (bAddSkeleton)
                    {
                        boneIndex = (int)boneNames.size();
                        boneNames.push_back(boneName);
                    }
                    else
                    {
                        boneIndex = 0;
                        OutputDebugString(L"LOD Mesh Has bone not found in already loaded list!!!! This is not good.\n");
                    }
                }

                for (int iCurrentVert = 0; iCurrentVert < (int)dwNumVerts; iCurrentVert++)
                {
                    vertices[iCurrentVert].bones = D3DXVECTOR4((float)boneIndex, 0, 0, 0);
                    vertices[iCurrentVert].weights = D3DXVECTOR4(1.f, 0, 0, 0);
                }
            }

            // Now copy over verts, transforming them by the combined transform underneath the bone(might be identity)...
            //  This puts the mesh pieces in the right locations for the ref pose...
            for (int iCurrentVert = 0; iCurrentVert < (int)dwNumVerts; iCurrentVert++)
            {

                D3DXVec3TransformCoord(&vertices[iCurrentVert].position, &sourceVerts[iCurrentVert].pos, &pMeshContainer->m_pStaticCombinedMatrix);
                D3DXVec3TransformNormal(&vertices[iCurrentVert].normal, &sourceVerts[iCurrentVert].normal, &pMeshContainer->m_pStaticCombinedMatrix);
                vertices[iCurrentVert].uv.x = sourceVerts[iCurrentVert].tex.x;
                vertices[iCurrentVert].uv.y = -sourceVerts[iCurrentVert].tex.y;
                D3DXVec3TransformNormal(&vertices[iCurrentVert].tangent, &sourceVerts[iCurrentVert].tangent, &pMeshContainer->m_pStaticCombinedMatrix);
                //vertices[iCurrentVert].bones = D3DXVECTOR4(0,0,0,0);
                //vertices[iCurrentVert].weights = D3DXVECTOR4(0.f,0,0,0);
            }

            pMesh->UnlockVertexBuffer();

            //set the IB
            DWORD *pIndices = new DWORD[dwNumIndices];
            void *pData = NULL;
            pMesh->LockIndexBuffer(0, (void **)&pData);
            if (pMesh->GetOptions() & D3DXMESH_32BIT)
            {
                CopyMemory((void *)pIndices, pData, dwNumIndices * sizeof(DWORD));
            }
            else
            {
                WORD *pSmallIndices = (WORD *)pData;
                DWORD *pBigIndices = new DWORD[dwNumIndices];
                for (DWORD i = 0; i < dwNumIndices; i++)
                    pBigIndices[i] = pSmallIndices[i];
                CopyMemory((void *)pIndices, (void *)pBigIndices, dwNumIndices * sizeof(DWORD));
                SAFE_DELETE_ARRAY(pBigIndices);
            }
            pMesh->UnlockIndexBuffer();

            unsigned int hash = 1315423911;
            for (int i = 0; i < (int)strlen(pMeshContainer->Name); i++)
            {
                hash ^= ((hash << 5) + pMeshContainer->Name[i] + (hash >> 2));
            }

            if (attachmentValue == 0)
                hash = 0;

            // Add in separate buffers for the single Draw case
            pAnimatedCharacter->addSingleDrawMesh(pDev10, vertices, dwNumVerts, pIndices, dwNumIndices, (int)attachmentValue, hash, textureName, normalMap);

            // clean up local storage
            delete[] pIndices;
            delete[] vertices;

            // copy in our data pointers
            pAnimatedCharacter->boundingRadius = multiAnim->m_fBoundingRadius;
        }
        SAFE_RELEASE(pMesh);
    }
}

// walks tree and updates the transformations matrices to be combined
void DX9AnimationLoader::UpdateFrames(MultiAnimFrame *pFrame, D3DXMATRIX *pmxBase)
{
    assert(pFrame != NULL);
    assert(pmxBase != NULL);

    // transform the bone, aggregating in our parent matrix
    D3DXMatrixMultiply(&pFrame->CombinedTransformationMatrix,
                       &pFrame->TransformationMatrix,
                       pmxBase);

    // This should only be used when initializing the mesh pieces
    if (pFrame->pMeshContainer)
    {
        MultiAnimMC *pMCFrame = (MultiAnimMC *)pFrame->pMeshContainer;
        pMCFrame->m_pStaticCombinedMatrix = pFrame->CombinedTransformationMatrix;
    }

    // transform siblings by the same matrix
    if (pFrame->pFrameSibling)
        UpdateFrames((MultiAnimFrame *)pFrame->pFrameSibling, pmxBase);

    // transform children by the transformed matrix - hierarchical transformation
    if (pFrame->pFrameFirstChild)
        UpdateFrames((MultiAnimFrame *)pFrame->pFrameFirstChild,
                     &pFrame->CombinedTransformationMatrix);
}

LPCSTR DX9AnimationLoader::FindLastBoneParentOf(MultiAnimFrame *pFrameRoot, MultiAnimMC *pMeshContainer, LPCSTR pLastBoneName)
{
    if (!pFrameRoot)
        return NULL;

    LPCSTR pFound = NULL;

    // find the container in our siblings, but pass in parents bone name.  if match, pass that result
    if ((pFound = FindLastBoneParentOf((MultiAnimFrame *)pFrameRoot->pFrameSibling, pMeshContainer, pLastBoneName)) != NULL)
        return pFound;

    // Save our name to pass to children if we match
    if (pFrameRoot->Name && strncmp(pFrameRoot->Name, "Bip", 3) == 0)
        pLastBoneName = pFrameRoot->Name;

    // We match, so just return what we think is the last bone name.
    if (pFrameRoot->pMeshContainer == pMeshContainer)
        return pLastBoneName; // gotta return our name...

    // our child found it, pass that result.
    if ((pFound = FindLastBoneParentOf((MultiAnimFrame *)pFrameRoot->pFrameFirstChild, pMeshContainer, pLastBoneName)) != NULL)
        return pFound;

    return NULL; // nothing matched in children or siblings...
}

void DX9AnimationLoader::extractSkeleton()
{
    // Update the bone count
    pAnimatedCharacter->numBones = (int)boneNames.size();

    MultiAnimFrame *pFrame;
    pBoneMatrixPtrs = new D3DXMATRIX *[pAnimatedCharacter->numBones];
    pBoneOffsetMatrices = new D3DXMATRIX[pAnimatedCharacter->numBones];

    for (int i = 0; i < pAnimatedCharacter->numBones; i++)
    {
        D3DXMatrixIdentity(&pBoneOffsetMatrices[i]);
        pBoneMatrixPtrs[i] = NULL;
    }

    // pull out pointers to the bone matrices in the hierarchy, for each skin info, effectively merging the skeletons
    //
    // NOTE:  this loop will most certainly redundantly set the matrices and ptrs, but should set to same values
    //      and is only on load, so we conveniently ignore that fact... :/
    for (int iCurrentSI = 0; iCurrentSI < (int)pSkinInfos.size(); iCurrentSI++)
    {
        LPD3DXSKININFO pSkinInfo = pSkinInfos[iCurrentSI];
        DWORD dwNumBones = pSkinInfo->GetNumBones();

        // pull out pointers to the bone matrices in the hierarchy
        // Insert them into our list of bones, offset by the subskeleton
        for (DWORD iBone = 0; iBone < dwNumBones; iBone++)
        {
            // find the actual index in our local list...  this coalesces the palette
            LPCSTR boneName = pSkinInfo->GetBoneName(iBone);
            int boneIndex = -1;
            for (int iLocalBone = 0; iLocalBone < (int)boneNames.size(); iLocalBone++)
            {
                if (strcmp(boneNames[iLocalBone].c_str(), boneName) == 0)
                {
                    boneIndex = iLocalBone;
                    break;
                }
            }

            if (boneIndex != -1) // can only extract bones we have bound to verts...
            {
                pFrame = (MultiAnimFrame *)D3DXFrameFind(multiAnim->m_pFrameRoot, boneName);
                assert(pFrame);
                pBoneMatrixPtrs[boneIndex] = &pFrame->CombinedTransformationMatrix;
                pBoneOffsetMatrices[boneIndex] = *pSkinInfo->GetBoneOffsetMatrix(iBone);
            }
        }
    }

    // It shouldn't happen but is possible for a bone used but not in skin info, if that is the case we need to allocate a matrix or there will be a read of
    // uninitialized mem.
    for (int i = 0; i < pAnimatedCharacter->numBones; i++)
    {
        if (pBoneMatrixPtrs[i] == NULL)
        {
            pBoneMatrixPtrs[i] = new D3DXMATRIX();
            D3DXMatrixIdentity(pBoneMatrixPtrs[i]);
        }
    }
}

// convert unit quaternion and translation to unit dual quaternion
void UQTtoUDQ(D3DXVECTOR4 dual[2], D3DXQUATERNION quat, D3DXVECTOR3 tran)
{
    dual[0].x = quat.x;
    dual[0].y = quat.y;
    dual[0].z = quat.z;
    dual[0].w = quat.w;
    dual[1].x = 0.5f * (tran[0] * quat.w + tran[1] * quat.z - tran[2] * quat.y);
    dual[1].y = 0.5f * (-tran[0] * quat.z + tran[1] * quat.w + tran[2] * quat.x);
    dual[1].z = 0.5f * (tran[0] * quat.y - tran[1] * quat.x + tran[2] * quat.w);
    dual[1].w = -0.5f * (tran[0] * quat.x + tran[1] * quat.y + tran[2] * quat.z);
}
// convert unit dual quaternion to rotation and translation matrix
D3DXMATRIX UDQtoRMT(const D3DXVECTOR4 dual[2])
{
    D3DXMATRIX rt;
    float length = D3DXVec4Dot(&dual[0], &dual[0]);
    float x = dual[0].x, y = dual[0].y, z = dual[0].z, w = dual[0].w;
    float t1 = dual[1].x, t2 = dual[1].y, t3 = dual[1].z, t0 = dual[1].w;

    rt.m[0][0] = w * w + x * x - y * y - z * z;
    rt.m[1][0] = 2 * x * y - 2 * w * z;
    rt.m[2][0] = 2 * x * z + 2 * w * y;
    rt.m[0][1] = 2 * x * y + 2 * w * z;
    rt.m[1][1] = w * w + y * y - x * x - z * z;
    rt.m[2][1] = 2 * y * z - 2 * w * x;
    rt.m[0][2] = 2 * x * z - 2 * w * y;
    rt.m[1][2] = 2 * y * z + 2 * w * x;
    rt.m[2][2] = w * w + z * z - x * x - y * y;

    rt.m[3][0] = -2 * t0 * x + 2 * t1 * w - 2 * t2 * z + 2 * t3 * y;
    rt.m[3][1] = -2 * t0 * y + 2 * t1 * z + 2 * t2 * w - 2 * t3 * x;
    rt.m[3][2] = -2 * t0 * z - 2 * t1 * y + 2 * t2 * x + 2 * t3 * w;

    rt.m[0][3] = 0.0;
    rt.m[1][3] = 0.0;
    rt.m[2][3] = 0.0;
    rt.m[3][3] = 1.0;

    rt /= length;

    return rt;
}

void DX9AnimationLoader::extractFrame(CharacterAnimation *pAnimation, SKINNING_TYPE eSkinningType)
{
    // Push down the hierarchy
    D3DXMATRIX mIdentity;
    D3DXMatrixIdentity(&mIdentity);
    UpdateFrames(multiAnim->m_pFrameRoot, &mIdentity);

    D3DXMATRIX *pFrame = new D3DXMATRIX[pAnimatedCharacter->numBones];
    D3DXMATRIX *pQuatFrame = new D3DXMATRIX[pAnimatedCharacter->numBones];
    for (int i = 0; i < pAnimatedCharacter->numBones; i++)
    {
        D3DXMatrixMultiply(&(pFrame[i]), &(pBoneOffsetMatrices[i]), pBoneMatrixPtrs[i]);

        D3DXMATRIX rotationmat;
        D3DXQUATERNION quat;
        for (UINT j = 0; j < 3; j++)
            for (UINT k = 0; k < 3; k++)
                rotationmat.m[j][k] = pFrame[i].m[j][k];
        for (UINT j = 0; j < 3; j++)
            rotationmat.m[j][3] = 0.0;
        for (UINT k = 0; k < 3; k++)
            rotationmat.m[3][k] = 0.0;
        rotationmat.m[3][3] = 1.0;
        D3DXQuaternionRotationMatrix(&quat, &rotationmat);
        //D3DXQuaternionNormalize ( &quat, &quat ) ;
        //D3DXMatrixRotationQuaternion ( &rotationmat, &quat ) ;

        D3DXVECTOR4 dual[2];
        D3DXVECTOR3 translation;
        translation.x = pFrame[i].m[3][0];
        translation.y = pFrame[i].m[3][1];
        translation.z = pFrame[i].m[3][2];
        UQTtoUDQ(dual, quat, translation);

        // for testing
        //rotationmat = UDQtoRMT(dual) ;
        //for (UINT j = 0; j < 4; j++)
        //    for (UINT k = 0; k < 4; k++)
        //        pFrame[i].m[j][k] = rotationmat.m[j][k] ;

        OutputDebugString(L"Check pFrame!");
        D3DXMATRIX temp_Mat;
        for (UINT j = 0; j < 2; j++)
        {
            pQuatFrame[i].m[j][0] = dual[j].x;
            pQuatFrame[i].m[j][1] = dual[j].y;
            pQuatFrame[i].m[j][2] = dual[j].z;
            pQuatFrame[i].m[j][3] = dual[j].w;
        }

        OutputDebugString(L"Check pFrame!");
    }
    pAnimation->quatFrames.push_back(pQuatFrame);
    pAnimation->frames.push_back(pFrame);
}

// Go through each animation, and run through it once, and generate key frames based on a constant subdivision.
void DX9AnimationLoader::processAnimations(float timeStep, SKINNING_TYPE eSkinningType)
{
    LPD3DXANIMATIONCONTROLLER pAnimationController = multiAnim->m_pAC;

    if (pAnimationController == NULL)
        return;

    // Start with all tracks disabled
    UINT dwTracks = pAnimationController->GetMaxNumTracks();
    for (int i = 0; i < (int)dwTracks; ++i)
        pAnimationController->SetTrackEnable(i, FALSE);

    pAnimationController->SetTrackEnable(0, TRUE); // only need one as we aren't blending m_animations here.

    // Go through all animation sets, set them to a track, and play them through capturing frames... ugh
    int numAnimationSets = pAnimationController->GetNumAnimationSets();
    for (int iCurrentAnimationSet = 0; iCurrentAnimationSet < numAnimationSets; iCurrentAnimationSet++)
    {
        LPD3DXKEYFRAMEDANIMATIONSET pAnimationSet = NULL;
        CharacterAnimation *pCharacterAnimation = new CharacterAnimation();

        pAnimationController->GetAnimationSet(iCurrentAnimationSet, (LPD3DXANIMATIONSET *)&pAnimationSet);
        pAnimationController->SetTrackAnimationSet(0, pAnimationSet);
        pAnimationSet->Release();

        pAnimationController->ResetTime();
        pCharacterAnimation->duration = (float)pAnimationSet->GetPeriod();
        pCharacterAnimation->timeStep = timeStep;
        pCharacterAnimation->name = pAnimationSet->GetName();
        for (double dTime = 0.0; dTime < (double)pCharacterAnimation->duration; dTime += (double)pCharacterAnimation->timeStep)
        {
            pAnimationController->AdvanceTime((double)pCharacterAnimation->timeStep, NULL);
            extractFrame(pCharacterAnimation, eSkinningType);
        }
        pAnimatedCharacter->m_animations.push_back(pCharacterAnimation);
    }
}

void DX9AnimationLoader::cleanup()
{
    meshList.clear();
    multiAnim->Cleanup(AH);
    delete multiAnim;
    multiAnim = NULL;
    delete AH;
    AH = NULL;
    SAFE_DELETE_ARRAY(pMats);
    SAFE_DELETE_ARRAY(pBoneOffsetMatrices);
    SAFE_DELETE_ARRAY(pBoneMatrixPtrs);
    SAFE_RELEASE(pDev9);
    SAFE_RELEASE(pD3D9);

    playout = NULL;

    frameNames.clear();
    boneNames.clear();
    pSkinInfos.clear();

    // $$ Need to keep attachment names to provide ordering consistency amongst meshes.  Thus don;t clear them...
}