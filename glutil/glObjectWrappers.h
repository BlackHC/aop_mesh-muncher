#pragma once

#include "gl/glew.h"
#include <vector>

//////////////////////////////////////////////////////////////////////////

namespace GL {

struct Renderbuffer {
	GLuint handle;

	Renderbuffer( GLenum internalFormat, int width, int height ) {
		glGenRenderbuffers( 1, &handle );
		glNamedRenderbufferStorageEXT( handle, internalFormat, width, height );
	}

	~Renderbuffer() {
		glDeleteRenderbuffers( 1, &handle );
	}
};

struct DisplayList {
	GLuint handle;

	DisplayList( GLuint handle = 0 ) : handle( handle ) {}

	void begin() {
		glNewList( handle, GL_COMPILE );
	}

	static void end() {
		glEndList();
	}

	void call() const {
		if( handle ) {
			glCallList( handle );
		}
	}

	void create() {
		release();
		handle = glGenLists( 1 );
	}

	void release() {
		if( handle ) {
			glDeleteLists( handle, 1 );
			handle = 0;
		}
	}
};

struct Texture {
	GLuint handle;

	Texture( GLuint handle = 0 ) : handle( handle ) {}

	void create() {
		glGenTextures( 1, &handle );
	}

	void release() {
		if( handle ) {
			glDeleteTextures( 1, &handle );
		}
	}

	static void createMultiple( int count, GLuint *handles ) {
		glGenTextures( count, handles );
	}

	static void releaseMultiple( int count, GLuint *handles ) {
		glDeleteTextures( count, handles );
	}
};

// TODO: remove this!
template< class SpecializedTexture = Texture >
struct ScopedTexture : SpecializedTexture {

	ScopedTexture() {
		SpecializedTexture::create();
	}
	~ScopedTexture() {
		SpecializedTexture::release();
	}

	SpecializedTexture publish() {
		SpecializedTexture globalTexture( handle );
		handle = 0;
		return globalTexture;
	}
};

struct Texture2D : Texture {
	Texture2D( GLuint handle = 0 ) : Texture( handle ) {}

	void bind() const {
		glBindTexture( GL_TEXTURE_2D, handle );
	}

	static void unbind() {
		glBindTexture( GL_TEXTURE_2D, 0 );
	}

	void load( GLint level, GLint internalformat, GLsizei width, GLsizei height, GLint border, GLenum format, GLenum type, const GLvoid *pixels = nullptr ) {
		glTextureImage2DEXT( handle, GL_TEXTURE_2D, level, internalformat, width, height, border, format, type, pixels );
	}

	// there is no ext extension for it, so make it static instead
	static void immutable( int numLevels, GLenum internalFormat, GLsizei width, GLsizei height ) {
		glTexStorage2D( GL_TEXTURE_2D, numLevels, internalFormat, width, height );
	}

	static void enable() {
		glEnable( GL_TEXTURE_2D );
	}

	static void disable() {
		glDisable( GL_TEXTURE_2D );
	}

	void generateMipmap() const {
		glGenerateTextureMipmapEXT( handle, GL_TEXTURE_2D );
	}

	static void bindExtern( GLuint handle ) {
		glBindTexture( GL_TEXTURE_2D, handle );
	}

	void parameter( GLenum pname, int param ) const {
		glTextureParameteriEXT( handle, GL_TEXTURE_2D, pname, param );
	}

	void parameter( GLenum pname, float param ) const {
		glTextureParameterfEXT( handle, GL_TEXTURE_2D, pname, param );
	}

	void parameter( GLenum pname, int *param ) const {
		glTextureParameterivEXT( handle, GL_TEXTURE_2D, pname, param );
	}

	void parameter( GLenum pname, float *param ) const {
		glTextureParameterfvEXT( handle, GL_TEXTURE_2D, pname, param );
	}

	void getLevelParameter( int lod, GLenum value, int *data ) const {
		glGetTextureLevelParameterivEXT( handle, GL_TEXTURE_2D, lod, value, data );
	}

	void getImage( int lod, GLenum format, GLenum type, void *img ) const {
		glGetTextureImageEXT( handle, GL_TEXTURE_2D, lod, format, type, img );
	}
};

template< class SpecializedTexture = Texture >
struct Textures {
	std::vector<GLuint> handles;

	Textures( int size = 0 ) : handles( size ) {}

	SpecializedTexture get( int index ) {
		return SpecializedTexture( handles[ index ] );
	}

	SpecializedTexture operator []( int index ) {
		return get( index );
	}

	const SpecializedTexture get( int index ) const {
		return SpecializedTexture( handles[ index ] );
	}

	const SpecializedTexture operator []( int index ) const {
		return get( index );
	}
};

template< class SpecializedTexture = Texture >
struct ScopedTextures : Textures< SpecializedTexture > {
	ScopedTextures( int size = 0 ) : Textures( size ) {
		if( size ) {
			glGenTextures( size, &handles.front() );
		}
	}

	~ScopedTextures() {
		if( handles.size() ) {
			glDeleteTextures( (GLsizei) handles.size(), &handles.front() );
		}
	}

	void resize( int size ) {
		if( handles.empty() ) {
			handles.resize( size );
			glGenTextures( size, &handles.front() );
		}
		else {
			throw std::logic_error( "ScopedTextures not zero!" );
		}
	}

	SpecializedTexture publish( int index ) {
		SpecializedTexture globalTexture( handles[ index ] );
		handles[ index ] = 0;
		return globalTexture;
	}

	Textures<SpecializedTexture> publish() {
		Textures<SpecializedTexture> globalTextures;

		std::swap( globalTextures.handles, handles );

		return globalTextures;
	}
};

typedef ScopedTexture< Texture2D > ScopedTexture2D;
typedef ScopedTextures< Texture2D > ScopedTextures2D;

typedef Textures< Texture2D > Textures2D;

struct ScopedFramebufferObject {
	GLuint handle;
	bool colorAttachment0;
	bool depthAttachment;

	ScopedFramebufferObject() : depthAttachment( false ), colorAttachment0( false ) {
		glGenFramebuffers( 1, &handle );
	}

	~ScopedFramebufferObject() {
		if( handle ) {
			glDeleteFramebuffers( 1, &handle );
		}
	}

	void bind() const  {
		glBindFramebuffer( GL_FRAMEBUFFER, handle );
	}

	static void unbind() {
		glBindFramebuffer( GL_FRAMEBUFFER, 0 );
	}

	void attach( const Renderbuffer &renderbuffer, GLenum attachment ) {
		glNamedFramebufferRenderbufferEXT( handle, attachment, GL_RENDERBUFFER, renderbuffer.handle );
		changeAttachmentState( attachment, true );
	}

	void attach( const Texture2D &texture, GLenum attachment, int level = 0 ) {
		glNamedFramebufferTexture2DEXT( handle, attachment, GL_TEXTURE_2D, texture.handle, level );
		changeAttachmentState( attachment, true );
	}

	void detach( GLenum attachment ) {
		glNamedFramebufferRenderbufferEXT( handle, attachment, GL_RENDERBUFFER, 0 );
		changeAttachmentState( attachment, false );
	}

	void changeAttachmentState( GLenum attachment, bool state ) {
		switch( attachment ) {
		case GL_DEPTH_ATTACHMENT:
			depthAttachment = state;
			break;
		case GL_COLOR_ATTACHMENT0:
			colorAttachment0 = state;
			break;
		}
	}

	void setDrawBuffers() const {
		GLenum drawBuffers[1];
		int i = 0;

		if( colorAttachment0 ) {
			drawBuffers[i++] = GL_COLOR_ATTACHMENT0;
		}

		if( i > 0 ) {
			glDrawBuffers( i, drawBuffers );
		}
		else {
			glDrawBuffer( GL_NONE );
		}
	}

	static void resetDrawBuffers() {
		glDrawBuffer( GL_BACK );
	}
};

struct Buffer {
	GLuint handle;

	Buffer( GLuint handle = 0 ) : handle( handle ) {}

	void bind( GLenum target ) {
		glBindBuffer( target, handle );
	}

	static void unbind( GLenum target ) {
		glBindBuffer( target, 0 );
	}

	void bufferData( GLsizei size, const void *data, GLenum usage ) {
		glNamedBufferDataEXT( handle, size, data, usage );
	}

	void bufferSubData( GLintptr offset, GLsizei size, const void *data ) {
		glNamedBufferSubDataEXT( handle, offset, size, data );
	}

	void create() {
		glGenBuffers( 1, &handle );
	}

	void release() {
		if( handle ) {
			glDeleteBuffers( 1, &handle );
			handle = 0;
		}
	}
};

struct VertexArrayObject {
	GLuint handle;

	VertexArrayObject( GLuint handle = 0 ) : handle( handle ) {}

	void create() {
		glGenVertexArrays( 1, &handle );
	}

	void release() {
		glDeleteVertexArrays( 1, &handle );
		handle = 0;
	}

	static void createMultiple( int count, GLuint *handles ) {
		glGenVertexArrays( count, handles );
	}

	static void releaseMultiple( int count, GLuint *handles ) {
		glDeleteVertexArrays( count, handles );
	}

	void bind() {
		glBindVertexArray( handle );
	}

	static void unbind() {
		glBindVertexArray( 0 );
	}
};

template< typename Base >
struct Scoped : Base {
	Scoped() {
		Base::create();
	}
	~Scoped() {
		Base::release();
	}

	Base publish() {
		Base published( handle );
		handle = 0;
		return published;
	}
};

typedef Scoped<DisplayList> ScopedDisplayList;
typedef Scoped<VertexArrayObject> ScopedVertexArrayObject;
typedef Scoped<Buffer> ScopedBuffer;

template< typename Base >
struct Array {
	std::vector<GLuint> handles;

	Array( int size = 0 ) : handles( size ) {}

	Base get( int index ) {
		return Base( handles[ index ] );
	}

	Base operator []( int index ) {
		return get( index );
	}
};

template< typename Base >
struct ScopedArray : Array< Base > {
	ScopedArray( int size = 0 ) : Array( size ) {
		if( size ) {
			Base::createMultiple( size, &handles.front() );
		}
	}

	~ScopedArray() {
		if( handles.size() ) {
			Base::releaseMultiple( (int) handles.size(), &handles.front() );
		}
	}

	void resize( int size ) {
		if( handles.empty() ) {
			handles.resize( size );
			Base::createMultiple( size, &handles.front() );
		}
		else {
			throw std::logic_error( "ScopedTextures not zero!" );
		}
	}

	Base publish( int index ) {
		Base global( handles[ index ] );
		handles[ index ] = 0;
		return global;
	}

	Array<Base> publishAll() {
		Array<Base> global;

		std::swap( global.handles, handles );

		return global;
	}
};

template<>
struct Array< DisplayList > {
	GLuint handle;
	GLsizei count;

	Array() : handle( 0 ), count( 0 ) {}

	DisplayList get( int index ) {
		return DisplayList( handle + index );
	}

	DisplayList operator []( int index ) {
		return get( index );
	}
};

template<>
struct ScopedArray< DisplayList > : Array< DisplayList > {
	ScopedArray( int size = 0 ) {
		if( size ) {
			handle = glGenLists( size );
			count = size;
		}
	}

	~ScopedArray() {
		if( handle ) {
			glDeleteLists( handle, count );
		}
	}

	void resize( int size ) {
		if( !handle ) {
			handle = glGenLists( size );
			count = size;			
		}
		else {
			throw std::logic_error( "ScopedTextures not zero!" );
		}
	}

	Array<DisplayList> publishAll() {
		Array<DisplayList> global;

		global.handle = handle;
		global.count = handle;

		handle = 0;

		return global;
	}
};

typedef ScopedArray< DisplayList > ScopedDisplayLists;
}

namespace SimpleGL {
	using namespace GL;

	class ImmediateMultiDrawElements {
	public:
		bool empty() const {
			return counts.empty();
		}

		void reserve( size_t capacity ) {
			counts.reserve( capacity );
			indices.reserve( capacity );
		}

		void push_back( GLsizei count, const void *firstIndex ) {
			counts.push_back( count );
			indices.push_back( firstIndex );
		}

		size_t size() const {
			return counts.size();
		}

		void operator () ( GLenum mode, GLenum elementType ) {
			glMultiDrawElements( mode, &counts.front(), elementType, &indices.front(), (GLsizei) size() );
		}

	private:
		std::vector< GLsizei > counts;
		std::vector< const void * > indices;
	};

	inline void setRepeatST( Texture2D &texture ) {
		texture.parameter( GL_TEXTURE_WRAP_S, GL_REPEAT );
		texture.parameter( GL_TEXTURE_WRAP_T, GL_REPEAT );
	}

	inline void setClampToBorderST( Texture2D &texture ) {
		texture.parameter( GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER );
		texture.parameter( GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER );
	}

	inline void setLinearMinMag( Texture2D &texture ) {
		texture.parameter( GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		texture.parameter( GL_TEXTURE_MIN_FILTER, GL_LINEAR );
	}

	inline void setLinearMipmapMinMag( Texture2D &texture ) {
		texture.parameter( GL_TEXTURE_MAG_FILTER, GL_LINEAR );
		texture.parameter( GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR );
	}
}