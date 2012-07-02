#pragma once

#include "niven.Volume.FileBlockStorage.h"
#include "niven.Volume.MarchingCubes.h"
#include "niven.Volume.Volume.h"

#include "niven.Core.MemoryLayout.h"
#include "niven.Core.Iterator3D.h"

#include "niven.Core.Math.Vector.h"
#include "niven.Core.Math.VectorFunctions.h"

#include "niven.Core.ArrayRef.h"

#include "utility.h"

namespace niven {
	// assuming even blockResolution
	struct MipVolume {
		Volume::IBlockStorage &container;

		MemoryLayout3D layout;
		int blockResolution;
		int borderSize;
		int totalSize;
		int blockVoxelCount;

		struct LevelInfo {
			niven::Vector3i min, size;
			int voxelSize;
		};

		std::vector<LevelInfo> levels;

		MipVolume( Volume::IBlockStorage &container ) : container( container ) {
			Init();
		}
	
		void Init() {
			Volume::FileBlockStorage::LayerDescriptor layerDescriptor = container.GetLayerDescriptor( "Density" );

			blockResolution = layerDescriptor.blockResolution;
			borderSize = layerDescriptor.borderSize;
			totalSize = blockResolution + borderSize*2;
			blockVoxelCount = totalSize * totalSize * totalSize;

			layout = MemoryLayout3D( totalSize );

			Volume::IBlockStorage::BlockIdRange blockIdRange = container.GetBlockIdRange();
			LevelInfo level = { blockIdRange.min, blockIdRange.size, 1 };
			levels.push_back(level);

			const int totalVoxelCount = MaxElement( level.size ) * blockResolution;
			while( totalVoxelCount >= level.voxelSize ) {			
				level.voxelSize <<= 1;

				level.min = VectorFloor(level.min.Cast<double>() / 2.0).Cast<int>();
				// level.size > 0 (otherwise totalVoxelCount = 0..)
				niven::Vector3i levelMax = VectorCeil( (level.min + level.size).Cast<double>() / 2.0 ).Cast<int>();

				level.size = levelMax - level.min;
				levels.push_back(level);
			}

			GenerateMipmaps();
		}

		bool HasBlock( int level, const Vector3i &position ) {
			return container.ContainsBlock( "Density", position, level );
		}

		void GetBlock( int level, const Vector3i &position, const MutableArrayRef<uint16> &buffer ) {
			container.GetBlock( "Density", position, level, buffer );
		}

		void AddBlock( int level, const Vector3i &position, const ArrayRef<uint16> &buffer ) {
			container.AddBlock( "Density", position, level, buffer );
		}

		uint16 GetVoxel( const ArrayRef<uint16> &buffer, const Vector3i &position ) {
			return buffer[ layout( position + Vector3i::Constant( borderSize ) ) ];
		}

		uint16 & Voxel( const MutableArrayRef<uint16> &buffer, const Vector3i &position ) {
			return buffer.GetData()[ layout( position + Vector3i::Constant( borderSize ) ) ];
		}

		void SplitCoordinates( const Vector3i &position, Vector3i &blockIndex, Vector3i &localPosition ) {
			blockIndex = VectorFloor( position.Cast<double>() / blockResolution ).Cast<int>();
			localPosition = position - blockIndex * blockResolution;
		}

		Vector3i GetPositionInLevel( const Vector3i &position, int level ) {
			return VectorFloor( position.Cast<double>() / levels[level].voxelSize ).Cast<int>();
		}

		void GetCubeForMippedVoxel( int level, const Vector3i &position, Vector3i &min, Vector3i &max ) {
			int voxelSize = levels[level].voxelSize;
			min = position * voxelSize;
			max = min + Vector3i::Constant( voxelSize );
		}

		void GenerateMipmaps() {
			// to generate level:
			//	 access level - 1
			//	xRange_level_size = (xRange_{level-1}_size+1) / 2
			//	yRange_level_size and zRange_level_size likewise
			//	for x = xRange_{level-1}_min to xRange_{level-1}_max step 2
			//		for y = yRange_{level-1}_min to yRange_{level-1}_max step 2
			//			for z = zRange_{level-1}_min to zRange_{level-1}_max step 2
			//				grab all 8 blocks
			//				if all empty
			//					dont create a block
			//				otherwise
			//					iterate over all voxels and merge groups of 8
			//					add new block

			for( int level = 1 ; level < levels.size() ; level++ ) {
				Iterator3D targetIterator( levels[level].min, levels[level].size );
				for( ; !targetIterator.IsAtEnd() ; targetIterator++ ) {
					// only create new mipmap levels
					if( HasBlock( level, targetIterator.ToVector() ) ) {
						continue;
					}

					Vector3i srcBlockOffset = targetIterator.ToVector() * 2;

					bool emptyBlock = true;
					for( int i = 0 ; i < 8 ; i++ ) {
						if( HasBlock( level - 1, srcBlockOffset + indexToCubeCorner[ i ] ) ) {
							emptyBlock = false;
							break;
						}
					}
					if( emptyBlock ) {
						continue;
					}
					// TODO: deal with borders!
					// TODO: store the final mipmaps in one block?
					std::vector<uint16> blockData( blockVoxelCount );
					std::vector<uint16> srcBlockData( blockVoxelCount );
					for( int i = 0 ; i < 8 ; i++ ) {
						const Vector3i srcBlock = srcBlockOffset + indexToCubeCorner[ i ];

						if( !HasBlock( level - 1, srcBlock ) ) {
							continue;
						}				
						GetBlock( level - 1, srcBlock, srcBlockData );

						const Vector3i targetVoxelOffset = indexToCubeCorner[ i ] * (blockResolution / 2);

						Iterator3D voxelIterator( Vector3i::Constant( 0 ), Vector3i::Constant( blockResolution / 2 ) );
						for( ; !voxelIterator.IsAtEnd() ; voxelIterator++ ) {
							const Vector3i srcOffset = voxelIterator.ToVector() * 2;					

							uint16 value = 0;
							for( int voxelIndex = 0 ; voxelIndex < 8 ; voxelIndex++ ) {
								value = std::max( value, GetVoxel( srcBlockData, srcOffset + indexToCubeCorner[ voxelIndex ] ) );
							}

							const Vector3i targetVoxelPosition = targetVoxelOffset + voxelIterator.ToVector();
							Voxel( blockData, targetVoxelPosition ) = value;
						}
					}

					AddBlock( level, targetIterator.ToVector(), blockData );
					std::cout << targetIterator.ToVector().X() << " " << targetIterator.ToVector().Y() << " " << targetIterator.ToVector().Z() << "\n";
				}
			}
		}

	};
}
