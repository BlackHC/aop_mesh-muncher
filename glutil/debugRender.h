#pragma once

#include <vector>

#include <gl/glew.h>
#include <Eigen/Eigen>
#include <unsupported/Eigen/OpenGLSupport>

#include <eigenProjectionMatrices.h>

namespace DebugRender {
	//////////////////////////////////////////////////////////////////////////
	// old implementation
	
	struct ImmediateCalls {
		static void begin() {
			glMatrixMode( GL_MODELVIEW );
			glPushMatrix();
		}

		static void setPosition( const Eigen::Vector3f &position ) {
			glPopMatrix();
			glPushMatrix();
			Eigen::glTranslate( position );
		}

		static void setTransformation( const Eigen::Matrix4f &transformation ) {
			glPopMatrix();
			glPushMatrix();
			Eigen::glMultMatrix( transformation );
		}

		static void setColor( const Eigen::Vector3f &color ) {
			Eigen::glColor( color );
		}

		static void end() {
			glPopMatrix();
		}

		// from glut 3.7 and SFML's SimpleGLScene
		static void drawBox(const Eigen::Vector3f &size, bool wireframe = true, bool gay = false)
		{
			static GLfloat n[6][3] =
			{
				{-1.0, 0.0, 0.0},
				{0.0, 1.0, 0.0},
				{1.0, 0.0, 0.0},
				{0.0, -1.0, 0.0},
				{0.0, 0.0, 1.0},
				{0.0, 0.0, -1.0}
			};
			static GLint faces[6][4] =
			{
				{0, 1, 2, 3},
				{3, 2, 6, 7},
				{7, 6, 5, 4},
				{4, 5, 1, 0},
				{5, 6, 2, 1},
				{7, 4, 0, 3}
			};
			static GLfloat gayColors[6][3] = {
				{0.f, 1.f, 1.f},
				{0.f, 1.f, 0.f},
				{1.f, 0.f, 0.f},
				{1.f, 0.f, 1.f},
				{0.f, 0.f, 1.f},
				{1.f, 1.f, 0.f}
			};
			GLfloat v[8][3];
			GLint i;

			v[0][0] = v[1][0] = v[2][0] = v[3][0] = -size.x() / 2;
			v[4][0] = v[5][0] = v[6][0] = v[7][0] = size.x() / 2;
			v[0][1] = v[1][1] = v[4][1] = v[5][1] = -size.y() / 2;
			v[2][1] = v[3][1] = v[6][1] = v[7][1] = size.y() / 2;
			v[0][2] = v[3][2] = v[4][2] = v[7][2] = -size.z() / 2;
			v[1][2] = v[2][2] = v[5][2] = v[6][2] = size.z() / 2;

			if( !wireframe ) {
				glBegin(GL_QUADS);
				for (i = 5; i >= 0; i--) {
					if( gay ) {
						glColor3fv(&gayColors[i][0]);
					}
					glNormal3fv(&n[i][0]);
					glVertex3fv(&v[faces[i][0]][0]);
					glVertex3fv(&v[faces[i][1]][0]);
					glVertex3fv(&v[faces[i][2]][0]);
					glVertex3fv(&v[faces[i][3]][0]);
				}
				glEnd();
			}
			else {
				for (i = 5; i >= 0; i--) {
					glBegin(GL_LINE_LOOP);
					if( gay ) {
						glColor3fv(&gayColors[i][0]);
					}
					glNormal3fv(&n[i][0]);
					glVertex3fv(&v[faces[i][0]][0]);
					glVertex3fv(&v[faces[i][1]][0]);
					glVertex3fv(&v[faces[i][2]][0]);
					glVertex3fv(&v[faces[i][3]][0]);
					glEnd();
				}				
			}
		}

		static void drawAABB( const Eigen::Vector3f &minCorner, const Eigen::Vector3f &maxCorner, bool wireframe = true, bool gay = false ) {
			glPushMatrix();
			setPosition( (minCorner + maxCorner) / 2 );
			drawBox( (maxCorner - minCorner).cwiseAbs(), wireframe, gay );
			glPopMatrix();
		}

		static void drawCordinateSystem( float size ) {
			glBegin(GL_LINES);
			glColor3f( 1.0, 0.0, 0.0 );
			glVertex3f( 0.0, 0.0, 0.0 );
			glVertex3f( size, 0.0, 0.0 );

			glColor3f( 0.0, 1.0, 0.0 );
			glVertex3f( 0.0, 0.0, 0.0 );
			glVertex3f( 0.0, size, 0.0 );

			glColor3f( 0.0, 0.0, 1.0 );
			glVertex3f( 0.0, 0.0, 0.0 );
			glVertex3f( 0.0, 0.0, size );
			glEnd();
		}

		static void drawEllipse( float radius, bool wireframe = true, int n = 20, const Eigen::Vector3f &axis1 = Eigen::Vector3f::UnitX(), const Eigen::Vector3f &axis2 = Eigen::Vector3f::UnitY() ) {
			const float step = float( 2 * M_PI / n );
			glBegin( wireframe ? GL_LINE_LOOP : GL_TRIANGLE_FAN );
			for( int i = 0 ; i < n ; i++ ) {
				Eigen::glVertex( radius * (axis1 * cos(step * i) + axis2 * sin(step * i)) );
			}
			glEnd();
		}

		static void drawAbstractSphere( float radius, bool wireframe = true, int n = 20 ) {
			drawEllipse( radius, wireframe, n, Eigen::Vector3f::UnitX(), Eigen::Vector3f::UnitY() );
			drawEllipse( radius, wireframe, n, Eigen::Vector3f::UnitY(), Eigen::Vector3f::UnitZ() );
			drawEllipse( radius, wireframe, n, Eigen::Vector3f::UnitX(), Eigen::Vector3f::UnitZ() );
		}

		static void drawVector( const Eigen::Vector3f &direction ) {
			glBegin( GL_LINES );
			glVertex3f( 0.0, 0.0, 0.0 );
			Eigen::glVertex( direction );
			glEnd();
		}

		static void drawVectorCone( const Eigen::Vector3f &direction, const float radius, int n = 10 ) {
			const Eigen::Matrix3f onb = Eigen::OrthonormalBase::fromNormal( direction );
			const float step = float( 2 * M_PI / n );

			glBegin( GL_LINES );
			for( int i = 0 ; i < n ; i++ ) {
				glVertex3f( 0.0, 0.0, 0.0 );
				Eigen::glVertex( direction + radius * (onb.col(1) * cos( step * i ) + onb.col(2) * sin(step * i)) );
			}
			glEnd();
		}

		static void drawLine( const Eigen::Vector3f &start, const Eigen::Vector3f &end ) {
			glBegin( GL_LINES );
			Eigen::glVertex( start );
			Eigen::glVertex( end );
			glEnd();
		}

		static void drawWireframeSphere( float radius, int n = 20, int slices = 5 ) {
			const float step = 2 * radius / (slices + 2);

			glPushMatrix();
			for( int axis = 0 ; axis < 3 ; ++axis ) {								
				for( int i = 0 ; i < slices ; ++i ) {
					const float z = step * (i+1) - radius;
					const float subRadius = sqrt( radius * radius - z*z );

					setPosition( Eigen::Vector3f::Unit(axis) * z );
					drawEllipse( subRadius, true, n, Eigen::Vector3f::Unit( (axis + 1) % 3 ), Eigen::Vector3f::Unit( (axis + 2) % 3 ) );
				}
			}
			glPopMatrix();
		}

		static void drawSolidSphere( float radius, int u = 10, int v = 20 ) {
			glBegin( GL_QUADS );
			for( int i = 0 ; i < u ; i++ ) {
				for( int j = 0 ; j < v ; j++ ) {
					glColor3f( float(i) / (u-1), float(j) / (v-1), 0.0 );
					// cos | sin | cos
					// cos | 1   | sin
#define U(x) float((x)*M_PI/u - M_PI / 2)
#define V(x) float((x)*2*M_PI/v)
#define P(u_r,v_r) (Eigen::Vector3f( cos(U(u_r)) * cos(V(v_r)), sin(U(u_r)), cos(U(u_r)) * sin(V(v_r)) ) * radius)
					Eigen::glVertex( P(i,j) );
					Eigen::glVertex( P(i+1,j) );
					Eigen::glVertex( P(i+1,j+1) );
					Eigen::glVertex( P(i,j+1) );
#undef U
#undef V
#undef P
				}
			}
			glEnd();
		}

		static void drawTexturedScreenQuad() {			
			glBegin( GL_QUADS );
				glMultiTexCoord2f( GL_TEXTURE0, 0.0, 0.0 );
				glVertex3f( -1.0, -1.0, 0.0 );

				glMultiTexCoord2f( GL_TEXTURE0, 1.0, 0.0 );
				glVertex3f( 1.0, -1.0, 0.0 );

				glMultiTexCoord2f( GL_TEXTURE0, 1.0, 1.0 );
				glVertex3f( 1.0, 1.0, 0.0 );

				glMultiTexCoord2f( GL_TEXTURE0, 0.0, 1.0 );
				glVertex3f( -1.0, 1.0, 0.0 );
			glEnd();
		}
	};

	struct CombinedCalls : ImmediateCalls {
		GLuint list;

		CombinedCalls() : list( 0 ) {}
		~CombinedCalls() {
			clear();
		}

		void clear() {
			if( list ) {
				glDeleteLists( list, 1 );
				list = 0;
			}
		}

		void begin() {
			clear();

			list = glGenLists( 1 );

			glNewList( list, GL_COMPILE );
			
			ImmediateCalls::begin();
		}

		void end() {
			ImmediateCalls::end();

			glEndList();
		}

		void append() {
			// TODO: this breaks destruction...
			GLuint oldList = list;
			
			list = glGenLists( 1 );

			glNewList( list, GL_COMPILE );
			
			if( oldList )
				glCallList( oldList );

			ImmediateCalls::begin();
		}

		void render() {
			if( list )
				glCallList( list );
		}
	};
}

namespace DebugRender {
	//////////////////////////////////////////////////////////////////////////
	// new implementation that is aware that it is using display lists and OGL [9/26/2012 kirschan2]
	// this is more honest than making all functions static
	
	// correctly handles appending to lists (no resource leaks)
	struct DisplayList {
		DisplayList() {}
		~DisplayList() {
			clear();
		}

		void clear() {
			for( auto list = lists.begin() ; list != lists.end() ; ++list ) {
				glDeleteLists( *list, 1 );
			}
			lists.clear();
		}

		void beginCompile() {
			clear();

			lists.push_back( glGenLists( 1 ) );

			glNewList( lists.back(), GL_COMPILE );
		}

		void endCompile() {
			glEndList();
		}

		void beginCompileAndAppend() {
			int previousList = 0;
			if( !lists.empty() ) {
				previousList = lists.back();
			}

			lists.push_back( glGenLists( 1 ) );

			glNewList( lists.back(), GL_COMPILE );

			if( previousList ) {
				glCallList( previousList );
			}
		}

		void render() {
			if( !lists.empty() )
				glCallList( lists.back() );
		}

	private:
		std::vector<GLuint> lists;
	};

	static void begin() {
		glMatrixMode( GL_MODELVIEW );
		glPushMatrix();
	}

	inline void setPosition( const Eigen::Vector3f &position ) {
		glPopMatrix();
		glPushMatrix();
		Eigen::glTranslate( position );
	}

	inline void startLocalTransform() {
		glPushMatrix();
	}

	inline void endLocalTransform() {
		glPopMatrix();
	}

	inline void setTransformation( const Eigen::Matrix4f &transformation ) {
		glPopMatrix();
		glPushMatrix();
		Eigen::glMultMatrix( transformation );
	}

	inline void setTransformation( const Eigen::Affine3f &transformation ) {
		glPopMatrix();
		glPushMatrix();
		Eigen::glMultMatrix( transformation );
	}

	inline void setColor( const Eigen::Vector3f &color ) {
		Eigen::glColor( color );
	}

	inline void end() {
		glPopMatrix();
	}

	inline void onlyDrawOccluded() {
		glDepthMask( GL_FALSE );
		glDepthFunc( GL_GEQUAL );
	}

	inline void onlyDrawUnoccluded() {
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_LEQUAL );
	}

	inline void alwaysDraw() {
		glDepthMask( GL_TRUE );
		glDepthFunc( GL_ALWAYS );
	}

	// from glut 3.7 and SFML's SimpleGLScene
	inline void drawBox(const Eigen::Vector3f &size, bool wireframe = true, bool gay = false)
	{
		static GLfloat n[6][3] =
		{
			{-1.0, 0.0, 0.0},
			{0.0, 1.0, 0.0},
			{1.0, 0.0, 0.0},
			{0.0, -1.0, 0.0},
			{0.0, 0.0, 1.0},
			{0.0, 0.0, -1.0}
		};
		static GLint faces[6][4] =
		{
			{0, 1, 2, 3},
			{3, 2, 6, 7},
			{7, 6, 5, 4},
			{4, 5, 1, 0},
			{5, 6, 2, 1},
			{7, 4, 0, 3}
		};
		static GLfloat gayColors[6][3] = {
			{0.f, 1.f, 1.f},
			{0.f, 1.f, 0.f},
			{1.f, 0.f, 0.f},
			{1.f, 0.f, 1.f},
			{0.f, 0.f, 1.f},
			{1.f, 1.f, 0.f}
		};
		GLfloat v[8][3];
		GLint i;

		v[0][0] = v[1][0] = v[2][0] = v[3][0] = -size.x() / 2;
		v[4][0] = v[5][0] = v[6][0] = v[7][0] = size.x() / 2;
		v[0][1] = v[1][1] = v[4][1] = v[5][1] = -size.y() / 2;
		v[2][1] = v[3][1] = v[6][1] = v[7][1] = size.y() / 2;
		v[0][2] = v[3][2] = v[4][2] = v[7][2] = -size.z() / 2;
		v[1][2] = v[2][2] = v[5][2] = v[6][2] = size.z() / 2;

		if( !wireframe ) {
			glBegin(GL_QUADS);
			for (i = 5; i >= 0; i--) {
				if( gay ) {
					glColor3fv(&gayColors[i][0]);
				}
				glNormal3fv(&n[i][0]);
				glVertex3fv(&v[faces[i][0]][0]);
				glVertex3fv(&v[faces[i][1]][0]);
				glVertex3fv(&v[faces[i][2]][0]);
				glVertex3fv(&v[faces[i][3]][0]);
			}
			glEnd();
		}
		else {
			for (i = 5; i >= 0; i--) {
				glBegin(GL_LINE_LOOP);
				if( gay ) {
					glColor3fv(&gayColors[i][0]);
				}
				glNormal3fv(&n[i][0]);
				glVertex3fv(&v[faces[i][0]][0]);
				glVertex3fv(&v[faces[i][1]][0]);
				glVertex3fv(&v[faces[i][2]][0]);
				glVertex3fv(&v[faces[i][3]][0]);
				glEnd();
			}				
		}
	}

	inline void drawAABB( const Eigen::Vector3f &minCorner, const Eigen::Vector3f &maxCorner, bool wireframe = true, bool gay = false ) {
		glPushMatrix();
		setPosition( (minCorner + maxCorner) / 2 );
		drawBox( (maxCorner - minCorner).cwiseAbs(), wireframe, gay );
		glPopMatrix();
	}

	inline void drawCoordinateSystem( float size ) {
		glBegin(GL_LINES);
		glColor3f( 1.0, 0.0, 0.0 );
		glVertex3f( 0.0, 0.0, 0.0 );
		glVertex3f( size, 0.0, 0.0 );

		glColor3f( 0.0, 1.0, 0.0 );
		glVertex3f( 0.0, 0.0, 0.0 );
		glVertex3f( 0.0, size, 0.0 );

		glColor3f( 0.0, 0.0, 1.0 );
		glVertex3f( 0.0, 0.0, 0.0 );
		glVertex3f( 0.0, 0.0, size );
		glEnd();
	}

	// TODO: deprecated...
	inline void drawCordinateSystem( float size ) {
		drawCoordinateSystem( size );
	}

	inline void drawEllipse( float radius, bool wireframe = true, int n = 20, const Eigen::Vector3f &axis1 = Eigen::Vector3f::UnitX(), const Eigen::Vector3f &axis2 = Eigen::Vector3f::UnitY() ) {
		const float step = float( 2 * M_PI / n );
		glBegin( wireframe ? GL_LINE_LOOP : GL_TRIANGLE_FAN );
		for( int i = 0 ; i < n ; i++ ) {
			Eigen::glVertex( radius * (axis1 * cos(step * i) + axis2 * sin(step * i)) );
		}
		glEnd();
	}

	inline void drawAbstractSphere( float radius, bool wireframe = true, int n = 20 ) {
		drawEllipse( radius, wireframe, n, Eigen::Vector3f::UnitX(), Eigen::Vector3f::UnitY() );
		drawEllipse( radius, wireframe, n, Eigen::Vector3f::UnitY(), Eigen::Vector3f::UnitZ() );
		drawEllipse( radius, wireframe, n, Eigen::Vector3f::UnitX(), Eigen::Vector3f::UnitZ() );
	}

	inline void drawVector( const Eigen::Vector3f &direction ) {
		glBegin( GL_LINES );
		glVertex3f( 0.0, 0.0, 0.0 );
		Eigen::glVertex( direction );
		glEnd();
	}

	inline void drawVectorCone( const Eigen::Vector3f &direction, const float radius, int n = 10 ) {
		const Eigen::Matrix3f onb = Eigen::OrthonormalBase::fromNormal( direction );
		const float step = float( 2 * M_PI / n );

		glBegin( GL_LINES );
		for( int i = 0 ; i < n ; i++ ) {
			glVertex3f( 0.0, 0.0, 0.0 );
			Eigen::glVertex( direction + radius * (onb.col(1) * cos( step * i ) + onb.col(2) * sin(step * i)) );
		}
		glEnd();
	}

	inline void drawLine( const Eigen::Vector3f &start, const Eigen::Vector3f &end ) {
		glBegin( GL_LINES );
		Eigen::glVertex( start );
		Eigen::glVertex( end );
		glEnd();
	}

	inline void drawWireframeSphere( float radius, int n = 20, int slices = 5 ) {
		const float step = 2 * radius / (slices + 2);

		glPushMatrix();
		for( int axis = 0 ; axis < 3 ; ++axis ) {								
			for( int i = 0 ; i < slices ; ++i ) {
				const float z = step * (i+1) - radius;
				const float subRadius = sqrt( radius * radius - z*z );

				setPosition( Eigen::Vector3f::Unit(axis) * z );
				drawEllipse( subRadius, true, n, Eigen::Vector3f::Unit( (axis + 1) % 3 ), Eigen::Vector3f::Unit( (axis + 2) % 3 ) );
			}
		}
		glPopMatrix();
	}

	inline void drawSolidSphere( float radius, int u = 10, int v = 20 ) {
		glBegin( GL_QUADS );
		for( int i = 0 ; i < u ; i++ ) {
			for( int j = 0 ; j < v ; j++ ) {
				glColor3f( float(i) / (u-1), float(j) / (v-1), 0.0 );
				// cos | sin | cos
				// cos | 1   | sin
#define U(x) float((x)*M_PI/u - M_PI / 2)
#define V(x) float((x)*2*M_PI/v)
#define P(u_r,v_r) (Eigen::Vector3f( cos(U(u_r)) * cos(V(v_r)), sin(U(u_r)), cos(U(u_r)) * sin(V(v_r)) ) * radius)
				Eigen::glVertex( P(i,j) );
				Eigen::glVertex( P(i+1,j) );
				Eigen::glVertex( P(i+1,j+1) );
				Eigen::glVertex( P(i,j+1) );
#undef U
#undef V
#undef P
			}
		}
		glEnd();
	}

	inline void drawQuad( const Eigen::Vector2f &minCorner, const Eigen::Vector2f &maxCorner, bool wireframe = true ) {			
		if( wireframe ) {
			glBegin( GL_LINE_LOOP );
		}
		else {
			glBegin( GL_QUADS );
		}

		//glMultiTexCoord2f( GL_TEXTURE0, 0.0, 0.0 );
		glVertex3f( minCorner.x(), minCorner.y(), 0.0 );

		//glMultiTexCoord2f( GL_TEXTURE0, 1.0, 0.0 );
		glVertex3f( maxCorner.x(), minCorner.y(), 0.0 );

		//glMultiTexCoord2f( GL_TEXTURE0, 1.0, 1.0 );
		glVertex3f( maxCorner.x(), maxCorner.y(), 0.0 );

		//glMultiTexCoord2f( GL_TEXTURE0, 0.0, 1.0 );
		glVertex3f( minCorner.x(), maxCorner.y(), 0.0 );
		
		glEnd();
	}

	inline void drawTexturedScreenQuad() {			
		glBegin( GL_QUADS );
		glMultiTexCoord2f( GL_TEXTURE0, 0.0, 0.0 );
		glVertex3f( -1.0, -1.0, 0.0 );

		glMultiTexCoord2f( GL_TEXTURE0, 1.0, 0.0 );
		glVertex3f( 1.0, -1.0, 0.0 );

		glMultiTexCoord2f( GL_TEXTURE0, 1.0, 1.0 );
		glVertex3f( 1.0, 1.0, 0.0 );

		glMultiTexCoord2f( GL_TEXTURE0, 0.0, 1.0 );
		glVertex3f( -1.0, 1.0, 0.0 );
		glEnd();
	}
}