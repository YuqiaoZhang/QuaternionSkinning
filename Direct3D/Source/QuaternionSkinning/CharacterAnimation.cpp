#include "DXUT.h"
#include "CharacterAnimation.h"

CharacterAnimation::CharacterAnimation()
{
    timeStep = 1/30.f;    // $$ 30fps animation interpolation off the curves
}

CharacterAnimation::~CharacterAnimation()
{
    while(frames.size())
    {
        delete [] frames.back();
        frames.pop_back();
    }

}

D3DXMATRIX *CharacterAnimation::GetFrameAt(float time)
{
    return frames[GetFrameIndexAt(time)];
}

D3DXMATRIX *CharacterAnimation::GetQuatFrameAt(float time)
{
    return quatFrames[GetFrameIndexAt(time)];
}

D3DXMATRIX *CharacterAnimation::GetFrame(int index)
{

    assert(index < (int)frames.size());
    return frames[index];
}

int CharacterAnimation::GetFrameIndexAt(float time)
{
    // get a [0.f ... 1.f) value by allowing the percent to wrap around 1
    float percent = time / duration;
    int percentINT = (int)percent;
    percent = percent - (float)percentINT;

    return (int)((float)frames.size() * percent);
}
