#include "surface.hpp"

#include <GLES2/gl2.h>
#include <GLES2/gl2ext.h>

#include <assert.h>
#include <cstddef>
#include <cstring>
#include <stdio.h>
#include <stereokit.h>

extern "C" {
#include "render/egl.h"
#define static
#include "render/gles2.h"
#undef static
#include "types/wlr_xdg_shell.h"
}

#include "sk_internal_defs.hpp"

#define DENSITY 1000.0f

using namespace sk;

Surface::Surface(struct wlr_renderer *renderer, struct wlr_surface *surface) {
	this->renderer = renderer;
	this->surface = surface;

	this->surfaceTex = tex_create();
	this->surfaceTex->type            = tex_type_image_nomips;
	this->surfaceTex->format          = tex_format_rgba32;

	this->surfaceTex->tex.type        = skg_tex_type_image;
	this->surfaceTex->tex.use         = skg_use_dynamic;
	this->surfaceTex->tex.mips        = skg_mip_none;
	this->surfaceTex->tex.format      = skg_tex_fmt_rgba32;
	this->surfaceTex->tex.array_count = 1;

	// this->surfaceShader = shader_create_file("/run/media/nova/MEDIA/xr/stardust/sk-wlroots/src/shaders/shader_unlit.sks");
	this->surfaceShader = shader_create_file("/run/media/nova/MEDIA/xr/stardust/sk-wlroots/src/shaders/shader_unlit_gamma.sks");

	this->surfaceMat = material_create(this->surfaceShader);
	material_set_transparency(this->surfaceMat, transparency_blend);
	material_set_texture(this->surfaceMat, "diffuse", this->surfaceTex);

	this->surfaceMesh = mesh_gen_plane(vec2_one, -vec3_forward, vec3_up);
}

Surface::~Surface() {}

void Surface::onMapped() {
	struct wlr_texture *surfaceTexture = wlr_surface_get_texture(surface);
	mesh_release(this->surfaceMesh);
	this->surfaceMesh = mesh_gen_plane(vec2{((float) surfaceTexture->width)/DENSITY, ((float) surfaceTexture->height)/DENSITY}, -vec3_forward, vec3_up);
}

GLuint tex = 0;

void readWaylandTexture(GLuint tex, GLenum target, GLenum format, uint32_t height, uint32_t width, void **data, size_t *size, GLuint *outTex) {
	*size = width * height * 4;
	*data = malloc(*size);
	memset(*data, uint8_t(127), *size);
	uint32_t fbo = 0;

	PFNGLFRAMEBUFFERTEXTUREOESPROC glFramebufferTextureOES = (PFNGLFRAMEBUFFERTEXTUREOESPROC) eglGetProcAddress("glFramebufferTextureOES");

	glGenFramebuffers          (1, &fbo);
	glBindFramebuffer          (GL_FRAMEBUFFER, fbo);
	(*glFramebufferTextureOES) (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, tex, 0);
	glFramebufferTexture2D     (GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

	glReadPixels(0, 0, width, height, format, GL_UNSIGNED_BYTE, *data);

	glBindFramebuffer   (GL_FRAMEBUFFER, 0);
	glDeleteFramebuffers(1, &fbo);
}

bool Surface::updateSurface() {
	struct wlr_texture *surfaceTexture = wlr_surface_get_texture(surface);
	if(!surfaceTexture || !wlr_texture_is_gles2(surfaceTexture)) {
		printf("Surface texture does not exist or is not GLES2\n");
		return false;
	}
	struct wlr_gles2_texture *eglTexture = (struct wlr_gles2_texture *) surfaceTexture;

	this->surfaceTex->tex.width       = surfaceTexture->width;
	this->surfaceTex->tex.height      = surfaceTexture->height;
	this->surfaceTex->tex._texture    = eglTexture->tex;
	this->surfaceTex->tex._target     = eglTexture->target;
	tex_set_options(this->surfaceTex, tex_sample_point, tex_address_clamp, 1);

	return true;
}

void Surface::draw() {
	if(!*mapped)
		return;
	if(updateSurface())
		render_add_mesh(this->surfaceMesh, this->surfaceMat, matrix_trs(vec3_forward * 0.5f));

	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	wlr_surface_send_frame_done(surface, &now);
}
