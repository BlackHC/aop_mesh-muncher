#pragma once

#include <boost/format.hpp>
#include <vector>
#include <boost/algorithm/string/join.hpp>

#include <optix_world.h>
#include "optixProgramInterface.h"

namespace OptixHelpers {
	namespace Buffer {
		// TODO: use arrayref
		// v is presized
		template< typename T >
		void copyToHost( optix::Buffer &buffer, T &first, int count ) {
			const T *start = (const T *) buffer->map();
			std::copy( start, start + count, &first );
			buffer->unmap();
		}

		// TODO: use arrayref
		template< typename T >
		void copyToDevice( optix::Buffer &buffer, const std::vector< T > &v ) {
			boost::range::copy( v, (T *) buffer->map() );
			buffer->unmap();
		}
	}

	namespace Namespace {
		typedef std::vector< std::string > Modules;

		inline Modules makeModules( const char *filename ) {
			Modules modules;
			modules.push_back( filename );
			return modules;
		}

		inline Modules makeModules( const char *filenameA, const char *filenameB ) {
			Modules modules;
			modules.push_back( filenameA );
			modules.push_back( filenameB );
			return modules;
		}

		inline Modules makeModules( const char *filenameA, const char *filenameB, const char *filenameC ) {
			Modules modules;
			modules.push_back( filenameA );
			modules.push_back( filenameB );
			modules.push_back( filenameC );
			return modules;
		}

		inline std::string getRayProgramName( unsigned rayType, const char *name ) {
			return ::boost::str( ::boost::format( "%s_%s" ) % OptixProgramInterface::rayTypeNamespaces[ rayType ] % name );
		}

		// supports namespaces
		inline std::string getMaterialProgramName( const std::string &materialName, unsigned rayType, const char *name ) {
			if( !materialName.empty() ) {
				return ::boost::str( ::boost::format( "%s::%s_%s" ) % materialName % OptixProgramInterface::rayTypeNamespaces[ rayType ] % name );
			}
			else {
				return ::boost::str( ::boost::format( "%s_%s" ) % OptixProgramInterface::rayTypeNamespaces[ rayType ] % name );
			}
		}

		// supports optional namespaces
		inline std::string getGeometryProgramName( const std::string &geometryName, const char *name ) {
			if( !geometryName.empty() ) {
				return ::boost::str( ::boost::format( "%s::%s" ) % geometryName % name );
			}
			else {
				return ::boost::str( ::boost::format( "%s" ) % name );
			}
		}

		inline std::string getEntryPointProgramName( unsigned entryPoint, const char *name ) {
			return ::boost::str( ::boost::format( "%s_%s" ) % OptixProgramInterface::entryPointNamespaces[ entryPoint ] % name );
		}

		inline bool getProgram( optix::Context &context, optix::Program &program, const Modules &modules, const std::string fullName ) {
			for( int moduleIndex = 0 ; moduleIndex < modules.size() ; moduleIndex++ ) {
				RTprogram innerProgram;

				RTresult result = rtProgramCreateFromPTXFile( context->get(), modules[ moduleIndex ].c_str(), fullName.c_str(), &innerProgram );

				// function not found?
				if( result == RT_ERROR_INVALID_SOURCE ) {
					continue;
				}
				else if( result == RT_SUCCESS ) {
					std::cout << boost::format( "%s: found '%s'\n" ) % modules[ moduleIndex ] % fullName;
					program = optix::Program::take( innerProgram );
					return true;
				}
				else {
					context->checkError( result );
				}
			}

			std::cout << boost::format( "'%s' not found in %s\n" ) % fullName % boost::algorithm::join( modules, ", " );
			return false;
		}

#define create_setMaterialProgram( name, ptxName ) \
	inline void set ##name ##Programs( optix::Material &material, const Modules &modules, const std::string &materialName ) { \
	for( unsigned int rayType = 0 ; rayType < OptixProgramInterface::RT_COUNT ; ++rayType ) { \
	optix::Program program; \
	if( getProgram( material->getContext(), program, modules, getMaterialProgramName( materialName, rayType, #ptxName ) ) ) { \
	material->set ##name ##Program( (unsigned) rayType, program ); \
	} \
	} \
		}

		create_setMaterialProgram( AnyHit, anyHit )
			create_setMaterialProgram( ClosestHit, closestHit )
#undef create_setMaterialProgram

			inline void setMaterialPrograms( optix::Material &material, const Modules &modules, const std::string &materialName ) {
				setAnyHitPrograms( material, modules, materialName );
				setClosestHitPrograms( material, modules, materialName );
		}

		inline void setMissPrograms( optix::Context &context, const Modules &modules ) {
			for( unsigned int rayType = 0 ; rayType < OptixProgramInterface::RT_COUNT ; ++rayType ) {
				optix::Program program;
				if( getProgram( context, program, modules, getRayProgramName( rayType, "miss" ) ) ) {
					context->setMissProgram( (unsigned) rayType, program );
				}
			}
		}

		inline void setIntersectionProgram( optix::Geometry &geometry, const Modules &modules, const std::string &geometryName ) {
			optix::Program program;
			if( getProgram( geometry->getContext(), program, modules, getGeometryProgramName( geometryName, "intersect" ) ) ) {
				geometry->setIntersectionProgram( program );
			}
		}

		inline void setBoundingBoxProgram( optix::Geometry &geometry, const Modules &modules, const std::string &geometryName ) {
			optix::Program program;
			if( getProgram( geometry->getContext(), program, modules, getGeometryProgramName( geometryName, "calculateBoundingBox" ) ) ) {
				geometry->setBoundingBoxProgram( program );
			}
		}

		inline void setGeometryPrograms( optix::Geometry &geometry, const Modules &modules, const std::string &geometryName ) {
			setIntersectionProgram( geometry, modules, geometryName );
			setBoundingBoxProgram( geometry, modules, geometryName );
		}

		inline void setRayGenerationPrograms( optix::Context &context, const Modules &modules ) {
			for( unsigned int entryPoint = 0 ; entryPoint < OptixProgramInterface::EP_COUNT ; ++entryPoint ) {
				optix::Program program;
				if( getProgram( context, program, modules, OptixProgramInterface::entryPointNamespaces[ entryPoint ] ) ) {
					context->setRayGenerationProgram( entryPoint, program );
				}
			}
		}

		inline void setExceptionPrograms( optix::Context &context, const Modules &modules ) {
			for( unsigned int entryPoint = 0 ; entryPoint < OptixProgramInterface::EP_COUNT ; ++entryPoint ) {
				optix::Program program;
				if( getProgram( context, program, modules, getEntryPointProgramName( entryPoint, "exception" ) ) ) {
					context->setExceptionProgram( entryPoint, program );
				}
			}
		}

		inline void setEntryPointPrograms( optix::Context &context, const Modules &modules ) {
			setRayGenerationPrograms( context, modules );
			setExceptionPrograms( context, modules );
		}
	}
}