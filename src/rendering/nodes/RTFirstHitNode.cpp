#include "RTFirstHitNode.h"

#include "RTAccelerationStructures.h"
#include <half.hpp>
#include <imgui.h>

RTFirstHitNode::RTFirstHitNode(Scene& scene)
    : RenderGraphNode(RTFirstHitNode::name())
    , m_scene(scene)
{
}

std::string RTFirstHitNode::name()
{
    return "rt-firsthit";
}

void RTFirstHitNode::constructNode(Registry& nodeReg)
{
    std::vector<Buffer*> vertexBuffers {};
    std::vector<Buffer*> indexBuffers {};
    std::vector<Texture*> allTextures {};
    std::vector<RTMesh> rtMeshes {};

    auto createTriangleMeshVertexBuffer = [&](Mesh& mesh) {
        // TODO: Would be nice if this could be cached too!
        std::vector<RTVertex> vertices {};
        {
            auto& posData = mesh.positionData();
            auto& normalData = mesh.normalData();
            auto& texCoordData = mesh.texcoordData();

            ASSERT(posData.size() == normalData.size());
            ASSERT(posData.size() == texCoordData.size());

            for (int i = 0; i < posData.size(); ++i) {
                vertices.push_back({ .position = vec4(posData[i], 0.0f),
                                     .normal = vec4(mesh.transform().localNormalMatrix() * normalData[i], 0.0f),
                                     .texCoord = vec4(texCoordData[i], 0.0f, 0.0f) });
            }
        }

        int texId = allTextures.size();
        allTextures.push_back(mesh.material().baseColorTexture());

        int meshId = rtMeshes.size();
        rtMeshes.push_back({ .objectId = meshId,
                             .baseColor = texId });

        // TODO: Later, we probably want to have combined vertex/ssbo and index/ssbo buffers instead!
        vertexBuffers.push_back(&nodeReg.createBuffer(vertices, Buffer::Usage::StorageBuffer, Buffer::MemoryHint::GpuOptimal));
        indexBuffers.push_back(&nodeReg.createBuffer(mesh.indexData(), Buffer::Usage::StorageBuffer, Buffer::MemoryHint::GpuOptimal));
    };

    m_scene.forEachModel([&](size_t, Model& model) {
        model.forEachMesh([&](Mesh& mesh) {
            createTriangleMeshVertexBuffer(mesh);
        });
    });

    Buffer& meshBuffer = nodeReg.createBuffer(rtMeshes, Buffer::Usage::StorageBuffer, Buffer::MemoryHint::GpuOptimal);
    m_objectDataBindingSet = &nodeReg.createBindingSet({ { 0, ShaderStageRTClosestHit, &meshBuffer },
                                                         { 1, ShaderStageRTClosestHit, vertexBuffers },
                                                         { 2, ShaderStageRTClosestHit, indexBuffers },
                                                         { 3, ShaderStageRTClosestHit, allTextures, RT_MAX_TEXTURES } });
}

RenderGraphNode::ExecuteCallback RTFirstHitNode::constructFrame(Registry& reg) const
{
    Texture& storageImage = reg.createTexture2D(reg.windowRenderTarget().extent(), Texture::Format::RGBA16F);
    reg.publish("image", storageImage);

    Buffer& timeBuffer = reg.createBuffer(sizeof(float), Buffer::Usage::UniformBuffer, Buffer::MemoryHint::TransferOptimal);
    BindingSet& environmentBindingSet = reg.createBindingSet({ { 0, ShaderStageRTMiss, reg.getTexture("scene", "environmentMap").value_or(&reg.createPixelTexture(vec4(1), true)), ShaderBindingType::TextureSampler } });

    TopLevelAS& sceneTLAS = *reg.getTopLevelAccelerationStructure(RTAccelerationStructures::name(), "scene");
    BindingSet& frameBindingSet = reg.createBindingSet({ { 0, ShaderStageRTRayGen, &sceneTLAS },
                                                         { 1, ShaderStageRTRayGen, &storageImage, ShaderBindingType::StorageImage },
                                                         { 2, ShaderStageRTRayGen, reg.getBuffer("scene", "camera") },
                                                         { 3, ShaderStageRTMiss, &timeBuffer } });

    ShaderFile raygen = ShaderFile("rt-firsthit/raygen.rgen");
    HitGroup mainHitGroup { ShaderFile("rt-firsthit/closestHit.rchit") };
    ShaderFile missShader { ShaderFile("rt-firsthit/miss.rmiss") };
    ShaderBindingTable sbt { raygen, { mainHitGroup }, { missShader } };

    uint32_t maxRecursionDepth = 1;
    RayTracingState& rtState = reg.createRayTracingState(sbt, { &frameBindingSet, m_objectDataBindingSet, &environmentBindingSet }, maxRecursionDepth);

    return [&](const AppState& appState, CommandList& cmdList) {
        cmdList.setRayTracingState(rtState);
        cmdList.bindSet(frameBindingSet, 0);

        float time = appState.elapsedTime();
        timeBuffer.updateData(&time, sizeof(time));

        cmdList.bindSet(*m_objectDataBindingSet, 1);
        cmdList.bindSet(environmentBindingSet, 2);
        cmdList.traceRays(appState.windowExtent());
    };
}
