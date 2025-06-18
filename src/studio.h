#pragma once

namespace r5
{
    struct mstudiocollmodel_t
    {
        int contentMasksIndex;
        int surfacePropsIndex;
        int surfaceNamesIndex;
        int headerCount;
    };


    namespace v8
    {
        struct mstudiocollheader_t
        {
            int unk;
            int bvhNodeIndex;
            int vertIndex;
            int bvhLeafIndex;
            float origin[3];
            float scale;
        };
    }

    namespace v121
    {
        struct mstudiocollheader_t
        {
            int unk;
            int bvhNodeIndex;
            int vertIndex;
            int bvhLeafIndex;
            int unkIndex;
            int unkNew;
            float origin[3];
            float scale;
        };
    }
}
