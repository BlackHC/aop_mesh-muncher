#pragma once

#include "sgsScene.h"

#include "gl/glew.h"
struct SGSSceneRenderer {
	GLuint displayList;

	void processScene( const SGSScene &scene ) {
		displayList = glGenLists( 1 );

		glNewList( displayList, GL_COMPILE );

		glPushClientAttrib( GL_CLIENT_ALL_ATTRIB_BITS );

		glEnableClientState( GL_VERTEX_ARRAY );
		glEnableClientState( GL_NORMAL_ARRAY );

		glVertexPointer( 3, GL_FLOAT, sizeof( SGSScene::Vertex ), scene.vertices[0].position );
		glNormalPointer( GL_FLOAT, sizeof( SGSScene::Vertex ), scene.vertices[0].normal );

		glColor3f( 0.8, 0.8, 0.8 );

		glDrawElements( GL_TRIANGLES, scene.indices.size(), GL_UNSIGNED_INT, &scene.indices.front() );

		glPopClientAttrib();

		glEndList();
	}

	void render() {
		glCallList( displayList );
	}

	SGSSceneRenderer() : displayList( 0 ) {}
	~SGSSceneRenderer() {
		if( displayList )
			glDeleteLists( displayList, 1 );
	}
};
