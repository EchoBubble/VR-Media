// Copyright 2019 UNAmedia. All rights reserved.

#include "ProceduralSphereComponent.h"


UProceduralSphereComponent::UProceduralSphereComponent(const FObjectInitializer& ObjectInitializer)
    : Super(ObjectInitializer)
{
}

void UProceduralSphereComponent::GenerateMesh(float Radius, int32 NumOfParallels, int32 NumOfMeridians)
{
    check(Radius > 0.f);
    check(NumOfParallels > 1);
    check(NumOfMeridians > 1);

    float const R = 1.f / (float)(NumOfParallels - 1);
    float const S = 1.f / (float)(NumOfMeridians - 1);

    TArray<FVector> Vertices;
    TArray<FVector> Normals;
    TArray<int32> Triangles;
    TArray<FVector2D> UV0;
    TArray<FColor> VertexColors;
    TArray<FProcMeshTangent> Tangents;

    uint32 NumOfVertices = NumOfParallels * NumOfMeridians;

    Vertices.SetNumUninitialized(NumOfVertices);
    Normals.SetNumUninitialized(NumOfVertices);
    //Tangents.SetNumUninitialized(NumOfVertices);
    VertexColors.SetNumUninitialized(NumOfVertices);
    UV0.SetNumUninitialized(NumOfVertices);
    Triangles.SetNumUninitialized((NumOfParallels - 1) * (NumOfMeridians - 1) * 6);

    auto v = Vertices.CreateIterator();
    auto n = Normals.CreateIterator();
    //auto t = Tangents.CreateIterator();
    auto uv = UV0.CreateIterator();
    auto i = Triangles.CreateIterator();

    // Make a matrix to rotate 180° on z-axis
    FRotationMatrix M(FRotator{ 0, 180, 0 });

    for (int32 r = 0; r < NumOfParallels; r++)
    {
        for (int32 s = 0; s < NumOfMeridians; s++)
        {
            float x = cos(2 * PI * s * S) * sin(PI * r * R);
            float y = sin(2 * PI * s * S) * sin(PI * r * R);
            float z = -sin(-HALF_PI + PI * r * R);

            // This rotation around the z-axis is needed to
            // match the center of the pano with the forward vec (x-axis).
            // It's almost equivalent to add +0.5f to UV.x but only if the
            // wrapping mode is "repeat".
            FVector V = M.TransformVector({ x, y, z });

            // uv
            *uv++ = { s * S, r * R };

            // vertices
            *v++ = V * Radius;

            // normals
            *n++ = -V;

            // tangents
            //*t++ = { }
        }
    }

    for (int32 r = 0; r < NumOfParallels - 1; r++)
    {
        for (int32 s = 0; s < NumOfMeridians - 1; s++)
        {
            int32 a = r * NumOfMeridians + s;
            int32 b = (r + 1) * NumOfMeridians + s;
            int32 c = r * NumOfMeridians + (s + 1);
            int32 d = (r + 1) * NumOfMeridians + (s + 1);

            // indices
            *i++ = c;
            *i++ = a;
            *i++ = d;

            *i++ = a;
            *i++ = b;
            *i++ = d;
        }
    }

    ClearAllMeshSections();
    CreateMeshSection(0, Vertices, Triangles, Normals, UV0, VertexColors, Tangents, false);
}

FBoxSphereBounds UProceduralSphereComponent::CalcBounds(const FTransform& LocalToWorld) const
{
    // Returns huge bounds to avoid culling
    return FBoxSphereBounds(FVector::ZeroVector, FVector(WORLD_MAX), WORLD_MAX); // .TransformBy(LocalToWorld);
}
