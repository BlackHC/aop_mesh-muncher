#pragma once

#include "gl/glew.h"

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

	void end() {
		glEndList();
	}

	void call() const {
		glCallList( handle );
	}

	void release() {
		if( handle ) {
			glDeleteLists( handle, 1 );
			handle = 0;
		}
	}
};

struct ScopedDisplayList : DisplayList {
	static GLuint createDisplayList() {
		GLuint handle = glGenLists( 1 );
		return handle;
	}

	ScopedDisplayList() : DisplayList( createDisplayList() ) {}
	~ScopedDisplayList() {
		release();
	}
};

struct Texture {
	GLuint handle;

	Texture( GLuint handle = 0 ) : handle( handle ) {}
};

template< class SpecializedTexture = Texture >
struct ScopedTexture : SpecializedTexture {
	static GLuint createTextureHandle() {
		GLuint handle;
		glGenTextures( 1, &handle );
		return handle;
	}

	ScopedTexture() : SpecializedTexture( createTextureHandle() ) {}
	~ScopedTexture() {
		if( handle ) {
			glDeleteTextures( 1, &handle );
		}
	}

	SpecializedTexture publish() {
		SpecializedTexture globalTexture( handle );
		handle = 0;
		return globalTexture;
	}
};

struct Texture2D {
	GLuint handle;

	Texture2D( GLuint handle = 0 ) : handle( handle ) {}

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

}