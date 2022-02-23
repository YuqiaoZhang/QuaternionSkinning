#pragma once

#include "MultiAnimation.h"
#include "AnimatedCharacter.h"
#include "AnimatedCharacterInstance.h"
#include "CharacterAnimation.h"

/*
    This class bridges a gap.  Current state of DX10 des not support loading X files, and I have specific data format needs.
    this class makes use of classes from the DX9 MultiAnimation sample, and loads an X file with Mesh and Animations and populates the data in my classes.

    NOTE: This class is TIGHTLY bound the all classes involved.  And to the vertex layout used by the Army Manager.  
    In the interests of a simple demo, I opted not to spend the time, and the complexity to try some swiss army knife class that can handle arbitrary vertex topology, etc

    This class uses the DX9 mesh class to load an X file.  The x file can contain many meshes, each of these meshes are smooth bound to some subset of the skeleton.  
    This class loads each mesh, registers the texture for it, loads the bones it is attached to, there is no redundant removal, 
    so if there are multiple meshes attached to the same bone, then we have multiple copies of each bone...

    The important bit of information to understand about this class is that it populates the

        TextureLibrary
        AnimatedCharacter
        
    classes.  Specifically, it loads and meshes and m_animations from the X file.  It also loads textures that those meshes reference.  
    
    The animation data is in the form of matrix palettes.  In a game scenario you would have your own mesh, animation and texture loading code.  
    
*/

class DX9AnimationLoader
{
    friend class CAnimInstance;
    friend class CMultiAnim;
    friend struct MultiAnimFrame;
    friend struct MultiAnimMC;
    friend class CMultiAnimAllocateHierarchy;
    friend class AnimatedCharacter;
    friend class AnimatedCharacterInstance;
    friend struct CharacterBone;
    friend class CharacterSkeleton;
    friend class CharacterAnimation;

public:
    // Input/intermediate structures
    WCHAR fullFilename[MAX_PATH];
    CMultiAnim *multiAnim;
    CMultiAnimAllocateHierarchy *AH;
    IDirect3D9 *pD3D9;
    IDirect3DDevice9 *pDev9;
    ID3D10Device *pDev10;
    CONST D3D10_INPUT_ELEMENT_DESC *playout;
    int cElements;

    std::vector<std::string> frameNames;
    std::vector<std::string> boneNames;
    std::vector<std::string> attachmentNames;
    std::vector<LPD3DXSKININFO> pSkinInfos;
    D3DXMATERIAL *pMats;
    std::vector<MultiAnimMC *> meshList;
    D3DXMATRIX *pBoneOffsetMatrices; // this is the bind pose matrices
    D3DXMATRIX **pBoneMatrixPtrs;    // pointers into the skeleton for each bone, this gets updates as animation continues

    //    AnimatedCharacter::Vertex *vertices;
    //    DWORD *indices;
    //    int numVertices;
    //    int numIndices;
    //    std::string material;

    // output vars
    AnimatedCharacter *pAnimatedCharacter;

    DX9AnimationLoader();
    HRESULT load(ID3D10Device *pDev10, LPWSTR filename, CONST D3D10_INPUT_ELEMENT_DESC *playout, int cElements, bool bLoadAnimations = true, float timeStep = 1 / 30.f, SKINNING_TYPE eSkinningType = ST_LINEAR);

    void prepareDevice();
    HRESULT loadDX9Data(LPWSTR filename);
    void fillMeshList(D3DXFRAME *pFrame, D3DXFRAME *pFrameParent, const CHAR *pLastNameFound); // recursive
    void generateDX10Buffer(CONST D3D10_INPUT_ELEMENT_DESC *playout, int cElements, bool bAddSkeleton = true);
    void extractSkeleton();
    void processAnimations(float timeStep, SKINNING_TYPE eSkinningType);
    void cleanup();

    void extractFrame(CharacterAnimation *pAnimation, SKINNING_TYPE eSkinningType);
    void UpdateFrames(MultiAnimFrame *pFrame, D3DXMATRIX *pmxBase);
    LPCSTR FindLastBoneParentOf(MultiAnimFrame *pFrameRoot, MultiAnimMC *pMeshContainer, LPCSTR pLastBoneName);
};