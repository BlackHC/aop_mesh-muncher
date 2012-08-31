// adapted glsdk's glutil Debug module to work with glew
// TODO: check license!!

#include <stdio.h>
#include <string>
#include <gl/glew.h>
#include <windows.h>
#include "Debug.h"

namespace glutil
{
	namespace
	{
		GLDEBUGPROCARB oldProc = NULL;

		std::string GetErrorSource(GLenum source)
		{
			switch(source)
			{
			case GL_DEBUG_SOURCE_API_ARB: return "API";
			case GL_DEBUG_SOURCE_WINDOW_SYSTEM_ARB: return "Window System";
			case GL_DEBUG_SOURCE_SHADER_COMPILER_ARB: return "Shader Compiler";
			case GL_DEBUG_SOURCE_THIRD_PARTY_ARB: return "Third Party";
			case GL_DEBUG_SOURCE_APPLICATION_ARB: return "Application";
			case GL_DEBUG_SOURCE_OTHER_ARB: return "Other";
			default: return "WTF?";
			}
		}

		std::string GetErrorType(GLenum type)
		{
			switch(type)
			{
			case GL_DEBUG_TYPE_ERROR_ARB: return "Error";
			case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR_ARB: return "Deprecated Functionality";
			case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR_ARB: return "Undefined Behavior";
			case GL_DEBUG_TYPE_PORTABILITY_ARB: return "Portability";
			case GL_DEBUG_TYPE_PERFORMANCE_ARB: return "Performance";
			case GL_DEBUG_TYPE_OTHER_ARB: return "Other";
			default: return "WTF?";
			}
		}

		std::string GetErrorSeverity(GLenum severity)
		{
			switch(severity)
			{
			case GL_DEBUG_SEVERITY_HIGH_ARB: return "High";
			case GL_DEBUG_SEVERITY_MEDIUM_ARB: return "Medium";
			case GL_DEBUG_SEVERITY_LOW_ARB: return "Low";
			default: return "WTF?";
			}
		}

		void APIENTRY DebugFuncStdOut(GLenum source, GLenum type, GLuint id, GLenum severity,
			GLsizei length, const GLchar* message, GLvoid* userParam)
		{
			if(oldProc)
				oldProc(source, type, id, severity, length, message, userParam);

			std::string srcName = GetErrorSource(source);
			std::string errorType = GetErrorType(type);
			std::string typeSeverity = GetErrorSeverity(severity);

			printf("************************\n%s from %s,\t%s priority\nMessage: %s\n",
				errorType.c_str(), srcName.c_str(), typeSeverity.c_str(), message);
		}

		void APIENTRY DebugFuncStdErr(GLenum source, GLenum type, GLuint id, GLenum severity,
			GLsizei length, const GLchar* message, GLvoid* userParam)
		{
			if(oldProc)
				oldProc(source, type, id, severity, length, message, userParam);

			std::string srcName = GetErrorSource(source);
			std::string errorType = GetErrorType(type);
			std::string typeSeverity = GetErrorSeverity(severity);

			fprintf(stderr, "************************\n%s from %s,\t%s priority\nMessage: %s\n",
				errorType.c_str(), srcName.c_str(), typeSeverity.c_str(), message);
		}
	}

	bool RegisterDebugOutput( OutputLocation eLoc )
	{
		if(!GLEW_ARB_debug_output)
			return false;

		void *pData = NULL;
		glGetPointerv(GL_DEBUG_CALLBACK_FUNCTION_ARB, (void**)(&oldProc));
		if(oldProc)
			glGetPointerv(GL_DEBUG_CALLBACK_USER_PARAM_ARB, &pData);

		glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS_ARB);

		switch(eLoc)
		{
		case STD_OUT:
			glDebugMessageCallbackARB(DebugFuncStdOut, pData);
			break;
		case STD_ERR:
			glDebugMessageCallbackARB(DebugFuncStdErr, pData);
			break;
		}

		return true;
	}
}

