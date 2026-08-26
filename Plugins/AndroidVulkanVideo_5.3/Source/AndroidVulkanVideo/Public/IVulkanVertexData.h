#pragma once

struct VertexData
{
    float x;
    float y;
    float z;
    float _ignored1;
    float u;
    float v;
    float _ignored2;
    float _ignored3;
};

struct ShaderModelMatrix
{
    ShaderModelMatrix()
    {
        memset(this, 0, sizeof(ShaderModelMatrix));
        for (int c = 0; c < 4; c++)
        {
            m44[c + c * 4] = 1.0;
        }
    }

    float m44[16];
};

struct ShaderViewMatrices
{
    ShaderViewMatrices()
    {
        memset(this, 0, sizeof(ShaderViewMatrices));
        for (int c = 0; c < 4; c++)
        {
            vp44[c + c * 4] = 1;
            vp44_2[c + c * 4] = 1;
        }
    }
    float vp44[16];
    float vp44_2[16];
    float pre_view_translation[4];
    float pre_view_translation_2[4];
};
