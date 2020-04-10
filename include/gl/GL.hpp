/**
 *  Shader class which wraps around GL calls
 *  Cuts down on excessive variable definitions within
 *  currently active scope.
 * 
 *  Also LearnOpenGL does this and it seems like a good idea
 */ 

#ifndef GL_H_
#define GL_H_

#include "glad/glad.h"
#include "GLFW/glfw3.h"
#include <string>

// set of functions which cut down on some of the BS
namespace GL {

/**
 *  Creates a GL program from a vertex and fragment shader
 *  whose paths are linked in their respective arguments.
 * 
 *  Returns true if program could be created successfully.
 *  Program desc contained in output.
 */ 
bool CreateProgram(const std::string& vert, const std::string& frag, GLuint* output);

GLuint CreateVBOFromArray(void* data, GLuint size, GLint usage);

GLuint CreateEBOFromArray(unsigned int* data, GLuint size, GLint usage);

}

#endif  // SHADER_H_