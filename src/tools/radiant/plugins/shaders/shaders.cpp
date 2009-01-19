/**
 * @file shaders.cpp
 * @brief Shaders Manager Plugin
 * @note there is an important distinction between SHADER_NOT_FOUND and SHADER_NOTEX:
 * SHADER_NOT_FOUND means we didn't find the raw texture or the shader for this
 * SHADER_NOTEX means we recognize this as a shader script, but we are missing the texture to represent it
 * this was in the initial design of the shader code since early GtkRadiant alpha, and got sort of foxed in 1.2 and put back in
 */

/*
Copyright (c) 2001, Loki software, inc.
All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met:

Redistributions of source code must retain the above copyright notice, this list
of conditions and the following disclaimer.

Redistributions in binary form must reproduce the above copyright notice, this
list of conditions and the following disclaimer in the documentation and/or
other materials provided with the distribution.

Neither the name of Loki software nor the names of its contributors may be used
to endorse or promote products derived from this software without specific prior
written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS ``AS IS''
AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT,INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

#include "shaders.h"

#include <stdio.h>
#include <stdlib.h>
#include <map>
#include <list>

#include "ifilesystem.h"
#include "ishaders.h"
#include "iscriplib.h"
#include "itextures.h"
#include "iradiant.h"
#include "irender.h"

#include <glib/gslist.h>

#include "autoptr.h"
#include "debugging/debugging.h"
#include "string/pooledstring.h"
#include "math/vector.h"
#include "generic/callback.h"
#include "generic/referencecounted.h"
#include "stream/memstream.h"
#include "stream/stringstream.h"
#include "stream/textfilestream.h"
#include "os/path.h"
#include "os/dir.h"
#include "os/file.h"
#include "stringio.h"
#include "shaderlib.h"
#include "texturelib.h"
#include "moduleobservers.h"
#include "archivelib.h"
#include "imagelib.h"

const char* g_texturePrefix = "textures/";

static Callback g_ActiveShadersChangedNotify;

class ShaderPoolContext {
};
typedef Static<StringPool, ShaderPoolContext> ShaderPool;
typedef PooledString<ShaderPool> ShaderString;
typedef ShaderString ShaderVariable;
typedef ShaderString ShaderValue;
typedef CopiedString TextureExpression;

// clean a texture name to the qtexture_t name format we use internally
// NOTE: case sensitivity: the engine is case sensitive. we store the shader name with case information and save with case
// information as well. but we assume there won't be any case conflict and so when doing lookups based on shader name,
// we compare as case insensitive. That is Radiant is case insensitive, but knows that the engine is case sensitive.
//++timo FIXME: we need to put code somewhere to detect when two shaders that are case insensitive equal are present
template<typename StringType>
void parseTextureName(StringType& name, const char* token) {
	StringOutputStream cleaned(256);
	cleaned << PathCleaned(token);
	name = CopiedString(StringRange(cleaned.c_str(), path_get_filename_base_end(cleaned.c_str()))).c_str(); // remove extension
}

bool Tokeniser_parseTextureName(Tokeniser& tokeniser, TextureExpression& name) {
	const char* token = tokeniser.getToken();
	if (token == 0) {
		Tokeniser_unexpectedError(tokeniser, token, "#texture-name");
		return false;
	}
	parseTextureName(name, token);
	return true;
}

bool Tokeniser_parseShaderName(Tokeniser& tokeniser, CopiedString& name) {
	const char* token = tokeniser.getToken();
	if (token == 0) {
		Tokeniser_unexpectedError(tokeniser, token, "#shader-name");
		return false;
	}
	parseTextureName(name, token);
	return true;
}

bool Tokeniser_parseString(Tokeniser& tokeniser, ShaderString& string) {
	const char* token = tokeniser.getToken();
	if (token == 0) {
		Tokeniser_unexpectedError(tokeniser, token, "#string");
		return false;
	}
	string = token;
	return true;
}



typedef std::list<ShaderVariable> ShaderParameters;
typedef std::list<ShaderVariable> ShaderArguments;

typedef std::pair<ShaderVariable, ShaderVariable> BlendFuncExpression;

class ShaderTemplate {
	std::size_t m_refcount;
	CopiedString m_Name;
public:

	ShaderParameters m_params;

	TextureExpression m_textureName;
	TextureExpression m_diffuse;
	TextureExpression m_bump;
	ShaderValue m_heightmapScale;
	TextureExpression m_specular;
	TextureExpression m_lightFalloffImage;

	int m_nFlags;
	float m_fTrans;

	// alphafunc stuff
	IShader::EAlphaFunc m_AlphaFunc;
	float m_AlphaRef;
	// cull stuff
	IShader::ECull m_Cull;

	ShaderTemplate() :
			m_refcount(0) {
		m_nFlags = 0;
		m_fTrans = 1.0f;
	}

	void IncRef() {
		++m_refcount;
	}
	void DecRef() {
		ASSERT_MESSAGE(m_refcount != 0, "shader reference-count going below zero");
		if (--m_refcount == 0) {
			delete this;
		}
	}

	std::size_t refcount() {
		return m_refcount;
	}

	const char* getName() const {
		return m_Name.c_str();
	}
	void setName(const char* name) {
		m_Name = name;
	}

	// -----------------------------------------

	bool parseUFO(Tokeniser& tokeniser);
	bool parseTemplate(Tokeniser& tokeniser);


	void CreateDefault(const char *name) {
		m_textureName = name;
		setName(name);
	}


	class MapLayerTemplate {
		TextureExpression m_texture;
		BlendFuncExpression m_blendFunc;
		bool m_clampToBorder;
		ShaderValue m_alphaTest;
	public:
		MapLayerTemplate(const TextureExpression& texture, const BlendFuncExpression& blendFunc, bool clampToBorder, const ShaderValue& alphaTest) :
				m_texture(texture),
				m_blendFunc(blendFunc),
				m_clampToBorder(false),
				m_alphaTest(alphaTest) {
		}
		const TextureExpression& texture() const {
			return m_texture;
		}
		const BlendFuncExpression& blendFunc() const {
			return m_blendFunc;
		}
		bool clampToBorder() const {
			return m_clampToBorder;
		}
		const ShaderValue& alphaTest() const {
			return m_alphaTest;
		}
	};
	typedef std::vector<MapLayerTemplate> MapLayers;
	MapLayers m_layers;
};

enum LayerTypeId {
	LAYER_NONE,
	LAYER_BLEND,
	LAYER_DIFFUSEMAP,
	LAYER_SPECULARMAP
};

class LayerTemplate {
public:
	LayerTypeId m_type;
	TextureExpression m_texture;
	BlendFuncExpression m_blendFunc;
	bool m_clampToBorder;
	ShaderValue m_alphaTest;
	ShaderValue m_heightmapScale;

	LayerTemplate() : m_type(LAYER_NONE), m_blendFunc("GL_ONE", "GL_ZERO"), m_clampToBorder(false), m_alphaTest("-1"), m_heightmapScale("0") {
	}
};

bool parseShaderParameters(Tokeniser& tokeniser, ShaderParameters& params) {
	Tokeniser_parseToken(tokeniser, "(");
	for (;;) {
		const char* param = tokeniser.getToken();
		if (string_equal(param, ")")) {
			break;
		}
		params.push_back(param);
		const char* comma = tokeniser.getToken();
		if (string_equal(comma, ")")) {
			break;
		}
		if (!string_equal(comma, ",")) {
			Tokeniser_unexpectedError(tokeniser, comma, ",");
			return false;
		}
	}
	return true;
}

bool ShaderTemplate::parseTemplate(Tokeniser& tokeniser) {
	m_Name = tokeniser.getToken();
	if (!parseShaderParameters(tokeniser, m_params))
		g_warning("shader template: '%s': parameter parse failed\n", m_Name.c_str());

	return false;
}

typedef SmartPointer<ShaderTemplate> ShaderTemplatePointer;
typedef std::map<CopiedString, ShaderTemplatePointer> ShaderTemplateMap;

ShaderTemplateMap g_shaders;
ShaderTemplateMap g_shaderTemplates;

ShaderTemplate* findTemplate(const char* name) {
	ShaderTemplateMap::iterator i = g_shaderTemplates.find(name);
	if (i != g_shaderTemplates.end()) {
		return (*i).second.get();
	}
	return 0;
}

class ShaderDefinition {
public:
	ShaderDefinition(ShaderTemplate* shaderTemplate, const ShaderArguments& args, const char* filename)
			: shaderTemplate(shaderTemplate), args(args), filename(filename) {
	}
	ShaderTemplate* shaderTemplate;
	ShaderArguments args;
	const char* filename;
};

typedef std::map<CopiedString, ShaderDefinition> ShaderDefinitionMap;

ShaderDefinitionMap g_shaderDefinitions;

const char* evaluateShaderValue(const char* value, const ShaderParameters& params, const ShaderArguments& args) {
	ShaderArguments::const_iterator j = args.begin();
	for (ShaderParameters::const_iterator i = params.begin(); i != params.end(); ++i, ++j) {
		const char* other = (*i).c_str();
		if (string_equal(value, other)) {
			return (*j).c_str();
		}
	}
	return value;
}

///\todo BlendFunc parsing
BlendFunc evaluateBlendFunc(const BlendFuncExpression& blendFunc, const ShaderParameters& params, const ShaderArguments& args) {
	return BlendFunc(BLEND_ONE, BLEND_ZERO);
}

qtexture_t* evaluateTexture(const TextureExpression& texture, const ShaderParameters& params, const ShaderArguments& args, const LoadImageCallback& loader = GlobalTexturesCache().defaultLoader()) {
	StringOutputStream result(64);
	const char* expression = texture.c_str();
	const char* end = expression + string_length(expression);
	if (!string_empty(expression)) {
		for (;;) {
			const char* best = end;
			const char* bestParam = 0;
			const char* bestArg = 0;
			ShaderArguments::const_iterator j = args.begin();
			for (ShaderParameters::const_iterator i = params.begin(); i != params.end(); ++i, ++j) {
				const char* found = strstr(expression, (*i).c_str());
				if (found != 0 && found < best) {
					best = found;
					bestParam = (*i).c_str();
					bestArg = (*j).c_str();
				}
			}
			if (best != end) {
				result << StringRange(expression, best);
				result << PathCleaned(bestArg);
				expression = best + string_length(bestParam);
			} else {
				break;
			}
		}
		result << expression;
	}
	return GlobalTexturesCache().capture(loader, result.c_str());
}

float evaluateFloat(const ShaderValue& value, const ShaderParameters& params, const ShaderArguments& args) {
	const char* result = evaluateShaderValue(value.c_str(), params, args);
	float f = 0.0f;
	if (!string_parse_float(result, f)) {
		globalErrorStream() << "parsing float value failed: " << makeQuoted(result) << "\n";
	}
	return f;
}

static BlendFactor evaluateBlendFactor(const ShaderValue& value, const ShaderParameters& params, const ShaderArguments& args) {
	const char* result = evaluateShaderValue(value.c_str(), params, args);

	if (string_equal_nocase(result, "gl_zero")) {
		return BLEND_ZERO;
	}
	if (string_equal_nocase(result, "gl_one")) {
		return BLEND_ONE;
	}
	if (string_equal_nocase(result, "gl_src_color")) {
		return BLEND_SRC_COLOUR;
	}
	if (string_equal_nocase(result, "gl_one_minus_src_color")) {
		return BLEND_ONE_MINUS_SRC_COLOUR;
	}
	if (string_equal_nocase(result, "gl_src_alpha")) {
		return BLEND_SRC_ALPHA;
	}
	if (string_equal_nocase(result, "gl_one_minus_src_alpha")) {
		return BLEND_ONE_MINUS_SRC_ALPHA;
	}
	if (string_equal_nocase(result, "gl_dst_color")) {
		return BLEND_DST_COLOUR;
	}
	if (string_equal_nocase(result, "gl_one_minus_dst_color")) {
		return BLEND_ONE_MINUS_DST_COLOUR;
	}
	if (string_equal_nocase(result, "gl_dst_alpha")) {
		return BLEND_DST_ALPHA;
	}
	if (string_equal_nocase(result, "gl_one_minus_dst_alpha")) {
		return BLEND_ONE_MINUS_DST_ALPHA;
	}
	if (string_equal_nocase(result, "gl_src_alpha_saturate")) {
		return BLEND_SRC_ALPHA_SATURATE;
	}

	g_warning("parsing blend-factor value failed: '%s'\n", result);
	return BLEND_ZERO;
}

class CShader : public IShader {
	std::size_t m_refcount;

	const ShaderTemplate& m_template;
	const ShaderArguments& m_args;
	const char* m_filename;
	// name is shader-name, otherwise texture-name (if not a real shader)
	CopiedString m_Name;

	qtexture_t* m_pTexture;
	qtexture_t* m_notfound;
	float m_heightmapScale;
	qtexture_t* m_pLightFalloffImage;
	BlendFunc m_blendFunc;

	bool m_bInUse;


public:
	static bool m_lightingEnabled;

	CShader(const ShaderDefinition& definition) :
			m_refcount(0),
			m_template(*definition.shaderTemplate),
			m_args(definition.args),
			m_filename(definition.filename),
			m_blendFunc(BLEND_SRC_ALPHA, BLEND_ONE_MINUS_SRC_ALPHA),
			m_bInUse(false) {
		m_pTexture = 0;

		m_notfound = 0;

		realise();
	}
	virtual ~CShader() {
		unrealise();

		ASSERT_MESSAGE(m_refcount == 0, "deleting active shader");
	}

	// IShaders implementation -----------------
	void IncRef() {
		++m_refcount;
	}
	void DecRef() {
		ASSERT_MESSAGE(m_refcount != 0, "shader reference-count going below zero");
		if (--m_refcount == 0) {
			delete this;
		}
	}

	std::size_t refcount() {
		return m_refcount;
	}

	// get/set the qtexture_t* Radiant uses to represent this shader object
	qtexture_t* getTexture() const {
		return m_pTexture;
	}
	// get shader name
	const char* getName() const {
		return m_Name.c_str();
	}
	bool IsInUse() const {
		return m_bInUse;
	}
	void SetInUse(bool bInUse) {
		m_bInUse = bInUse;
		g_ActiveShadersChangedNotify();
	}
	// get the shader flags
	int getFlags() const {
		return m_template.m_nFlags;
	}
	// get the transparency value
	float getTrans() const {
		return m_template.m_fTrans;
	}
	// test if it's a true shader, or a default shader created to wrap around a texture
	bool IsDefault() const {
		return string_empty(m_filename);
	}
	// get the alphaFunc
	void getAlphaFunc(EAlphaFunc *func, float *ref) {
		*func = m_template.m_AlphaFunc;
		*ref = m_template.m_AlphaRef;
	};
	BlendFunc getBlendFunc() const {
		return m_blendFunc;
	}
	// get the cull type
	ECull getCull() {
		return m_template.m_Cull;
	};
	// get shader file name (ie the file where this one is defined)
	const char* getShaderFileName() const {
		return m_filename;
	}
	// -----------------------------------------

	void realise() {
		m_pTexture = evaluateTexture(m_template.m_textureName, m_template.m_params, m_args);

		if (m_pTexture->texture_number == 0) {
			m_notfound = m_pTexture;

			{
				StringOutputStream name(256);
				name << GlobalRadiant().getEnginePath() << GlobalRadiant().getRequiredGameDescriptionKeyValue("basegame") <<  "/textures/tex_common/nodraw";
				m_pTexture = GlobalTexturesCache().capture(name.c_str());
			}
		}
	}

	void unrealise() {
		GlobalTexturesCache().release(m_pTexture);

		if (m_notfound != 0) {
			GlobalTexturesCache().release(m_notfound);
		}
	}

	void realiseLighting() {
	}

	void unrealiseLighting() {
	}

	// set shader name
	void setName(const char* name) {
		m_Name = name;
	}

class MapLayer : public ShaderLayer {
		qtexture_t* m_texture;
		BlendFunc m_blendFunc;
		bool m_clampToBorder;
		float m_alphaTest;
	public:
		MapLayer(qtexture_t* texture, BlendFunc blendFunc, bool clampToBorder, float alphaTest) :
				m_texture(texture),
				m_blendFunc(blendFunc),
				m_clampToBorder(false),
				m_alphaTest(alphaTest) {
		}
		qtexture_t* texture() const {
			return m_texture;
		}
		BlendFunc blendFunc() const {
			return m_blendFunc;
		}
		bool clampToBorder() const {
			return m_clampToBorder;
		}
		float alphaTest() const {
			return m_alphaTest;
		}
	};

	static MapLayer evaluateLayer(const ShaderTemplate::MapLayerTemplate& layerTemplate, const ShaderParameters& params, const ShaderArguments& args) {
		return MapLayer(
		           evaluateTexture(layerTemplate.texture(), params, args),
		           evaluateBlendFunc(layerTemplate.blendFunc(), params, args),
		           layerTemplate.clampToBorder(),
		           evaluateFloat(layerTemplate.alphaTest(), params, args)
		       );
	}

	typedef std::vector<MapLayer> MapLayers;
	MapLayers m_layers;

	const ShaderLayer* firstLayer() const {
		if (m_layers.empty()) {
			return 0;
		}
		return &m_layers.front();
	}
	void forEachLayer(const ShaderLayerCallback& callback) const {
		for (MapLayers::const_iterator i = m_layers.begin(); i != m_layers.end(); ++i) {
			callback(*i);
		}
	}

	qtexture_t* lightFalloffImage() const {
		if (!string_empty(m_template.m_lightFalloffImage.c_str())) {
			return m_pLightFalloffImage;
		}
		return 0;
	}
};

bool CShader::m_lightingEnabled = false;

typedef SmartPointer<CShader> ShaderPointer;
typedef std::map<CopiedString, ShaderPointer, shader_less_t> shaders_t;

shaders_t g_ActiveShaders;

static shaders_t::iterator g_ActiveShadersIterator;

static void ActiveShaders_IteratorBegin() {
	g_ActiveShadersIterator = g_ActiveShaders.begin();
}

static bool ActiveShaders_IteratorAtEnd() {
	return g_ActiveShadersIterator == g_ActiveShaders.end();
}

static IShader *ActiveShaders_IteratorCurrent() {
	return static_cast<CShader*>(g_ActiveShadersIterator->second);
}

static void ActiveShaders_IteratorIncrement() {
	++g_ActiveShadersIterator;
}

void debug_check_shaders(shaders_t& shaders) {
	for (shaders_t::iterator i = shaders.begin(); i != shaders.end(); ++i) {
		ASSERT_MESSAGE(i->second->refcount() == 1, "orphan shader still referenced");
	}
}

bool ShaderTemplate::parseUFO(Tokeniser& tokeniser) {
	// name of the qtexture_t we'll use to represent this shader (this one has the "textures/" before)
	m_textureName = m_Name.c_str();

	tokeniser.nextLine();

	// we need to read until we hit a balanced }
	int depth = 0;
	for (;;) {
		tokeniser.nextLine();
		const char* token = tokeniser.getToken();

		if (token == 0)
			return false;

		if (string_equal(token, "{")) {
			++depth;
			continue;
		} else if (string_equal(token, "}")) {
			--depth;
			if (depth < 0) { // underflow
				return false;
			}
			if (depth == 0) { // end of shader
				break;
			}

			continue;
		}

		if (depth == 1) {
			if (string_equal_nocase(token, "trans")) {
				RETURN_FALSE_IF_FAIL(Tokeniser_getFloat(tokeniser, m_fTrans));
				m_nFlags |= QER_TRANS;
			} else if (string_equal_nocase(token, "alphafunc")) {
				const char* alphafunc = tokeniser.getToken();

				if (alphafunc == 0) {
					Tokeniser_unexpectedError(tokeniser, alphafunc, "#alphafunc");
					return false;
				}

				if (string_equal_nocase(alphafunc, "equal")) {
					m_AlphaFunc = IShader::eEqual;
				} else if (string_equal_nocase(alphafunc, "greater")) {
					m_AlphaFunc = IShader::eGreater;
				} else if (string_equal_nocase(alphafunc, "less")) {
					m_AlphaFunc = IShader::eLess;
				} else if (string_equal_nocase(alphafunc, "gequal")) {
					m_AlphaFunc = IShader::eGEqual;
				} else if (string_equal_nocase(alphafunc, "lequal")) {
					m_AlphaFunc = IShader::eLEqual;
				} else {
					m_AlphaFunc = IShader::eAlways;
				}

				m_nFlags |= QER_ALPHATEST;

				RETURN_FALSE_IF_FAIL(Tokeniser_getFloat(tokeniser, m_AlphaRef));
			} else if (string_equal_nocase(token, "param")) {
				const char* surfaceparm = tokeniser.getToken();

				if (surfaceparm == 0) {
					Tokeniser_unexpectedError(tokeniser, surfaceparm, "param");
					return false;
				}

				if (string_equal_nocase(surfaceparm, "clip")) {
					m_nFlags |= QER_CLIP;
				}
			}
		}
	}

	return true;
}

class Layer {
public:
	LayerTypeId m_type;
	TextureExpression m_texture;
	BlendFunc m_blendFunc;
	bool m_clampToBorder;
	float m_alphaTest;
	float m_heightmapScale;

	Layer() : m_type(LAYER_NONE), m_blendFunc(BLEND_ONE, BLEND_ZERO), m_clampToBorder(false), m_alphaTest(-1), m_heightmapScale(0) {
	}
};

void ParseShaderFile(Tokeniser& tokeniser, const char* filename) {

	tokeniser.nextLine();
	for (;;) {
		const char* token = tokeniser.getToken();

		if (token == 0) {
			break;
		}

		if (!string_equal(token, "material")
		 && !string_equal(token, "particle")
		 && !string_equal(token, "skin")) {
			tokeniser.ungetToken();
		}
		// first token should be the path + name.. (from base)
		CopiedString name;
		Tokeniser_parseShaderName(tokeniser, name);
		ShaderTemplatePointer shaderTemplate(new ShaderTemplate());
		shaderTemplate->setName(name.c_str());

		g_shaders.insert(ShaderTemplateMap::value_type(shaderTemplate->getName(), shaderTemplate));

		const bool result = shaderTemplate->parseUFO(tokeniser);
		if (result) {
			// do we already have this shader?
			if (!g_shaderDefinitions.insert(ShaderDefinitionMap::value_type(shaderTemplate->getName(), ShaderDefinition(shaderTemplate.get(), ShaderArguments(), filename))).second) {
				g_debug("Shader '%s' is already in memory, definition in '%s' ignored.\n", shaderTemplate->getName(), filename);
			}
		} else {
			g_warning("Error parsing shader '%s'\n", shaderTemplate->getName());
			return;
		}
	}
}

static void LoadShaderFile(const char* filename) {
	const char* appPath = GlobalRadiant().getAppPath();
	StringOutputStream shadername(256);
	shadername << appPath << filename;

	AutoPtr<ArchiveTextFile> file(GlobalFileSystem().openTextFile(shadername.c_str()));
	if (file) {
		g_message("Parsing shaderfile '%s'\n", shadername.c_str());

		AutoPtr<Tokeniser> tokeniser(GlobalScriptLibrary().m_pfnNewScriptTokeniser(file->getInputStream()));
		ParseShaderFile(*tokeniser, shadername.c_str());
	} else {
		g_warning("Unable to read shaderfile '%s'\n", shadername.c_str());
	}
}


CShader* Try_Shader_ForName(const char* name) {
	{
		shaders_t::iterator i = g_ActiveShaders.find(name);
		if (i != g_ActiveShaders.end()) {
			return (*i).second;
		}
	}
	// active shader was not found

	// find matching shader definition
	ShaderDefinitionMap::iterator i = g_shaderDefinitions.find(name);
	if (i == g_shaderDefinitions.end()) {
		// shader definition was not found

		// create new shader definition from default shader template
		ShaderTemplatePointer shaderTemplate(new ShaderTemplate());
		shaderTemplate->CreateDefault(name);
		g_shaderTemplates.insert(ShaderTemplateMap::value_type(shaderTemplate->getName(), shaderTemplate));

		i = g_shaderDefinitions.insert(ShaderDefinitionMap::value_type(name, ShaderDefinition(shaderTemplate.get(), ShaderArguments(), ""))).first;
	}

	// create shader from existing definition
	ShaderPointer pShader(new CShader((*i).second));
	pShader->setName(name);
	g_ActiveShaders.insert(shaders_t::value_type(name, pShader));
	g_ActiveShadersChangedNotify();
	return pShader;
}

IShader *Shader_ForName(const char *name) {
	ASSERT_NOTNULL(name);

	IShader *pShader = Try_Shader_ForName(name);
	pShader->IncRef();
	return pShader;
}

#include "stream/filestream.h"

void Shaders_Load() {
	LoadShaderFile("shaders/common.shader");
	LoadShaderFile("shaders/textures.shader");
}

// will free all GL binded qtextures and shaders
// NOTE: doesn't make much sense out of Radiant exit or called during a reload
void Shaders_Free() {
	// reload shaders
	// empty the actives shaders list
	debug_check_shaders(g_ActiveShaders);
	g_ActiveShaders.clear();
	g_shaders.clear();
	g_shaderTemplates.clear();
	g_shaderDefinitions.clear();
	g_ActiveShadersChangedNotify();
}

static ModuleObservers g_observers;

/** @brief wait until filesystem and is realised before loading anything */
static std::size_t g_shaders_unrealised = 1;

bool Shaders_realised() {
	return g_shaders_unrealised == 0;
}
void Shaders_Realise() {
	if (--g_shaders_unrealised == 0) {
		Shaders_Load();
		g_observers.realise();
	}
}
void Shaders_Unrealise() {
	if (++g_shaders_unrealised == 1) {
		g_observers.unrealise();
		Shaders_Free();
	}
}

void Shaders_Refresh() {
	Shaders_Unrealise();
	Shaders_Realise();
}

class UFOShaderSystem : public ShaderSystem, public ModuleObserver {
public:
	void realise() {
		Shaders_Realise();
	}
	void unrealise() {
		Shaders_Unrealise();
	}
	void refresh() {
		Shaders_Refresh();
	}

	IShader* getShaderForName(const char* name) {
		return Shader_ForName(name);
	}

	void foreachShaderName(const ShaderNameCallback& callback) {
		for (ShaderDefinitionMap::const_iterator i = g_shaderDefinitions.begin(); i != g_shaderDefinitions.end(); ++i) {
			callback((*i).first.c_str());
		}
	}

	void beginActiveShadersIterator() {
		ActiveShaders_IteratorBegin();
	}
	bool endActiveShadersIterator() {
		return ActiveShaders_IteratorAtEnd();
	}
	IShader* dereferenceActiveShadersIterator() {
		return ActiveShaders_IteratorCurrent();
	}
	void incrementActiveShadersIterator() {
		ActiveShaders_IteratorIncrement();
	}
	void setActiveShadersChangedNotify(const Callback& notify) {
		g_ActiveShadersChangedNotify = notify;
	}

	void attach(ModuleObserver& observer) {
		g_observers.attach(observer);
	}
	void detach(ModuleObserver& observer) {
		g_observers.detach(observer);
	}

	void setLightingEnabled(bool enabled) {
		if (CShader::m_lightingEnabled != enabled) {
			for (shaders_t::const_iterator i = g_ActiveShaders.begin(); i != g_ActiveShaders.end(); ++i) {
				(*i).second->unrealiseLighting();
			}
			CShader::m_lightingEnabled = enabled;
			for (shaders_t::const_iterator i = g_ActiveShaders.begin(); i != g_ActiveShaders.end(); ++i) {
				(*i).second->realiseLighting();
			}
		}
	}

	const char* getTexturePrefix() const {
		return g_texturePrefix;
	}
};

UFOShaderSystem g_UFOShaderSystem;

ShaderSystem& GetShaderSystem() {
	return g_UFOShaderSystem;
}

void Shaders_Construct() {
	GlobalFileSystem().attach(g_UFOShaderSystem);
}
void Shaders_Destroy() {
	GlobalFileSystem().detach(g_UFOShaderSystem);

	if (Shaders_realised()) {
		Shaders_Free();
	}
}
