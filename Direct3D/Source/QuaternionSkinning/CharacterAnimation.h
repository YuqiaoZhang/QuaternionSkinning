#pragma once 

#include <vector>

class CharacterSkeleton;
struct CharacterBone;

/*
    A single animation for a character.  Hold a list of interpolated frames.
*/
class CharacterAnimation
{
public:
    CharacterAnimation();
    ~CharacterAnimation();

    typedef std::vector<D3DXMATRIX *> FrameList;

    D3DXMATRIX *GetFrameAt(float time);
    D3DXMATRIX *GetQuatFrameAt(float time);
    D3DXMATRIX *GetFrame(int index);
    int GetFrameIndexAt(float time);
    int GetNumFrames(){return (int)frames.size();}

    float duration;
    float timeStep;

    std::string name;
    
    FrameList frames;
    FrameList quatFrames;
};