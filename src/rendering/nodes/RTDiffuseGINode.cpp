#include "RTDiffuseGINode.h"

#include "ForwardRenderNode.h"
#include "LightData.h"
#include "RTAccelerationStructures.h"
#include "SceneUniformNode.h"
#include "utility/GlobalState.h"
#include <half.hpp>
#include <imgui.h>

RTDiffuseGINode::RTDiffuseGINode(const Scene& scene)
    : RenderGraphNode(RTDiffuseGINode::name())
    , m_scene(scene)
{
}

std::string RTDiffuseGINode::name()
{
    return "rt-diffuse-gi";
}

void RTDiffuseGINode::constructNode(Registry& nodeReg)
{
    std::vector<const Buffer*> vertexBuffers {};
    std::vector<const Buffer*> indexBuffers {};
    std::vector<const Texture*> allTextures {};
    std::vector<RTMesh> rtMeshes {};

    auto createTriangleMeshVertexBuffer = [&](const Mesh& mesh) {
        // TODO: Would be nice if this could be cached too!
        std::vector<RTVertex> vertices {};
        {
            auto posData = mesh.positionData();
            auto normalData = mesh.normalData();
            auto texCoordData = mesh.texcoordData();

            ASSERT(posData.size() == normalData.size());
            ASSERT(posData.size() == texCoordData.size());

            for (int i = 0; i < posData.size(); ++i) {
                vertices.push_back({ .position = vec4(posData[i], 0.0f),
                                     .normal = vec4(mesh.transform().localNormalMatrix() * normalData[i], 0.0f),
                                     .texCoord = vec4(texCoordData[i], 0.0f, 0.0f) });
            }
        }

        const Material& material = mesh.material();
        Texture* baseColorTexture { nullptr };
        if (material.baseColor.empty()) {
            // the color is already in linear sRGB so we don't want to make an sRGB texture for it!
            baseColorTexture = &nodeReg.createPixelTexture(material.baseColorFactor, false);
        } else {
            baseColorTexture = &nodeReg.loadTexture2D(material.baseColor, true, true);
        }

        size_t texIndex = allTextures.size();
        allTextures.push_back(baseColorTexture);

        rtMeshes.push_back({ .objectId = (int)rtMeshes.size(),
                             .baseColor = (int)texIndex });

        // TODO: Later, we probably want to have combined vertex/ssbo and index/ssbo buffers instead!
        vertexBuffers.push_back(&nodeReg.createBuffer(vertices, Buffer::Usage::StorageBuffer, Buffer::MemoryHint::GpuOptimal));
        indexBuffers.push_back(&nodeReg.createBuffer(mesh.indexData(), Buffer::Usage::StorageBuffer, Buffer::MemoryHint::GpuOptimal));
    };

    m_scene.forEachModel([&](size_t, const Model& model) {
        model.forEachMesh([&](const Mesh& mesh) {
            createTriangleMeshVertexBuffer(mesh);
        });

        model.proxy().forEachMesh([&](const Mesh& proxyMesh) {
            createTriangleMeshVertexBuffer(proxyMesh);
        });
    });

    Buffer& meshBuffer = nodeReg.createBuffer(std::move(rtMeshes), Buffer::Usage::StorageBuffer, Buffer::MemoryHint::GpuOptimal);
    m_objectDataBindingSet = &nodeReg.createBindingSet({ { 0, ShaderStageRTClosestHit, &meshBuffer, ShaderBindingType::StorageBuffer },
                                                         { 1, ShaderStageRTClosestHit, vertexBuffers },
                                                         { 2, ShaderStageRTClosestHit, indexBuffers },
                                                         { 3, ShaderStageRTClosestHit, allTextures, RT_MAX_TEXTURES } });

    Extent2D windowExtent = GlobalState::get().windowExtent();
    m_accumulationTexture = &nodeReg.createTexture2D(windowExtent, Texture::Format::RGBA16F, Texture::Usage::StorageAndSample);
}

RenderGraphNode::ExecuteCallback RTDiffuseGINode::constructFrame(Registry& reg) const
{
    const Texture* gBufferColor = reg.getTexture("g-buffer", "baseColor").value();
    const Texture* gBufferNormal = reg.getTexture("g-buffer", "normal").value();
    const Texture* gBufferDepth = reg.getTexture("g-buffer", "depth").value();

    auto createStateForTLAS = [&](const TopLevelAS& tlas) -> std::pair<BindingSet&, RayTracingState&> {
        BindingSet& frameBindingSet = reg.createBindingSet({ { 0, ShaderStage(ShaderStageRTRayGen | ShaderStageRTClosestHit), &tlas },
                                                             { 1, ShaderStageRTRayGen, m_accumulationTexture, ShaderBindingType::StorageImage },
                                                             { 2, ShaderStageRTRayGen, gBufferColor, ShaderBindingType::TextureSampler },
                                                             { 3, ShaderStageRTRayGen, gBufferNormal, ShaderBindingType::TextureSampler },
                                                             { 4, ShaderStageRTRayGen, gBufferDepth, ShaderBindingType::TextureSampler },
                                                             { 5, ShaderStageRTRayGen, reg.getBuffer(SceneUniformNode::name(), "camera") },
                                                             { 6, ShaderStageRTMiss, reg.getBuffer(SceneUniformNode::name(), "environmentData") },
                                                             { 7, ShaderStageRTMiss, reg.getTexture(SceneUniformNode::name(), "environmentMap").value_or(&reg.createPixelTexture(vec4(1.0), true)) },
                                                             { 8, ShaderStageRTClosestHit, reg.getBuffer(SceneUniformNode::name(), "directionalLight") } });

        ShaderFile raygen = ShaderFile("rt-diffuseGI/raygen.rgen");
        HitGroup mainHitGroup { ShaderFile("rt-diffuseGI/closestHit.rchit") };
        std::vector<ShaderFile> missShaders { ShaderFile("rt-diffuseGI/miss.rmiss"),
                                              ShaderFile("rt-diffuseGI/shadow.rmiss") };
        ShaderBindingTable sbt { raygen, { mainHitGroup }, missShaders };

        uint32_t maxRecursionDepth = 2;
        RayTracingState& rtState = reg.createRayTracingState(sbt, { &frameBindingSet, m_objectDataBindingSet }, maxRecursionDepth);

        return { frameBindingSet, rtState };
    };

    const TopLevelAS& mainTLAS = *reg.getTopLevelAccelerationStructure(RTAccelerationStructures::name(), "scene");
    auto& [frameBindingSet, rtState] = createStateForTLAS(mainTLAS);

    const TopLevelAS& proxyTLAS = *reg.getTopLevelAccelerationStructure(RTAccelerationStructures::name(), "proxy");
    auto& [frameBindingSetProxy, rtStateProxy] = createStateForTLAS(proxyTLAS);

    Texture& diffuseGI = reg.createTexture2D(reg.windowRenderTarget().extent(), Texture::Format::RGBA16F, Texture::Usage::StorageAndSample);
    reg.publish("diffuseGI", diffuseGI);

    BindingSet& avgAccumBindingSet = reg.createBindingSet({ { 0, ShaderStageCompute, m_accumulationTexture, ShaderBindingType::StorageImage },
                                                            { 1, ShaderStageCompute, &diffuseGI, ShaderBindingType::StorageImage } });
    ComputeState& compAvgAccumState = reg.createComputeState(Shader::createCompute("common/averageAccum.comp"), { &avgAccumBindingSet });

    return [&](const AppState& appState, CommandList& cmdList) {
        constexpr int samplesPerPass = 1; // (I don't wanna pass in a uniform for optimization reasons, so keep this up to date!)
        int currentSamplesPerPixel = samplesPerPass * m_numAccumulatedFrames;

        if (currentSamplesPerPixel < maxSamplesPerPixel) {
            ImGui::TextColored(ImVec4(1, 0, 0, 1), "Accumulating ... (%i SPP)", currentSamplesPerPixel);
        } else {
            ImGui::TextColored(ImVec4(0, 1, 0, 1), "Ready! (%i SPP)", currentSamplesPerPixel);
        }

        static bool doRender = true;
        ImGui::Checkbox("Render", &doRender);
        static bool ignoreColor = false;
        ImGui::Checkbox("Ignore color", &ignoreColor);
        static bool useProxies = false;
        ImGui::Checkbox("Use proxies", &useProxies);

        if (!doRender) {
            return;
        }

        if (Input::instance().wasKeyPressed(GLFW_KEY_O)) {
            useProxies = false;
        }
        if (Input::instance().wasKeyPressed(GLFW_KEY_P)) {
            useProxies = true;
        }

        if (useProxies) {
            cmdList.setRayTracingState(rtStateProxy);
            cmdList.bindSet(frameBindingSetProxy, 0);
        } else {
            cmdList.setRayTracingState(rtState);
            cmdList.bindSet(frameBindingSet, 0);
        }

        cmdList.bindSet(*m_objectDataBindingSet, 1);
        cmdList.pushConstant(ShaderStageRTRayGen, ignoreColor);
        cmdList.pushConstant(ShaderStageRTRayGen, appState.frameIndex(), 4);

        cmdList.waitEvent(0, appState.frameIndex() == 0 ? PipelineStage::Host : PipelineStage::RayTracing);
        cmdList.resetEvent(0, PipelineStage::RayTracing);
        {
            if (m_scene.camera().didModify() || Input::instance().isKeyDown(GLFW_KEY_R)) {
                cmdList.clearTexture(*m_accumulationTexture, ClearColor(0, 0, 0));
                m_numAccumulatedFrames = 0;
            }

            if (currentSamplesPerPixel < maxSamplesPerPixel) {
                cmdList.traceRays(appState.windowExtent());
                m_numAccumulatedFrames += 1;
            }

            cmdList.debugBarrier(); // TODO: Add fine grained barrier here to make sure ray tracing is done before averaging!

            cmdList.setComputeState(compAvgAccumState);
            cmdList.bindSet(avgAccumBindingSet, 0);
            cmdList.pushConstant(ShaderStageCompute, m_numAccumulatedFrames);

            Extent2D globalSize = appState.windowExtent();
            cmdList.dispatch(globalSize, Extent3D(16));
        }
        cmdList.signalEvent(0, PipelineStage::RayTracing);
    };
}
