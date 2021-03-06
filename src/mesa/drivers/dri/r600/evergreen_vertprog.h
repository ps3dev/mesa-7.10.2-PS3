/*
 * Copyright (C) 2008-2009  Advanced Micro Devices, Inc.
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE COPYRIGHT HOLDER(S) BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN
 * AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN
 * CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/*
 * Authors:
 *   Richard Li <RichardZ.Li@amd.com>, <richardradeon@gmail.com>
 */


#ifndef _EVERGREEN_VERTPROG_H_
#define _EVERGREEN_VERTPROG_H_

#include "main/glheader.h"
#include "main/mtypes.h" 

#include "r700_shader.h"
#include "r700_assembler.h"

typedef struct evergreenArrayDesc //TEMP
{
	GLint size;   //number of data element
	GLenum type;  //data element type
	GLsizei stride;
	GLenum format; //GL_RGBA or GL_BGRA
} evergreenArrayDesc;

struct evergreen_vertex_program 
{
    struct gl_vertex_program *mesa_program; /* Must be first */

    struct evergreen_vertex_program *next;

    r700_AssemblerBase r700AsmCode;
    R700_Shader        r700Shader;

    GLboolean translated;
    GLboolean loaded;

    void * shaderbo;

	GLuint K0used;
    void * constbo0;

    evergreenArrayDesc              aos_desc[VERT_ATTRIB_MAX];
};

struct evergreen_vertex_program_cont
{
    struct gl_vertex_program mesa_program;

    struct evergreen_vertex_program *progs;
};

//Internal
unsigned int evergreen_Map_Vertex_Output(r700_AssemblerBase       *pAsm, 
			       struct gl_vertex_program *mesa_vp,
			       unsigned int unStart);
unsigned int evergreen_Map_Vertex_Input(r700_AssemblerBase       *pAsm, 
			      struct gl_vertex_program *mesa_vp,
			      unsigned int unStart);
GLboolean evergreen_Process_Vertex_Program_Vfetch_Instructions(
	struct evergreen_vertex_program *vp,
	struct gl_vertex_program   *mesa_vp);
GLboolean evergreen_Process_Vertex_Program_Vfetch_Instructions2(
    struct gl_context *ctx,
	struct evergreen_vertex_program *vp,
	struct gl_vertex_program   *mesa_vp);
void evergreen_Map_Vertex_Program(struct gl_context *ctx,
            struct evergreen_vertex_program *vp,
			struct gl_vertex_program   *mesa_vp);
GLboolean evergreen_Find_Instruction_Dependencies_vp(struct evergreen_vertex_program *vp,
					   struct gl_vertex_program   *mesa_vp);

struct evergreen_vertex_program* evergreenTranslateVertexShader(struct gl_context *ctx,
						      struct gl_vertex_program   *mesa_vp);

/* Interface */
extern void evergreenSelectVertexShader(struct gl_context *ctx);
extern void evergreenSetVertexFormat(struct gl_context *ctx, const struct gl_client_array *arrays[], int count);

extern GLboolean evergreenSetupVertexProgram(struct gl_context * ctx);

extern GLboolean evergreenSetupVPconstants(struct gl_context * ctx);

extern void * evergreenGetActiveVpShaderBo(struct gl_context * ctx);

extern void * evergreenGetActiveVpShaderConstBo(struct gl_context * ctx);

extern int evergreen_getTypeSize(GLenum type);

#endif /* _EVERGREEN_VERTPROG_H_ */
