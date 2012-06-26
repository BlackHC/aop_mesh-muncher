/**
* @author: Matthias Reitinger
*
* License: 3c-BSD
*/

#include "Core/inc/Core.h"
#include "Core/inc/CommandlineParser.h"
#include "Core/inc/Exception.h"
#include "Core/inc/Log.h"
#include "Core/inc/nString.h"
#include "Core/inc/StringFormat.h"
#include "Core/inc/io/File.h"
#include "Core/inc/io/FileSystem.h"

#include "Engine/inc/BaseApplication3D.h"
#include "niven.Render.DrawCommand.h"
#include "niven.Render.RenderBuffer.h"
#include "niven.Render.IndexBuffer.h"
#include "niven.Render.VertexBuffer.h"
#include "niven.Render.RenderContext.h"
#include "niven.Render.RenderSystem.h"
#include "niven.Render.RenderTargetDescriptor.h"
#include "niven.Render.RenderTargetTexture3D.h"
#include "niven.Render.Effect.h"
#include "niven.Render.EffectLoader.h"
#include "niven.Render.EffectManager.h"
#include "niven.Render.StreamOutQuery.h"
#include "niven.Render.Texture3D.h"
#include "niven.Render.VertexFormats.PositionNormal.h"
#include "niven.Render.VertexLayout.h"
#include "niven.Volume.ShardFile.h"
#include "niven.Volume.MarchingCubes.h"
#include "niven.Volume.Volume.h"
#include <iostream>

using namespace niven;

class SampleApplication : public BaseApplication3D 
{
	NIV_DEFINE_CLASS(SampleApplication, BaseApplication3D)

private:
	String volumeName_;
	
public:
	SampleApplication(const String& volumeName)
		: volumeName_(volumeName),
	{
	}

	// Default constructor is needed for NIV_DEFINE_CLASS
	SampleApplication() 
	{
	}

private:
	void InitImpl() 
	{
		Super::InitImpl();

		camera_->GetFrustum().SetPerspectiveProjection(
			Degree (75.0f),
			renderWindow_->GetAspectRatio(),
			1.0f, 1024.0f);

		camera_->SetViewLookAt(Vector3f(16, 16, -50),
			Vector3f(16, 16, 16),
			Vector3f(0, 1, 0));

		camera_->SetMoveSpeedMultiplier(64.0f);

		effectManager_.Initialize(renderSystem_.get(), &effectLoader_);

		listCellseffect_   = effectManager_.GetEffect ("MCListCells");
		listEdgeseffect_   = effectManager_.GetEffect ("MCListEdges");
		genVerticeseffect_ = effectManager_.GetEffect ("MCGenVerts");
		splatIDseffect_    = effectManager_.GetEffect ("MCSplatIDs");
		genIndiceseffect_  = effectManager_.GetEffect ("MCGenIndices");
		rendereffect_      = effectManager_.GetEffect ("MCRender",
			Render::VertexFormats::PositionNormal::GetId());

		vertexLayout_ = renderSystem_->CreateVertexLayout(
			Render::VertexFormats::PositionNormal::GetVertexLayoutElementCount(),
			Render::VertexFormats::PositionNormal::GetVertexLayout(),
			rendereffect_->GetVertexShaderProgram ());

		Log::Info("MarchingCubesSample", "Loading shard file");
		if( !shardFile_.Open( volumeName_, true ) ) {
			throw NIV_RUNTIME_EXCEPTION( "Could not open shard file!", volumeName_ );
		}

		Log::Info("MarchingCubesSample", "Loading volume from file");
		ScopedArray<float> data(Math::Cube(voxelDimWithMargins_));
		IFile::Ptr volumeFile = FileSystem::OpenFile(volumeName_);    
		volumeFile->Read (data.Get (), data.GetSize ());
		Texture3DDescriptor desc(voxelDimWithMargins_,
			voxelDimWithMargins_,
			voxelDimWithMargins_,
			PixelFormat::G32_F);
		const void* dataByte = static_cast<void*>(data.Get());
		desc.SetData(&dataByte);
		auto volume = renderSystem_->CreateTexture3D(desc);

		Log::Info("MarchingCubesSample", "Running marching cubes algorithm");
		CreateResources();
		MarchingCubes(volume);
		renderSystem_->Release (volume);

		if (primitiveCount_ == 0) {
			Log::Warn("MarchingCubesSample", "No vertices generated");
		}
	}

	void ShutdownImpl() 
	{
		effectManager_.Shutdown ();

		Super::ShutdownImpl();
	}

	void DrawImpl() 
	{
		if (primitiveCount_ == 0) return;

		DrawIndexedCommand dic;
		dic.SetIndexBuffer(indexBuffer_);
		dic.SetVertexBuffers(1, &vertexBuffer_);
		dic.SetVertexLayout(vertexLayout_);
		dic.type = PrimitiveType::Triangle_List;
		dic.indexCount = indexCount_;
		dic.vertexCount = primitiveCount_;

		rendereffect_->Bind (renderContext_);
		renderContext_->Draw(dic);
		rendereffect_->Unbind (renderContext_);
	}

	void CreateResources() 
	{
		ids_ = shardFile_.GetBlockIds("Density");

		vertexBuffers_.resize( ids_.size() );
		indexBuffers_.resize( ids_.size() );

		const Volume::ShardFile::LayerDescriptor &layerDesc = shardFile_.GetLayerDescriptor("Density");
		Volume::FloatBlock floatDensity = {new float[3 * layerDesc.blockSize * layerDesc.blockSize * layerDesc.blockSize], layerDesc.blockSize, 0, 0};

		// convert the density blocks to FloatBlocks on-the-fly
		for( auto iid = ids_.cbegin() ; iid != ids_.cend() ; iid++ ) {
			auto id = *iid;
					
		}

		// Create indices volume render target
		RenderTargetTexture3DDescriptor desc(3*voxelDim_, voxelDim_, voxelDim_);
		desc.AddTarget(PixelFormat::G32, RenderTargetFlags::Allow_Shader_Binding);
		indicesVolumeTarget_ = renderSystem_->CreateRenderTargetTexture3D(desc);

		// Create cell vertex buffer
		cellBuffer_ = renderSystem_->CreateVertexBuffer(
			sizeof(uint32),
			Math::Cube(voxelDim_),
			Render::ResourceUsage::StreamOut);

		// Create cell vertex layout
		{
			VertexElement elements[] = {
				VertexElement(0, VertexElementType::UInt_1, VertexElementSemantic::Position)
			};
			cellLayout_ = renderSystem_->CreateVertexLayout(1, elements,
				listEdgeseffect_->GetVertexShaderProgram ());
		}

		// Create edge vertex buffer
		edgeBuffer_ = renderSystem_->CreateVertexBuffer(
			sizeof(uint32),
			3 * Math::Cube(voxelDim_),
			Render::ResourceUsage::StreamOut);

		// Create edge vertex layout
		{
			VertexElement elements[] = {
				VertexElement(0, VertexElementType::UInt_1, VertexElementSemantic::Position)
			};
			edgeLayout_ = renderSystem_->CreateVertexLayout(1, elements,
				genVerticeseffect_->GetVertexShaderProgram ());
		}

		// Create voxel slice vertex buffer
		ScopedArray<uint32> voxels(Math::Square(voxelDim_)*2);
		uint idx = 0;
		for (uint i = 0; i < voxelDim_; ++i) {
			for (uint j = 0; j < voxelDim_; ++j) {
				voxels[idx++] = i;
				voxels[idx++] = j;
			}
		}
		voxelSliceBuffer_ = renderSystem_->CreateVertexBuffer(
			2*sizeof(uint32),
			Math::Square(voxelDim_),
			Render::ResourceUsage::Static,
			voxels.Get());

		// Create voxel slice vertex layout
		{
			VertexElement elements[] = {
				VertexElement(0, VertexElementType::UInt_2, VertexElementSemantic::Position)
			};
			voxelSliceLayout_ = renderSystem_->CreateVertexLayout(1, elements,
				listCellseffect_->GetVertexShaderProgram ());
		}
	}

	void SetShaderVariables(Render::Effect* shader)
	{
		shader->SetInt("Margin", margin_);
		shader->SetInt("VoxelDim", voxelDim_);
		shader->SetInt("VoxelDimMinusOne", voxelDimMinusOne_);
		shader->SetInt("VoxelDimWithMargins", voxelDimWithMargins_);
		shader->SetVector("InvVoxelDim", Vector2f(invVoxelDim_, 0));
		shader->SetVector("InvVoxelDimMinusOne", Vector2f(invVoxelDimMinusOne_, 0));
		shader->SetVector("InvVoxelDimWithMargins", Vector2f(invVoxelDimWithMargins_, 0));
		shader->SetFloat("BlockSize", blockSize_);
		shader->SetFloat("IsoValue", isoValue_);
	}

	void MarchingCubes(ITexture3D* volume) 
	{
		if (ListNonEmptyCells(volume) == 0) {
			primitiveCount_ = 0;
			indexCount_ = 0;
			return;
		}
		if (ListEdges() == 0) { // Should not happen at all
			indexCount_ = 0;
			return;
		}
		CreateRenderBuffers();
		GenerateVertices(volume);

		SplatIndices();
		GenerateIndices();
	}

	int ListNonEmptyCells(ITexture3D* volume) 
	{
		DrawInstancedCommand dic;
		dic.SetStreamOutBuffer(0, cellBuffer_);
		dic.SetVertexBuffers(1, &voxelSliceBuffer_);
		dic.SetVertexLayout(voxelSliceLayout_);
		dic.type = PrimitiveType::Point_List;
		dic.vertexCount = Math::Square(voxelDim_);
		dic.instanceCount = voxelDim_;

		SetShaderVariables(listCellseffect_);
		listCellseffect_->SetTexture("Volume", volume);

		listCellseffect_->Bind (renderContext_);

		auto query = renderSystem_->CreateStreamOutQuery();
		renderContext_->BeginQuery(query);
		renderContext_->Draw(dic);
		renderContext_->EndQuery(query);
		renderContext_->UpdateQueryResult(query, true);
		nonEmptyCellCount_ = static_cast<int>(query->GetPrimitivesWritten());
		renderSystem_->Release (query);

		listCellseffect_->Unbind (renderContext_);

		return nonEmptyCellCount_;
	}

	int ListEdges() 
	{
		DrawAutoCommand dc;
		dc.SetStreamOutBuffer(0, edgeBuffer_);
		dc.SetVertexBuffers(1, &cellBuffer_);
		dc.SetVertexLayout(cellLayout_);
		dc.type = PrimitiveType::Point_List;

		SetShaderVariables(listEdgeseffect_);

		listEdgeseffect_->Bind (renderContext_);

		auto query = renderSystem_->CreateStreamOutQuery();
		renderContext_->BeginQuery(query);    
		renderContext_->Draw(dc);
		renderContext_->EndQuery(query);
		renderContext_->UpdateQueryResult(query, true);
		primitiveCount_ = static_cast<int>(query->GetPrimitivesWritten());
		renderSystem_->Release (query);

		listEdgeseffect_->Unbind (renderContext_);

		return primitiveCount_;
	}

	void CreateRenderBuffers() 
	{
		vertexBuffer_ = renderSystem_->CreateVertexBuffer(
			2 * sizeof(Vector3f), // Position + Normal
			primitiveCount_,
			Render::ResourceUsage::StreamOut);

		indexBuffer_ = renderSystem_->CreateIndexBuffer(
			IndexBufferFormat::UInt_32,
			15 * nonEmptyCellCount_, // Max. 5 triangles per cell
			Render::ResourceUsage::StreamOut);
	}

	void GenerateVertices(ITexture3D* volume) 
	{
		DrawAutoCommand dc;
		dc.SetStreamOutBuffer(0, vertexBuffer_);
		dc.SetVertexBuffers(1, &edgeBuffer_);
		dc.SetVertexLayout(edgeLayout_);
		dc.type = PrimitiveType::Point_List;

		SetShaderVariables(genVerticeseffect_);
		genVerticeseffect_->SetTexture("Volume", volume);

		genVerticeseffect_->Bind (renderContext_);
		renderContext_->Draw(dc);
		genVerticeseffect_->Unbind (renderContext_);
	}

	void SplatIndices()
	{
		DrawAutoCommand dc;
		dc.SetVertexBuffers(1, &edgeBuffer_);
		dc.SetVertexLayout(edgeLayout_);
		dc.type = PrimitiveType::Point_List;

		renderContext_->SetRenderTarget(indicesVolumeTarget_);
		renderContext_->SetViewport(0, 0, 3*voxelDim_, voxelDim_);

		SetShaderVariables(splatIDseffect_);

		splatIDseffect_->Bind (renderContext_);
		renderContext_->Draw(dc);
		splatIDseffect_->Unbind (renderContext_);

		renderContext_->SetRenderTarget(renderWindow_);
		renderContext_->SetViewport(0, 0, renderWindow_->GetWidth(), renderWindow_->GetHeight());
	}

	int GenerateIndices() 
	{
		DrawAutoCommand dc;
		dc.SetStreamOutBuffer(0, indexBuffer_);
		dc.SetVertexBuffers(1, &cellBuffer_);
		dc.SetVertexLayout(cellLayout_);
		dc.type = PrimitiveType::Point_List;

		SetShaderVariables(genIndiceseffect_);
		genIndiceseffect_->SetTexture("IndicesVolume", indicesVolumeTarget_->GetRenderTexture(0));

		genIndiceseffect_->Bind (renderContext_);

		auto query = renderSystem_->CreateStreamOutQuery();
		renderContext_->BeginQuery(query);
		renderContext_->Draw(dc);
		renderContext_->EndQuery(query);
		renderContext_->UpdateQueryResult(query, true);
		indexCount_ = static_cast<int>(query->GetPrimitivesWritten());
		renderSystem_->Release (query);

		genIndiceseffect_->Unbind (renderContext_);

		return indexCount_;
	}

	String GetMainWindowName () const
	{
		return "GPU marching cubes sample";
	}

private:
	Render::EffectManager	effectManager_;
	Render::EffectLoader	effectLoader_;

	Volume::ShardFile		shardFile_;

	Render::Effect*			listCellseffect_;
	Render::Effect*			listEdgeseffect_;
	Render::Effect*			genVerticeseffect_;
	Render::Effect*			splatIDseffect_;
	Render::Effect*			genIndiceseffect_;
	Render::Effect*			rendereffect_;

	// Intermediate buffers for marching cubes
	IVertexBuffer*			voxelSliceBuffer_;
	IVertexLayout*			voxelSliceLayout_;  
	IVertexBuffer*			cellBuffer_;
	IVertexLayout*			cellLayout_;
	IVertexBuffer*			edgeBuffer_;
	IVertexLayout*			edgeLayout_;

	IRenderTargetTexture3D*	indicesVolumeTarget_;

	// Generated buffers for rendering
	IVertexLayout *vertexLayout_;
	std::vector<IVertexBuffer*>	vertexBuffers_;
	std::vector<IIndexBuffer*>	indexBuffers_;

	std::vector<Volume::ShardFile::Id> ids_;

	int						nonEmptyCellCount_;
	int						primitiveCount_;
	int						indexCount_;
};

NIV_IMPLEMENT_CLASS(SampleApplication, TypeInfo::Default, AppMarchingCubes)

	int main(int argc, char* argv[]) 
{
	CoreLifeTimeHelper clth;
	try {
		CommandlineParser clp;
		clp.Add (CommandlineOption ("shard", VariantType::String))
			.Add (CommandlineOption ("size", VariantType::Integer))
			.Add (CommandlineOption ("iso-value", Variant (0.5f)));

		const auto options = clp.Parse(argc, argv);

		if (!options.IsSet ("shard")) {
			Log::Error ("SampleMarchingCubes", "No shard file specified. Use -shard <name> "
				"to set the shard file");
			return 1;
		}

		SampleApplication sa (options.Get<String> ("volume"));
		sa.Init();
		sa.Run();
		sa.Shutdown();
	} catch (Exception& e) {
		std::cerr << e.what() << std::endl;
		std::cerr << e.where() << std::endl;
	} catch (std::exception& e) {
		std::cerr << e.what() << std::endl;
	}

	return 0;
}
