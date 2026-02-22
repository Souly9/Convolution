#pragma once

#include <additional_libs/SSS/bend_sss_cpu.h>
#include <DirectXMath.h>

namespace RenderPasses
{
namespace SSSHelper
{
    inline Bend::DispatchList BuildBendDispatchList(
        const mathstl::Vector3& dirLightDirection,
        const mathstl::Matrix& mainCamInvViewProj,
        const DirectX::XMUINT3& depthExtents)
    {
        mathstl::Vector4 lightDir(dirLightDirection.x, dirLightDirection.y, dirLightDirection.z, 0.0f);
        lightDir.Normalize();
        
        mathstl::Matrix actualViewProj = mainCamInvViewProj.Invert();

        mathstl::Vector4 lightProjVec = DirectX::XMVector4Transform(lightDir, actualViewProj);
        
        float inLightProj[4] = { lightProjVec.x, lightProjVec.y, lightProjVec.z, lightProjVec.w };
        
        int viewportSize[2] = { (int)depthExtents.x, (int)depthExtents.y };
        int minBounds[2] = { 0, 0 };
        int maxBounds[2] = { (int)depthExtents.x, (int)depthExtents.y };
        
        return Bend::BuildDispatchList(inLightProj, viewportSize, minBounds, maxBounds, false, 64);
    }
}
}
