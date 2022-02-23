#pragma once

class AnimatedCharacter;

/*
    A placed and animating character instance.  Simply wraps up the position, animation, and mesh set for this instance.
*/
class AnimatedCharacterInstance
{
public:
    // Character logic
    enum AIState
    {
        DEAD3 = 0,
        DEAD2 = 1,
        DEAD1 = 2,
        IDLE2 = 3,
        JUMPCHEER = 4,
        IDLE1 = 0,
        CHEER3 = 6,
        CHEER2 = 7,
        CHEER1 = 8,
        MAX_STATES = 9
    };

    AnimatedCharacterInstance();
    ~AnimatedCharacterInstance();

    void Initialize(D3DXVECTOR3 position, D3DXVECTOR3 rotation, D3DXVECTOR3 scale = D3DXVECTOR3(1, 1, 1));
    void Initialize(D3DXMATRIX worldMatrix);

    void MeshSet(AnimatedCharacter *m_ReferenceCharacter);

    void SetState(AIState state);

    void UpdateWorldMatrix();
    void Update(float deltatime, const D3DXVECTOR3 &cameraAt, const D3DXVECTOR3 &camDir);

    AnimatedCharacter *m_ReferenceCharacter;
    int animation;
    float animationTime;
    float animationPlaybackRate;
    int meshSet;
    D3DXVECTOR3 color;
    D3DXVECTOR3 baseColor;
    D3DXMATRIX mWorld;

protected:
    bool bOnlyWorldMatrix;
    D3DXVECTOR3 mPosition;
    D3DXVECTOR3 mRotation;
    D3DXVECTOR3 mScale;

    int GetAnimationForState(AIState state);

    static char **animationNames;

    AIState iState;
    D3DXVECTOR3 targetPosition;
};
