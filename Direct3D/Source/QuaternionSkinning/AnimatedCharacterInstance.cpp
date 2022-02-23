#include "DXUT.h"
#include "AnimatedCharacterInstance.h"
#include "AnimatedCharacter.h"
#include "CharacterAnimation.h"
#include "nvutmesh.h"
//#include "ArmyManager.h"

AnimatedCharacterInstance::AnimatedCharacterInstance()
{
    iState = IDLE1;
    targetPosition = D3DXVECTOR3(0,0,0);
    m_ReferenceCharacter = NULL;
    color = D3DXVECTOR3(1,1,1);
    animationPlaybackRate = 1.f;
}

AnimatedCharacterInstance::~AnimatedCharacterInstance()
{

}

void AnimatedCharacterInstance::Initialize(D3DXVECTOR3 position,D3DXVECTOR3 rotation,D3DXVECTOR3 scale)
{
    assert(m_ReferenceCharacter);
    mPosition = position;
    mRotation = rotation;
    mScale = scale;
    animation = iState = IDLE1;
    this->animationTime = m_ReferenceCharacter->m_animations.size()?m_ReferenceCharacter->m_animations[animation]->duration * ((float)rand()/32768.0f):0;
    bOnlyWorldMatrix = false;
}

void AnimatedCharacterInstance::Initialize(D3DXMATRIX worldMatrix)
{
    mWorld = worldMatrix;
    bOnlyWorldMatrix = true;
    animation = iState ;
    this->animationTime = m_ReferenceCharacter->m_animations.size()?m_ReferenceCharacter->m_animations[animation]->duration * ((float)rand()/32768.0f):0;
}

static const D3DXVECTOR3 colors[] = 
{
    D3DXVECTOR3(1.f,1.f,1.f),
    D3DXVECTOR3(0.9f,0.9f,1.f),
    D3DXVECTOR3(1.f,0.9f,1.f),
    D3DXVECTOR3(1.f,0.9f,0.9f),
    D3DXVECTOR3(0.9f,0.9f,1.f),
    D3DXVECTOR3(0.9f,1.f,1.f),
    D3DXVECTOR3(0.9f,1.f,0.9f),
};

void AnimatedCharacterInstance::MeshSet(AnimatedCharacter *m_ReferenceCharacter)
{
    this->m_ReferenceCharacter = m_ReferenceCharacter;

    //meshSet = 0;

  /*  int heads[] = {19,20,12};
    int pads[] = {-1,3,5,4};
    int pads2[] = {-1,10,11,9};
    int shields[] = {-1,6,8,7};
    int weapons[] = {0,2,1};
    int torsos[] = {18,16,14};
    int legs[] = {17,15,13};

    meshSet = 0;

    int index = rand()%(sizeof(heads)/sizeof(int));
    index = heads[index];
    if(index != -1)
        meshSet |= 1<<index;

    index = rand()%(sizeof(pads)/sizeof(int));
    index = pads[index];
    if(index != -1)
        meshSet |= 1<<index;

    index = rand()%(sizeof(pads2)/sizeof(int));
    index = pads2[index];
    if(index != -1)
        meshSet |= 1<<index;

    index = rand()%(sizeof(shields)/sizeof(int));
    index = shields[index];
    if(index != -1)
        meshSet |= 1<<index;

    index = rand()%(sizeof(weapons)/sizeof(int));
    index = weapons[index];
    if(index != -1)
        meshSet |= 1<<index;

    index = rand()%(sizeof(torsos)/sizeof(int));
    index = torsos[index];
    if(index != -1)
        meshSet |= 1<<index;

    index = rand()%(sizeof(legs)/sizeof(int));
    index = legs[index];
    if(index != -1)
        meshSet |= 1<<index;*/

    // color = colors[rand() % (sizeof(colors)/sizeof(D3DXVECTOR3))];
    // baseColor = color;
}

void AnimatedCharacterInstance::SetState(AIState state)
{
    animationPlaybackRate = 1.f;
    iState = state;
    animation = GetAnimationForState(iState);
}

void AnimatedCharacterInstance::UpdateWorldMatrix()
{
    if(bOnlyWorldMatrix) return;

    D3DXMATRIX translation;
    D3DXMATRIX rotation;
    D3DXMATRIX scale;

    D3DXMatrixTranslation(&translation,mPosition.x,mPosition.y,mPosition.z);
    D3DXMatrixRotationYawPitchRoll(&rotation,mRotation.x,mRotation.y,mRotation.z);
    D3DXMatrixScaling(&scale,mScale.x,mScale.y,mScale.z);
    mWorld = scale * rotation * translation;
}

/*
    this is a hardcoded list of animations that we expect to be in the mesh file.  
    
    If it can't find the right animation for the state it is in, returns the first animation in the list.
*/
int AnimatedCharacterInstance::GetAnimationForState(AIState state)
{
    static const char *animationNames[] = 
    {
        "Dead3",
        "Dead2",
        "Dead1",
        "Idle2",
        "JumpCheer",
        "Idle1",
        "Cheer3",
        "Cheer2",
        "Cheer1"
    };

    for(int i=0;i<(int)m_ReferenceCharacter->m_animations.size();i++)
    {
        if(strcmp(animationNames[(int)state],m_ReferenceCharacter->m_animations[i]->name.c_str()) == 0)
        {
            return i;
        }
    }
    return 0;
}

/*
    just loop the animation we've chosen.
*/
void AnimatedCharacterInstance::Update(float deltatime, const D3DXVECTOR3 &cameraAt,const D3DXVECTOR3 &camDir)
{
    animationTime += animationPlaybackRate*deltatime;
    
    // only do AI if we have a base character to refer to
    if(m_ReferenceCharacter && m_ReferenceCharacter->m_animations.size() && animationTime > m_ReferenceCharacter->m_animations[animation]->duration) 
    {
            animationTime = 0.f;
    }

    UpdateWorldMatrix();
}
