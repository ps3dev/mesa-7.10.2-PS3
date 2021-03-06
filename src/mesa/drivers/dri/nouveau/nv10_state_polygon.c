/*
 * Copyright (C) 2009 Francisco Jerez.
 * All Rights Reserved.
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the
 * "Software"), to deal in the Software without restriction, including
 * without limitation the rights to use, copy, modify, merge, publish,
 * distribute, sublicense, and/or sell copies of the Software, and to
 * permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 *
 * The above copyright notice and this permission notice (including the
 * next paragraph) shall be included in all copies or substantial
 * portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE COPYRIGHT OWNER(S) AND/OR ITS SUPPLIERS BE
 * LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION
 * OF CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 * WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "nouveau_driver.h"
#include "nouveau_context.h"
#include "nouveau_gldefs.h"
#include "nouveau_util.h"
#include "nv10_3d.xml.h"
#include "nv10_driver.h"

void
nv10_emit_cull_face(struct gl_context *ctx, int emit)
{
	struct nouveau_channel *chan = context_chan(ctx);
	struct nouveau_grobj *celsius = context_eng3d(ctx);
	GLenum mode = ctx->Polygon.CullFaceMode;

	BEGIN_RING(chan, celsius, NV10_3D_CULL_FACE_ENABLE, 1);
	OUT_RINGb(chan, ctx->Polygon.CullFlag);

	BEGIN_RING(chan, celsius, NV10_3D_CULL_FACE, 1);
	OUT_RING(chan, (mode == GL_FRONT ? NV10_3D_CULL_FACE_FRONT :
			mode == GL_BACK ? NV10_3D_CULL_FACE_BACK :
			NV10_3D_CULL_FACE_FRONT_AND_BACK));
}

void
nv10_emit_front_face(struct gl_context *ctx, int emit)
{
	struct nouveau_channel *chan = context_chan(ctx);
	struct nouveau_grobj *celsius = context_eng3d(ctx);

	BEGIN_RING(chan, celsius, NV10_3D_FRONT_FACE, 1);
	OUT_RING(chan, ctx->Polygon.FrontFace == GL_CW ?
		 NV10_3D_FRONT_FACE_CW : NV10_3D_FRONT_FACE_CCW);
}

void
nv10_emit_line_mode(struct gl_context *ctx, int emit)
{
	struct nouveau_channel *chan = context_chan(ctx);
	struct nouveau_grobj *celsius = context_eng3d(ctx);
	GLboolean smooth = ctx->Line.SmoothFlag &&
		ctx->Hint.LineSmooth == GL_NICEST;

	BEGIN_RING(chan, celsius, NV10_3D_LINE_WIDTH, 1);
	OUT_RING(chan, MAX2(smooth ? 0 : 1,
			    ctx->Line.Width) * 8);
	BEGIN_RING(chan, celsius, NV10_3D_LINE_SMOOTH_ENABLE, 1);
	OUT_RINGb(chan, smooth);
}

void
nv10_emit_line_stipple(struct gl_context *ctx, int emit)
{
}

void
nv10_emit_point_mode(struct gl_context *ctx, int emit)
{
	struct nouveau_channel *chan = context_chan(ctx);
	struct nouveau_grobj *celsius = context_eng3d(ctx);

	BEGIN_RING(chan, celsius, NV10_3D_POINT_SIZE, 1);
	OUT_RING(chan, (uint32_t)(ctx->Point.Size * 8));

	BEGIN_RING(chan, celsius, NV10_3D_POINT_SMOOTH_ENABLE, 1);
	OUT_RINGb(chan, ctx->Point.SmoothFlag);
}

void
nv10_emit_polygon_mode(struct gl_context *ctx, int emit)
{
	struct nouveau_channel *chan = context_chan(ctx);
	struct nouveau_grobj *celsius = context_eng3d(ctx);

	BEGIN_RING(chan, celsius, NV10_3D_POLYGON_MODE_FRONT, 2);
	OUT_RING(chan, nvgl_polygon_mode(ctx->Polygon.FrontMode));
	OUT_RING(chan, nvgl_polygon_mode(ctx->Polygon.BackMode));

	BEGIN_RING(chan, celsius, NV10_3D_POLYGON_SMOOTH_ENABLE, 1);
	OUT_RINGb(chan, ctx->Polygon.SmoothFlag);
}

void
nv10_emit_polygon_offset(struct gl_context *ctx, int emit)
{
	struct nouveau_channel *chan = context_chan(ctx);
	struct nouveau_grobj *celsius = context_eng3d(ctx);

	BEGIN_RING(chan, celsius, NV10_3D_POLYGON_OFFSET_POINT_ENABLE, 3);
	OUT_RINGb(chan, ctx->Polygon.OffsetPoint);
	OUT_RINGb(chan, ctx->Polygon.OffsetLine);
	OUT_RINGb(chan, ctx->Polygon.OffsetFill);

	BEGIN_RING(chan, celsius, NV10_3D_POLYGON_OFFSET_FACTOR, 2);
	OUT_RINGf(chan, ctx->Polygon.OffsetFactor);
	OUT_RINGf(chan, ctx->Polygon.OffsetUnits);
}

void
nv10_emit_polygon_stipple(struct gl_context *ctx, int emit)
{
}
