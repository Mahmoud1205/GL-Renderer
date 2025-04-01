#pragma once

#include "defines.h"
#include "Shader.h"
#include <glm/glm.hpp>

struct Skybox final
{
	static bool		StartUpSystem();
	static void		ShutDownSystem();

	static void		UpdateProjection(const glm::mat4& inProjection);

	/**
	 * @param inBasePath The base path of the skybox faces. (i.e. if the skybox's sides file names is Day_posx, Day_posy, Day_negx,...
	 *		  then the base path name is Day_)
	 * The names of the faces must be in the format, `<SkyboxName><SignName><Axis>`
	 * SignName is = 'pos' or 'neg'
	 * Axis is = 'x' or 'y' or 'z'
	 * e.g. Night_posx is the X+ side of 'Night_' skybox.
	 * Note: When the image dimensions of the six faces is not the same, the function will fail and return false.
	 */
	bool			Load(const char* inBasePath, const char* inExtension);
	void			Unload();
	void			Draw(const glm::mat4& inView) const;

	u32				mID = UINT32_MAX;

	sinline Shader	sSkyboxShader{};
	sinline u32		sVAO = UINT32_MAX;
	sinline u32		sVBO = UINT32_MAX;
};
