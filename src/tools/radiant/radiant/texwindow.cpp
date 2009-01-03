/**
 * @file texwindow.cpp
 * @brief Texture Window
 * @author Leonardo Zide (leo@lokigames.com)
 */

/*
Copyright (C) 1999-2006 Id Software, Inc. and contributors.
For a list of contributors, see the accompanying CONTRIBUTORS file.

This file is part of GtkRadiant.

GtkRadiant is free software; you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation; either version 2 of the License, or
(at your option) any later version.

GtkRadiant is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with GtkRadiant; if not, write to the Free Software
Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "texwindow.h"
#include "radiant.h"

#include "debugging/debugging.h"

#include "ifilesystem.h"
#include "iundo.h"
#include "igl.h"
#include "iarchive.h"
#include "moduleobserver.h"

#include <set>
#include <string>
#include <vector>

#include "signal/signal.h"
#include "math/vector.h"
#include "texturelib.h"
#include "string/string.h"
#include "shaderlib.h"
#include "os/file.h"
#include "os/path.h"
#include "stream/memstream.h"
#include "stream/textfilestream.h"
#include "stream/stringstream.h"
#include "texmanip.h"
#include "textures.h"
#include "convert.h"

#include "gtkutil/menu.h"
#include "gtkutil/nonmodal.h"
#include "gtkutil/cursor.h"
#include "gtkutil/widget.h"
#include "gtkutil/glwidget.h"
#include "gtkutil/messagebox.h"

#include "error.h"
#include "map.h"
#include "qgl.h"
#include "select.h"
#include "brush_primit.h"
#include "brushmanip.h"
#include "plugin.h"
#include "qe3.h"
#include "gtkmisc.h"
#include "mainframe.h"
#include "dialogs/findtextures.h"
#include "sidebar/sidebar.h"
#include "preferences.h"
#include "commands.h"
#include "xywindow.h"

void TextureBrowser_queueDraw(TextureBrowser& textureBrowser);

static bool string_equal_start(const char* string, StringRange start) {
	return string_equal_n(string, start.first, start.last - start.first);
}

typedef std::set<CopiedString> TextureGroups;

void TextureGroups_addShader(TextureGroups& groups, const char* shaderName) {
	const char* texture = path_make_relative(shaderName, "textures/");
	if (texture != shaderName) {
		const char* last = path_get_filename_start(texture);
		if (!string_empty(last)) {
			groups.insert(CopiedString(StringRange(texture, --last)));
		}
	}
}
typedef ReferenceCaller1<TextureGroups, const char*, TextureGroups_addShader> TextureGroupsAddShaderCaller;

void TextureGroups_addDirectory(TextureGroups& groups, const char* directory) {
	// skip svn subdirs
	if (strstr(directory, ".svn") != 0)
		return;

	groups.insert(directory);
}
typedef ReferenceCaller1<TextureGroups, const char*, TextureGroups_addDirectory> TextureGroupsAddDirectoryCaller;

namespace {
bool g_TextureBrowser_fixedSize = false;
}

class DeferredAdjustment {
	gdouble m_value;
	guint m_handler;
	typedef void (*ValueChangedFunction)(void* data, gdouble value);
	ValueChangedFunction m_function;
	void* m_data;

	static gboolean deferred_value_changed(gpointer data) {
		reinterpret_cast<DeferredAdjustment*>(data)->m_function(
			reinterpret_cast<DeferredAdjustment*>(data)->m_data,
			reinterpret_cast<DeferredAdjustment*>(data)->m_value
		);
		reinterpret_cast<DeferredAdjustment*>(data)->m_handler = 0;
		reinterpret_cast<DeferredAdjustment*>(data)->m_value = 0;
		return FALSE;
	}
public:
	DeferredAdjustment(ValueChangedFunction function, void* data) : m_value(0), m_handler(0), m_function(function), m_data(data) {
	}
	void flush() {
		if (m_handler != 0) {
			g_source_remove(m_handler);
			deferred_value_changed(this);
		}
	}
	void value_changed(gdouble value) {
		m_value = value;
		if (m_handler == 0) {
			m_handler = g_idle_add(deferred_value_changed, this);
		}
	}
	static void adjustment_value_changed(GtkAdjustment *adjustment, DeferredAdjustment* self) {
		self->value_changed(adjustment->value);
	}
};



class TextureBrowser;

typedef ReferenceCaller<TextureBrowser, TextureBrowser_queueDraw> TextureBrowserQueueDrawCaller;

void TextureBrowser_scrollChanged(void* data, gdouble value);

void TextureBrowser_hideUnusedExport(const BoolImportCallback& importer);
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_hideUnusedExport> TextureBrowserHideUnusedExport;

void TextureBrowser_showShadersExport(const BoolImportCallback& importer);
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_showShadersExport> TextureBrowserShowShadersExport;

void TextureBrowser_fixedSize(const BoolImportCallback& importer);
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_fixedSize> TextureBrowserFixedSizeExport;

class TextureBrowser {
public:
	int width, height;
	int originy;
	int m_nTotalHeight;
	WindowPositionTracker m_position_tracker;

	CopiedString shader;

	GtkWindow* m_parent;
	GtkWidget* m_gl_widget;
	GtkWidget* m_texture_scroll;
	GtkWidget* m_treeViewTree;
	GtkWidget* m_tag_frame;
	GtkListStore* m_assigned_store;
	GtkListStore* m_available_store;
	GtkWidget* m_assigned_tree;
	GtkWidget* m_available_tree;
	GtkWidget* m_scr_win_tree;
	GtkWidget* m_scr_win_tags;
	GtkWidget* m_tag_notebook;
	GtkWidget* m_search_button;

	std::set<CopiedString> m_all_tags;
	GtkListStore* m_all_tags_list;
	std::vector<CopiedString> m_copied_tags;
	std::set<CopiedString> m_found_shaders;

	ToggleItem m_hideunused_item;
	ToggleItem m_showshaders_item;
	ToggleItem m_fixedsize_item;

	guint m_sizeHandler;
	guint m_exposeHandler;

	bool m_heightChanged;
	bool m_originInvalid;

	DeferredAdjustment m_scrollAdjustment;
	FreezePointer m_freezePointer;

	Vector3 color_textureback;
	// the increment step we use against the wheel mouse
	std::size_t m_mouseWheelScrollIncrement;
	std::size_t m_textureScale;
	// make the texture increments match the grid changes
	bool m_showShaders;
	bool m_showTextureScrollbar;
	// if true, the texture window will only display in-use shaders
	// if false, all the shaders in memory are displayed
	bool m_hideUnused;
	bool m_rmbSelected;
	// The uniform size (in pixels) that textures are resized to when m_resizeTextures is true.
	int m_uniformTextureSize;
	// Return the display width of a texture in the texture browser
	int getTextureWidth(const qtexture_t* tex) {
		int width;
		if (!g_TextureBrowser_fixedSize) {
			// Don't use uniform size
			width = (int)(tex->width * ((float)m_textureScale / 100));
		} else if (tex->width >= tex->height) {
			// Texture is square, or wider than it is tall
			width = m_uniformTextureSize;
		} else {
			// Otherwise, preserve the texture's aspect ratio
			width = (int)(m_uniformTextureSize * ((float)tex->width / tex->height));
		}
		return width;
	}
	// Return the display height of a texture in the texture browser
	int getTextureHeight(const qtexture_t* tex) {
		int height;
		if (!g_TextureBrowser_fixedSize) {
			// Don't use uniform size
			height = (int)(tex->height * ((float)m_textureScale / 100));
		} else if (tex->height >= tex->width) {
			// Texture is square, or taller than it is wide
			height = m_uniformTextureSize;
		} else {
			// Otherwise, preserve the texture's aspect ratio
			height = (int)(m_uniformTextureSize * ((float)tex->height / tex->width));
		}
		return height;
	}

	TextureBrowser() :
			m_texture_scroll(0),
			m_hideunused_item(TextureBrowserHideUnusedExport()),
			m_showshaders_item(TextureBrowserShowShadersExport()),
			m_fixedsize_item(TextureBrowserFixedSizeExport()),
			m_heightChanged(true),
			m_originInvalid(true),
			m_scrollAdjustment(TextureBrowser_scrollChanged, this),
			color_textureback(0.25f, 0.25f, 0.25f),
			m_mouseWheelScrollIncrement(64),
			m_textureScale(50),
			m_showShaders(true),
			m_showTextureScrollbar(true),
			m_hideUnused(false),
			m_rmbSelected(false),
			m_uniformTextureSize(128) {
	}
};

static void (*TextureBrowser_textureSelected)(const char* shader);


static void TextureBrowser_updateScroll(TextureBrowser& textureBrowser);

static inline int TextureBrowser_fontHeight(TextureBrowser& textureBrowser) {
	return GlobalOpenGL().m_fontHeight;
}

const char* TextureBrowser_GetSelectedShader(TextureBrowser& textureBrowser) {
	if (!strcmp(textureBrowser.shader.c_str(), "textures/"))
		return "textures/tex_common/nodraw";
	return textureBrowser.shader.c_str();
}

/**
 * @brief Updates statusbar with texture information
 * @sa MainFrame::RedrawStatusText
 * @sa MainFrame::SetStatusText
 */
void TextureBrowser_SetStatus(TextureBrowser& textureBrowser, const char* name) {
	IShader* shader = QERApp_Shader_ForName(name);
	qtexture_t* q = shader->getTexture();
	StringOutputStream strTex(256);
	strTex << name << " W: " << Unsigned(q->width) << " H: " << Unsigned(q->height);
	shader->DecRef();
	g_pParentWnd->SetStatusText(g_pParentWnd->m_texture_status, strTex.c_str() + strlen("textures/"));
}

void TextureBrowser_Focus(TextureBrowser& textureBrowser, const char* name);

void TextureBrowser_SetSelectedShader(TextureBrowser& textureBrowser, const char* shader) {
	textureBrowser.shader = shader;
	TextureBrowser_SetStatus(textureBrowser, shader);
	TextureBrowser_Focus(textureBrowser, shader);

	if (FindTextureDialog_isOpen()) {
		FindTextureDialog_selectTexture(shader);
	}

	// disable the menu item "shader info" if no shader was selected
	IShader* ishader = QERApp_Shader_ForName(shader);

	ishader->DecRef();
}


CopiedString g_TextureBrowser_currentDirectory;

/*
============================================================================

TEXTURE LAYOUT

TTimo: now based on a rundown through all the shaders
NOTE: we expect the Active shaders count doesn't change during a Texture_StartPos .. Texture_NextPos cycle
  otherwise we may need to rely on a list instead of an array storage
============================================================================
*/

class TextureLayout {
public:
	// texture layout functions
	// TTimo: now based on shaders
	int current_x, current_y, current_row;
};

static void Texture_StartPos(TextureLayout& layout) {
	layout.current_x = 8;
	layout.current_y = -8;
	layout.current_row = 0;
}

static void Texture_NextPos(TextureBrowser& textureBrowser, TextureLayout& layout, qtexture_t* current_texture, int *x, int *y) {
	qtexture_t* q = current_texture;

	const int nWidth = textureBrowser.getTextureWidth(q);
	const int nHeight = textureBrowser.getTextureHeight(q);
	if (layout.current_x + nWidth > textureBrowser.width - 8 && layout.current_row) { // go to the next row unless the texture is the first on the row
		layout.current_x = 8;
		layout.current_y -= layout.current_row + TextureBrowser_fontHeight(textureBrowser) + 4;
		layout.current_row = 0;
	}

	*x = layout.current_x;
	*y = layout.current_y;

	// Is our texture larger than the row? If so, grow the
	// row height to match it

	if (layout.current_row < nHeight)
		layout.current_row = nHeight;

	// never go less than 96, or the names get all crunched up
	layout.current_x += nWidth < 96 ? 96 : nWidth;
	layout.current_x += 8;
}

// if texture_showinuse jump over non in-use textures
static bool Texture_IsShown(IShader* shader, bool show_shaders, bool hideUnused) {
	if (!shader_equal_prefix(shader->getName(), "textures/"))
		return false;

	if (!show_shaders && !shader->IsDefault())
		return false;

	if (hideUnused && !shader->IsInUse())
		return false;

	if (!shader_equal_prefix(shader_get_textureName(shader->getName()), g_TextureBrowser_currentDirectory.c_str()))
		return false;

	return true;
}

void TextureBrowser_heightChanged(TextureBrowser& textureBrowser) {
	textureBrowser.m_heightChanged = true;

	TextureBrowser_updateScroll(textureBrowser);
	TextureBrowser_queueDraw(textureBrowser);
}

void TextureBrowser_evaluateHeight(TextureBrowser& textureBrowser) {
	if (textureBrowser.m_heightChanged) {
		textureBrowser.m_heightChanged = false;

		textureBrowser.m_nTotalHeight = 0;

		TextureLayout layout;
		Texture_StartPos(layout);
		for (QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement()) {
			IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

			if (!Texture_IsShown(shader, textureBrowser.m_showShaders, textureBrowser.m_hideUnused))
				continue;

			int   x, y;
			Texture_NextPos(textureBrowser, layout, shader->getTexture(), &x, &y);
			textureBrowser.m_nTotalHeight = std::max(textureBrowser.m_nTotalHeight, abs(layout.current_y) + TextureBrowser_fontHeight(textureBrowser) + textureBrowser.getTextureHeight(shader->getTexture()) + 4);
		}
	}
}

int TextureBrowser_TotalHeight(TextureBrowser& textureBrowser) {
	TextureBrowser_evaluateHeight(textureBrowser);
	return textureBrowser.m_nTotalHeight;
}

inline const int& min_int(const int& left, const int& right) {
	return std::min(left, right);
}

void TextureBrowser_clampOriginY(TextureBrowser& textureBrowser) {
	if (textureBrowser.originy > 0) {
		textureBrowser.originy = 0;
	}
	int lower = min_int(textureBrowser.height - TextureBrowser_TotalHeight(textureBrowser), 0);
	if (textureBrowser.originy < lower) {
		textureBrowser.originy = lower;
	}
}

int TextureBrowser_getOriginY(TextureBrowser& textureBrowser) {
	if (textureBrowser.m_originInvalid) {
		textureBrowser.m_originInvalid = false;
		TextureBrowser_clampOriginY(textureBrowser);
		TextureBrowser_updateScroll(textureBrowser);
	}
	return textureBrowser.originy;
}

void TextureBrowser_setOriginY(TextureBrowser& textureBrowser, int originy) {
	textureBrowser.originy = originy;
	TextureBrowser_clampOriginY(textureBrowser);
	TextureBrowser_updateScroll(textureBrowser);
	TextureBrowser_queueDraw(textureBrowser);
}


Signal0 g_activeShadersChangedCallbacks;

void TextureBrowser_addActiveShadersChangedCallback(const SignalHandler& handler) {
	g_activeShadersChangedCallbacks.connectLast(handler);
}

class ShadersObserver : public ModuleObserver {
	Signal0 m_realiseCallbacks;
public:
	void realise() {
		m_realiseCallbacks();
	}
	void unrealise() {
	}
	void insert(const SignalHandler& handler) {
		m_realiseCallbacks.connectLast(handler);
	}
};

namespace {
ShadersObserver g_ShadersObserver;
}

void TextureBrowser_addShadersRealiseCallback(const SignalHandler& handler) {
	g_ShadersObserver.insert(handler);
}

void TextureBrowser_activeShadersChanged(TextureBrowser& textureBrowser) {
	TextureBrowser_heightChanged(textureBrowser);
	textureBrowser.m_originInvalid = true;

	g_activeShadersChangedCallbacks();
}

void TextureBrowser_importShowScrollbar(TextureBrowser& textureBrowser, bool value) {
	textureBrowser.m_showTextureScrollbar = value;
	if (textureBrowser.m_texture_scroll != 0) {
		widget_set_visible(textureBrowser.m_texture_scroll, textureBrowser.m_showTextureScrollbar);
		TextureBrowser_updateScroll(textureBrowser);
	}
}
typedef ReferenceCaller1<TextureBrowser, bool, TextureBrowser_importShowScrollbar> TextureBrowserImportShowScrollbarCaller;


/*
==============
TextureBrowser_ShowDirectory
relies on texture_directory global for the directory to use
1) Load the shaders for the given directory
2) Scan the remaining texture, load them and assign them a default shader (the "noshader" shader)
NOTE: when writing a texture plugin, or some texture extensions, this function may need to be overriden, and made
  available through the IShaders interface
NOTE: for texture window layout:
  all shaders are stored with alphabetical order after load
  previously loaded and displayed stuff is hidden, only in-use and newly loaded is shown
  ( the GL textures are not flushed though)
==============
*/
bool texture_name_ignore(const char* name) {
	StringOutputStream strTemp(string_length(name));
	strTemp << LowerCase(name);

	/* only show the dummy texture - the others should not be used directly */
	return strstr(strTemp.c_str(), "tex_terrain") != 0 &&
			strstr(strTemp.c_str(), "dummy") == 0;
}

class LoadShaderVisitor : public Archive::Visitor {
public:
	void visit(const char* name) {
		gchar *shaderName = g_path_get_dirname(name);
		// TODO: Fix this mess
		IShader* shader = QERApp_Shader_ForName(CopiedString(StringRange(name, path_get_filename_base_end(name))).c_str());
		shader->DecRef();
		g_free(shaderName);
	}
};

void TextureBrowser_SetHideUnused(TextureBrowser& textureBrowser, bool hideUnused);

GtkWindow* g_window_textures;

void TextureBrowser_toggleShow() {
	GtkWidget *widget = GTK_WIDGET(g_window_textures);
#if 0
	if (!widget_is_visible(widget)) {
		// workaround for strange gtk behaviour - modifying the contents of a window while it is not
		// visible causes the window position to change without sending a configure_event
		GlobalTextureBrowser().m_position_tracker.sync(GlobalTextureBrowser().m_parent);
	}
#endif
	widget_toggle_visible(widget);
}

class TextureCategoryLoadShader {
	const char* m_directory;
	std::size_t& m_count;
public:
	typedef const char* first_argument_type;

	TextureCategoryLoadShader(const char* directory, std::size_t& count)
			: m_directory(directory), m_count(count) {
		m_count = 0;
	}
	void operator()(const char* name) const {
		if (shader_equal_prefix(name, "textures/")
		        && shader_equal_prefix(name + string_length("textures/"), m_directory)) {
			++m_count;
			// request the shader, this will load the texture if needed
			// this Shader_ForName call is a kind of hack
			IShader *pFoo = QERApp_Shader_ForName(name);
			pFoo->DecRef();
		}
	}
};

void TextureDirectory_loadTexture(const char* directory, const char* texture) {
	StringOutputStream name(256);
	name << directory << StringRange(texture, path_get_filename_base_end(texture));

	if (texture_name_ignore(name.c_str())) {
		return;
	}

	if (!shader_valid(name.c_str())) {
		g_warning("Skipping invalid texture name: [%s]\n", name.c_str());
		return;
	}

	// if a texture is already in use to represent a shader, ignore it
	IShader* shader = QERApp_Shader_ForName(name.c_str());
	shader->DecRef();
}
typedef ConstPointerCaller1<char, const char*, TextureDirectory_loadTexture> TextureDirectoryLoadTextureCaller;

class LoadTexturesByTypeVisitor : public ImageModules::Visitor {
	const char* m_dirstring;
public:
	LoadTexturesByTypeVisitor(const char* dirstring)
			: m_dirstring(dirstring) {
	}
	void visit(const char* minor, const _QERPlugImageTable& table) const {
		GlobalFileSystem().forEachFile(m_dirstring, minor, TextureDirectoryLoadTextureCaller(m_dirstring));
	}
};

void TextureBrowser_ShowDirectory(TextureBrowser& textureBrowser, const char* directory) {
	g_TextureBrowser_currentDirectory = directory;
	TextureBrowser_heightChanged(textureBrowser);

	std::size_t shaders_count;
	GlobalShaderSystem().foreachShaderName(makeCallback1(TextureCategoryLoadShader(directory, shaders_count)));
	g_message("Showing %u shaders.\n", Unsigned(shaders_count));

	// load remaining texture files
	StringOutputStream dirstring(64);
	dirstring << "textures/" << directory;

	Radiant_getImageModules().foreachModule(LoadTexturesByTypeVisitor(dirstring.c_str()));

	// we'll display the newly loaded textures + all the ones already in use
	TextureBrowser_SetHideUnused(textureBrowser, false);
}

static bool TextureBrowser_hideUnused(void);

void TextureBrowser_hideUnusedExport(const BoolImportCallback& importer) {
	importer(TextureBrowser_hideUnused());
}
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_hideUnusedExport> TextureBrowserHideUnusedExport;

void TextureBrowser_showShadersExport(const BoolImportCallback& importer) {
	importer(GlobalTextureBrowser().m_showShaders);
}
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_showShadersExport> TextureBrowserShowShadersExport;

void TextureBrowser_fixedSize(const BoolImportCallback& importer) {
	importer(g_TextureBrowser_fixedSize);
}
typedef FreeCaller1<const BoolImportCallback&, TextureBrowser_fixedSize> TextureBrowser_FixedSizeExport;

void TextureBrowser_SetHideUnused(TextureBrowser& textureBrowser, bool hideUnused) {
	if (hideUnused) {
		textureBrowser.m_hideUnused = true;
	} else {
		textureBrowser.m_hideUnused = false;
	}

	textureBrowser.m_hideunused_item.update();

	TextureBrowser_heightChanged(textureBrowser);
	textureBrowser.m_originInvalid = true;
}

void TextureBrowser_ShowStartupShaders(TextureBrowser& textureBrowser) {
	TextureBrowser_ShowDirectory(textureBrowser, "tex_common/");
}


//++timo NOTE: this is a mix of Shader module stuff and texture explorer
// it might need to be split in parts or moved out .. dunno
// scroll origin so the specified texture is completely on screen
// if current texture is not displayed, nothing is changed
void TextureBrowser_Focus(TextureBrowser& textureBrowser, const char* name) {
	TextureLayout layout;
	// scroll origin so the texture is completely on screen
	Texture_StartPos(layout);

	for (QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement()) {
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if (!Texture_IsShown(shader, textureBrowser.m_showShaders, textureBrowser.m_hideUnused))
			continue;

		int x, y;
		Texture_NextPos(textureBrowser, layout, shader->getTexture(), &x, &y);
		const qtexture_t* q = shader->getTexture();
		if (!q)
			break;

		// we have found when texdef->name and the shader name match
		// NOTE: as everywhere else for our comparisons, we are not case sensitive
		if (shader_equal(name, shader->getName())) {
			const int textureHeight = (int)(q->height * ((float)textureBrowser.m_textureScale / 100))
			                    + 2 * TextureBrowser_fontHeight(textureBrowser);

			int originy = TextureBrowser_getOriginY(textureBrowser);
			if (y > originy) {
				originy = y;
			}

			if (y - textureHeight < originy - textureBrowser.height) {
				originy = (y - textureHeight) + textureBrowser.height;
			}

			TextureBrowser_setOriginY(textureBrowser, originy);
			return;
		}
	}
}

IShader* Texture_At(TextureBrowser& textureBrowser, int mx, int my) {
	my += TextureBrowser_getOriginY(textureBrowser) - textureBrowser.height;

	TextureLayout layout;
	Texture_StartPos(layout);
	for (QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement()) {
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if (!Texture_IsShown(shader, textureBrowser.m_showShaders, textureBrowser.m_hideUnused))
			continue;

		int   x, y;
		Texture_NextPos(textureBrowser, layout, shader->getTexture(), &x, &y);
		qtexture_t  *q = shader->getTexture();
		if (!q)
			break;

		int nWidth = textureBrowser.getTextureWidth(q);
		int nHeight = textureBrowser.getTextureHeight(q);
		if (mx > x && mx - x < nWidth
		        && my < y && y - my < nHeight + TextureBrowser_fontHeight(textureBrowser)) {
			return shader;
		}
	}

	return 0;
}

static void SelectTexture(TextureBrowser& textureBrowser, int mx, int my) {
	IShader* shader = Texture_At(textureBrowser, mx, my);
	if (shader != 0) {
		TextureBrowser_SetSelectedShader(textureBrowser, shader->getName());
		TextureBrowser_textureSelected(shader->getName());

		if (!FindTextureDialog_isOpen() && !textureBrowser.m_rmbSelected) {
			UndoableCommand undo("textureNameSetSelected");
			Select_SetShader(shader->getName());
		}
	}
}

/*
============================================================================

  MOUSE ACTIONS

============================================================================
*/

void TextureBrowser_trackingDelta(int x, int y, unsigned int state, void* data) {
	TextureBrowser& textureBrowser = *reinterpret_cast<TextureBrowser*>(data);
	if (y != 0) {
		int scale = 1;

		if (state & GDK_SHIFT_MASK)
			scale = 4;

		int originy = TextureBrowser_getOriginY(textureBrowser);
		originy += y * scale;
		TextureBrowser_setOriginY(textureBrowser, originy);
	}
}

void TextureBrowser_Tracking_MouseDown(TextureBrowser& textureBrowser) {
	textureBrowser.m_freezePointer.freeze_pointer(textureBrowser.m_parent, TextureBrowser_trackingDelta, &textureBrowser);
}

void TextureBrowser_Tracking_MouseUp(TextureBrowser& textureBrowser) {
	textureBrowser.m_freezePointer.unfreeze_pointer(textureBrowser.m_parent);
}

void TextureBrowser_Selection_MouseDown(TextureBrowser& textureBrowser, guint32 flags, int pointx, int pointy) {
	SelectTexture(textureBrowser, pointx, textureBrowser.height - 1 - pointy);
}

/*
============================================================================

DRAWING

============================================================================
*/

/*
============
Texture_Draw
TTimo: relying on the shaders list to display the textures
we must query all qtexture_t* to manage and display through the IShaders interface
this allows a plugin to completely override the texture system
============
*/
void Texture_Draw(TextureBrowser& textureBrowser) {
	int originy = TextureBrowser_getOriginY(textureBrowser);

	glClearColor(textureBrowser.color_textureback[0],
	             textureBrowser.color_textureback[1],
	             textureBrowser.color_textureback[2],
	             0);
	glViewport(0, 0, textureBrowser.width, textureBrowser.height);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();

	glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glDisable (GL_DEPTH_TEST);
	glDisable(GL_BLEND);
	glOrtho (0, textureBrowser.width, originy - textureBrowser.height, originy, -100, 100);
	glEnable (GL_TEXTURE_2D);

	glPolygonMode (GL_FRONT_AND_BACK, GL_FILL);

	int last_y = 0, last_height = 0;

	TextureLayout layout;
	Texture_StartPos(layout);
	for (QERApp_ActiveShaders_IteratorBegin(); !QERApp_ActiveShaders_IteratorAtEnd(); QERApp_ActiveShaders_IteratorIncrement()) {
		IShader* shader = QERApp_ActiveShaders_IteratorCurrent();

		if (!Texture_IsShown(shader, textureBrowser.m_showShaders, textureBrowser.m_hideUnused))
			continue;

		int x, y;
		Texture_NextPos(textureBrowser, layout, shader->getTexture(), &x, &y);
		const qtexture_t *q = shader->getTexture();
		if (!q)
			break;

		const int nWidth = textureBrowser.getTextureWidth(q);
		const int nHeight = textureBrowser.getTextureHeight(q);

		if (y != last_y) {
			last_y = y;
			last_height = 0;
		}
		last_height = std::max (nHeight, last_height);

		// Is this texture visible?
		if ((y - nHeight - TextureBrowser_fontHeight(textureBrowser) < originy)
		        && (y > originy - textureBrowser.height)) {
			// borders rules:
			// if it's the current texture, draw a thick red line, else:
			// shaders have a white border, simple textures don't
			// if !texture_showinuse: (some textures displayed may not be in use)
			// draw an additional square around with 0.5 1 0.5 color
			if (shader_equal(TextureBrowser_GetSelectedShader(textureBrowser), shader->getName())) {
				glLineWidth (3);
				if (textureBrowser.m_rmbSelected) {
					glColor3f (0, 0, 1);
				} else {
					glColor3f (1, 0, 0);
				}
				glDisable (GL_TEXTURE_2D);

				glBegin (GL_LINE_LOOP);
				glVertex2i (x - 4, y - TextureBrowser_fontHeight(textureBrowser) + 4);
				glVertex2i (x - 4, y - TextureBrowser_fontHeight(textureBrowser) - nHeight - 4);
				glVertex2i (x + 4 + nWidth, y - TextureBrowser_fontHeight(textureBrowser) - nHeight - 4);
				glVertex2i (x + 4 + nWidth, y - TextureBrowser_fontHeight(textureBrowser) + 4);
				glEnd();

				glEnable (GL_TEXTURE_2D);
				glLineWidth (1);
			} else {
				glLineWidth (1);
				// shader border:
				if (!shader->IsDefault()) {
					glColor3f (1, 1, 1);
					glDisable (GL_TEXTURE_2D);

					glBegin (GL_LINE_LOOP);
					glVertex2i (x - 1, y + 1 - TextureBrowser_fontHeight(textureBrowser));
					glVertex2i (x - 1, y - nHeight - 1 - TextureBrowser_fontHeight(textureBrowser));
					glVertex2i (x + 1 + nWidth, y - nHeight - 1 - TextureBrowser_fontHeight(textureBrowser));
					glVertex2i (x + 1 + nWidth, y + 1 - TextureBrowser_fontHeight(textureBrowser));
					glEnd();
					glEnable (GL_TEXTURE_2D);
				}

				// highlight in-use textures
				if (!textureBrowser.m_hideUnused && shader->IsInUse()) {
					glColor3f (0.5, 1, 0.5);
					glDisable (GL_TEXTURE_2D);
					glBegin (GL_LINE_LOOP);
					glVertex2i (x - 3, y + 3 - TextureBrowser_fontHeight(textureBrowser));
					glVertex2i (x - 3, y - nHeight - 3 - TextureBrowser_fontHeight(textureBrowser));
					glVertex2i (x + 3 + nWidth, y - nHeight - 3 - TextureBrowser_fontHeight(textureBrowser));
					glVertex2i (x + 3 + nWidth, y + 3 - TextureBrowser_fontHeight(textureBrowser));
					glEnd();
					glEnable (GL_TEXTURE_2D);
				}
			}

			// Draw the texture
			glBindTexture (GL_TEXTURE_2D, q->texture_number);
			glColor3f (1, 1, 1);
			glBegin (GL_QUADS);
			glTexCoord2i (0, 0);
			glVertex2i (x, y - TextureBrowser_fontHeight(textureBrowser));
			glTexCoord2i (1, 0);
			glVertex2i (x + nWidth, y - TextureBrowser_fontHeight(textureBrowser));
			glTexCoord2i (1, 1);
			glVertex2i (x + nWidth, y - TextureBrowser_fontHeight(textureBrowser) - nHeight);
			glTexCoord2i (0, 1);
			glVertex2i (x, y - TextureBrowser_fontHeight(textureBrowser) - nHeight);
			glEnd();

			// draw the texture name
			glDisable (GL_TEXTURE_2D);
			glColor3f (1, 1, 1);

			glRasterPos2i (x, y - TextureBrowser_fontHeight(textureBrowser) + 5);

			// don't draw the directory name
			const char* name = shader->getName();
			name += strlen(name);
			while (name != shader->getName() && *(name - 1) != '/' && *(name - 1) != '\\')
				name--;

			GlobalOpenGL().drawString(name);
			glEnable (GL_TEXTURE_2D);
		}

		//int totalHeight = abs(y) + last_height + TextureBrowser_fontHeight(textureBrowser) + 4;
	}


	// reset the current texture
	glBindTexture(GL_TEXTURE_2D, 0);
	//qglFinish();
}

void TextureBrowser_queueDraw(TextureBrowser& textureBrowser) {
	if (textureBrowser.m_gl_widget != 0) {
		gtk_widget_queue_draw(textureBrowser.m_gl_widget);
	}
}


void TextureBrowser_setScale(TextureBrowser& textureBrowser, std::size_t scale) {
	textureBrowser.m_textureScale = scale;

	TextureBrowser_queueDraw(textureBrowser);
}


void TextureBrowser_MouseWheel(TextureBrowser& textureBrowser, bool bUp) {
	int originy = TextureBrowser_getOriginY(textureBrowser);

	if (bUp) {
		originy += int(textureBrowser.m_mouseWheelScrollIncrement);
	} else {
		originy -= int(textureBrowser.m_mouseWheelScrollIncrement);
	}

	TextureBrowser_setOriginY(textureBrowser, originy);
}

gboolean TextureBrowser_button_press(GtkWidget* widget, GdkEventButton* event, TextureBrowser* textureBrowser) {
	if (event->type == GDK_BUTTON_PRESS) {
		if (event->button == 3) {
			TextureBrowser_Tracking_MouseDown(*textureBrowser);
		} else if (event->button == 1) {
			TextureBrowser_Selection_MouseDown(*textureBrowser, event->state, static_cast<int>(event->x), static_cast<int>(event->y));
		}
	}
	return FALSE;
}

gboolean TextureBrowser_button_release(GtkWidget* widget, GdkEventButton* event, TextureBrowser* textureBrowser) {
	if (event->type == GDK_BUTTON_RELEASE) {
		if (event->button == 3) {
			TextureBrowser_Tracking_MouseUp(*textureBrowser);
		}
	}
	return FALSE;
}

gboolean TextureBrowser_motion(GtkWidget *widget, GdkEventMotion *event, TextureBrowser* textureBrowser) {
	return FALSE;
}

gboolean TextureBrowser_scroll(GtkWidget* widget, GdkEventScroll* event, TextureBrowser* textureBrowser) {
	if (event->direction == GDK_SCROLL_UP) {
		TextureBrowser_MouseWheel(*textureBrowser, true);
	} else if (event->direction == GDK_SCROLL_DOWN) {
		TextureBrowser_MouseWheel(*textureBrowser, false);
	}
	return FALSE;
}

void TextureBrowser_scrollChanged(void* data, gdouble value) {
	//globalOutputStream() << "vertical scroll\n";
	TextureBrowser_setOriginY(*reinterpret_cast<TextureBrowser*>(data), -(int)value);
}

static void TextureBrowser_verticalScroll(GtkAdjustment *adjustment, TextureBrowser* textureBrowser) {
	textureBrowser->m_scrollAdjustment.value_changed(adjustment->value);
}

static void TextureBrowser_updateScroll (TextureBrowser& textureBrowser) {
	if (textureBrowser.m_showTextureScrollbar) {
		int totalHeight = TextureBrowser_TotalHeight(textureBrowser);

		totalHeight = std::max(totalHeight, textureBrowser.height);

		GtkAdjustment *vadjustment = gtk_range_get_adjustment(GTK_RANGE(textureBrowser.m_texture_scroll));

		vadjustment->value = -TextureBrowser_getOriginY(textureBrowser);
		vadjustment->page_size = textureBrowser.height;
		vadjustment->page_increment = textureBrowser.height / 2;
		vadjustment->step_increment = 20;
		vadjustment->lower = 0;
		vadjustment->upper = totalHeight;

		g_signal_emit_by_name(G_OBJECT (vadjustment), "changed");
	}
}

static gboolean TextureBrowser_size_allocate (GtkWidget* widget, GtkAllocation* allocation, TextureBrowser* textureBrowser) {
	textureBrowser->width = allocation->width;
	textureBrowser->height = allocation->height;
	TextureBrowser_heightChanged(*textureBrowser);
	textureBrowser->m_originInvalid = true;
	TextureBrowser_queueDraw(*textureBrowser);
	return FALSE;
}

static gboolean TextureBrowser_expose (GtkWidget* widget, GdkEventExpose* event, TextureBrowser* textureBrowser) {
	if (glwidget_make_current(textureBrowser->m_gl_widget) != FALSE) {
		TextureBrowser_evaluateHeight(*textureBrowser);
		Texture_Draw(*textureBrowser);
		glwidget_swap_buffers(textureBrowser->m_gl_widget);
	}
	return FALSE;
}

static TextureBrowser g_TextureBrowser;

TextureBrowser& GlobalTextureBrowser (void) {
	return g_TextureBrowser;
}

static bool TextureBrowser_hideUnused (void) {
	return g_TextureBrowser.m_hideUnused;
}

void TextureBrowser_ToggleHideUnused (void) {
	if (g_TextureBrowser.m_hideUnused) {
		TextureBrowser_SetHideUnused(g_TextureBrowser, false);
	} else {
		TextureBrowser_SetHideUnused(g_TextureBrowser, true);
	}
}

/**
 * @brief Generates the treeview with the subdirs in textures/
 */
static void TextureGroups_constructTreeModel (TextureGroups groups, GtkTreeStore* store) {
	GtkTreeIter iter, child;

	TextureGroups::const_iterator i = groups.begin();
	while (i != groups.end()) {
		const char* dirName = (*i).c_str();
		const char* firstUnderscore = strchr(dirName, '_');
		StringRange dirRoot(dirName, (firstUnderscore == 0) ? dirName : firstUnderscore + 1);

		TextureGroups::const_iterator next = i;
		++next;
		if (firstUnderscore != 0 && next != groups.end() && string_equal_start((*next).c_str(), dirRoot)) {
			gtk_tree_store_append(store, &iter, NULL);
			gtk_tree_store_set(store, &iter, 0, CopiedString(StringRange(dirName, firstUnderscore)).c_str(), -1);

			// keep going...
			while (i != groups.end() && string_equal_start((*i).c_str(), dirRoot)) {
				gtk_tree_store_append(store, &child, &iter);
				gtk_tree_store_set(store, &child, 0, (*i).c_str(), -1);
				++i;
			}
		} else {
			gtk_tree_store_append(store, &iter, NULL);
			gtk_tree_store_set(store, &iter, 0, dirName, -1);
			++i;
		}
	}
}

static TextureGroups TextureGroups_constructTreeView (void) {
	TextureGroups groups;

	GlobalFileSystem().forEachDirectory("textures/", TextureGroupsAddDirectoryCaller(groups));
	GlobalShaderSystem().foreachShaderName(TextureGroupsAddShaderCaller(groups));

	return groups;
}

static void TextureBrowser_constructTreeStore (void) {
	TextureGroups groups = TextureGroups_constructTreeView();
	GtkTreeStore* store = gtk_tree_store_new(1, G_TYPE_STRING);
	TextureGroups_constructTreeModel(groups, store);

	GtkTreeModel* model = GTK_TREE_MODEL(store);

	gtk_tree_view_set_model(GTK_TREE_VIEW(g_TextureBrowser.m_treeViewTree), model);
	gtk_tree_view_expand_all(GTK_TREE_VIEW(g_TextureBrowser.m_treeViewTree));

	g_object_unref(G_OBJECT(store));
}

static void TreeView_onRowActivated(GtkTreeView* treeview, GtkTreePath* path, GtkTreeViewColumn* col, gpointer userdata) {
	GtkTreeIter iter;

	GtkTreeModel* model = gtk_tree_view_get_model(GTK_TREE_VIEW(treeview));

	if (gtk_tree_model_get_iter (model, &iter, path)) {
		gchar dirName[1024];

		gchar* buffer;
		gtk_tree_model_get(model, &iter, 0, &buffer, -1);
		strcpy(dirName, buffer);
		g_free(buffer);
		strcat(dirName, "/");

		ScopeDisableScreenUpdates disableScreenUpdates(dirName, _("Loading Textures"));
		TextureBrowser_ShowDirectory(GlobalTextureBrowser (), dirName);
		TextureBrowser_queueDraw(GlobalTextureBrowser ());
	}
}

static void TextureBrowser_createTreeViewTree (void) {
	GtkCellRenderer* renderer;
	g_TextureBrowser.m_treeViewTree = GTK_WIDGET(gtk_tree_view_new());
	gtk_tree_view_set_enable_search(GTK_TREE_VIEW(g_TextureBrowser.m_treeViewTree), FALSE);

	gtk_tree_view_set_headers_visible(GTK_TREE_VIEW(g_TextureBrowser.m_treeViewTree), FALSE);
	g_signal_connect(g_TextureBrowser.m_treeViewTree, "row-activated", (GCallback) TreeView_onRowActivated, NULL);

	renderer = gtk_cell_renderer_text_new();
	gtk_tree_view_insert_column_with_attributes(GTK_TREE_VIEW(g_TextureBrowser.m_treeViewTree), -1, "", renderer, "text", 0, (char const*)0);

	TextureBrowser_constructTreeStore();
}

static GtkMenuItem* TextureBrowser_constructViewMenu (GtkMenu* menu) {
	GtkMenuItem* textures_menu_item = new_sub_menu_item_with_mnemonic(_("_View"));

	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff (menu);

	create_check_menu_item_with_mnemonic(menu, _("Hide _Unused"), "ShowInUse");

	menu_separator(menu);
	create_menu_item_with_mnemonic(menu, _("Show All"), "ShowAllTextures");
	create_check_menu_item_with_mnemonic(menu, _("Show shaders"), "ToggleShowShaders");
	create_check_menu_item_with_mnemonic(menu, _("Fixed Size"), "FixedSize");

	return textures_menu_item;
}

static GtkMenuItem* TextureBrowser_constructToolsMenu (GtkMenu* menu) {
	GtkMenuItem* textures_menu_item = new_sub_menu_item_with_mnemonic(_("_Tools"));

	if (g_Layout_enableDetachableMenus.m_value)
		menu_tearoff (menu);

	create_menu_item_with_mnemonic(menu, _("Flush & Reload Shaders"), "RefreshShaders");
	//! @todo some better place for this?
	create_menu_item_with_mnemonic(menu, _("Find / Replace..."), "FindReplaceTextures");

	return textures_menu_item;
}

GtkWidget* TextureBrowser_constructWindow (GtkWindow* toplevel) {
	GlobalShaderSystem().setActiveShadersChangedNotify(ReferenceCaller<TextureBrowser, TextureBrowser_activeShadersChanged>(g_TextureBrowser));

	g_TextureBrowser.m_parent = toplevel;
	g_TextureBrowser.m_position_tracker.connect(toplevel);

	GtkWidget* table = gtk_table_new(3, 3, FALSE);
	GtkWidget* vbox = gtk_vbox_new(FALSE, 0);
	gtk_table_attach(GTK_TABLE(table), vbox, 0, 1, 1, 3, GTK_FILL, GTK_FILL, 0, 0);
	gtk_widget_show(vbox);

	GtkWidget* menu_bar;

	{ // menu bar
		menu_bar = gtk_menu_bar_new();
		GtkWidget* menu_view = gtk_menu_new();
		GtkWidget* view_item = (GtkWidget*)TextureBrowser_constructViewMenu(GTK_MENU(menu_view));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), menu_view);
		gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), view_item);

		GtkWidget* menu_tools = gtk_menu_new();
		GtkWidget* tools_item = (GtkWidget*)TextureBrowser_constructToolsMenu(GTK_MENU(menu_tools));
		gtk_menu_item_set_submenu(GTK_MENU_ITEM(tools_item), menu_tools);
		gtk_menu_bar_append(GTK_MENU_BAR(menu_bar), tools_item);

		gtk_table_attach(GTK_TABLE (table), menu_bar, 0, 3, 0, 1, GTK_FILL, GTK_SHRINK, 0, 0);
		gtk_widget_show(menu_bar);
	}
	{ // Texture TreeView
		g_TextureBrowser.m_scr_win_tree = gtk_scrolled_window_new(NULL, NULL);
		gtk_container_set_border_width(GTK_CONTAINER(g_TextureBrowser.m_scr_win_tree), 0);

		// vertical only scrolling for treeview
		gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(g_TextureBrowser.m_scr_win_tree), GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

		gtk_widget_show(g_TextureBrowser.m_scr_win_tree);

		TextureBrowser_createTreeViewTree();

		gtk_scrolled_window_add_with_viewport(GTK_SCROLLED_WINDOW(g_TextureBrowser.m_scr_win_tree), GTK_WIDGET(g_TextureBrowser.m_treeViewTree));
		gtk_widget_show(GTK_WIDGET(g_TextureBrowser.m_treeViewTree));
	}
	{ // gl_widget scrollbar
		GtkWidget* w = gtk_vscrollbar_new(GTK_ADJUSTMENT(gtk_adjustment_new (0, 0, 0, 1, 1, 1)));
		gtk_table_attach(GTK_TABLE (table), w, 2, 3, 1, 2, GTK_SHRINK, GTK_FILL, 0, 0);
		gtk_widget_show(w);
		g_TextureBrowser.m_texture_scroll = w;

		GtkAdjustment *vadjustment = gtk_range_get_adjustment (GTK_RANGE (g_TextureBrowser.m_texture_scroll));
		g_signal_connect(G_OBJECT(vadjustment), "value_changed", G_CALLBACK(TextureBrowser_verticalScroll), &g_TextureBrowser);

		widget_set_visible(g_TextureBrowser.m_texture_scroll, g_TextureBrowser.m_showTextureScrollbar);
	}
	{ // gl_widget
		g_TextureBrowser.m_gl_widget = glwidget_new(FALSE);
		// TODO: store these values in the config file and reuse them
		gtk_widget_set_size_request(GTK_WIDGET(g_TextureBrowser.m_gl_widget), 800, 600);
		gtk_widget_ref(g_TextureBrowser.m_gl_widget);

		gtk_widget_set_events(g_TextureBrowser.m_gl_widget, GDK_DESTROY | GDK_EXPOSURE_MASK | GDK_BUTTON_PRESS_MASK | GDK_BUTTON_RELEASE_MASK | GDK_POINTER_MOTION_MASK | GDK_SCROLL_MASK);
		GTK_WIDGET_SET_FLAGS(g_TextureBrowser.m_gl_widget, GTK_CAN_FOCUS);

		gtk_table_attach_defaults(GTK_TABLE(table), g_TextureBrowser.m_gl_widget, 1, 2, 1, 2);
		gtk_widget_show(g_TextureBrowser.m_gl_widget);

		g_TextureBrowser.m_sizeHandler = g_signal_connect(G_OBJECT(g_TextureBrowser.m_gl_widget), "size_allocate", G_CALLBACK(TextureBrowser_size_allocate), &g_TextureBrowser);
		g_TextureBrowser.m_exposeHandler = g_signal_connect(G_OBJECT(g_TextureBrowser.m_gl_widget), "expose_event", G_CALLBACK(TextureBrowser_expose), &g_TextureBrowser);

		g_signal_connect(G_OBJECT(g_TextureBrowser.m_gl_widget), "button_press_event", G_CALLBACK(TextureBrowser_button_press), &g_TextureBrowser);
		g_signal_connect(G_OBJECT(g_TextureBrowser.m_gl_widget), "button_release_event", G_CALLBACK(TextureBrowser_button_release), &g_TextureBrowser);
		g_signal_connect(G_OBJECT(g_TextureBrowser.m_gl_widget), "motion_notify_event", G_CALLBACK(TextureBrowser_motion), &g_TextureBrowser);
		g_signal_connect(G_OBJECT(g_TextureBrowser.m_gl_widget), "scroll_event", G_CALLBACK(TextureBrowser_scroll), &g_TextureBrowser);
	}

	gtk_box_pack_start(GTK_BOX(vbox), g_TextureBrowser.m_scr_win_tree, TRUE, TRUE, 0);

	return table;
}

void TextureBrowser_destroyWindow (void) {
	GlobalShaderSystem().setActiveShadersChangedNotify(Callback());

	g_signal_handler_disconnect(G_OBJECT(g_TextureBrowser.m_gl_widget), g_TextureBrowser.m_sizeHandler);
	g_signal_handler_disconnect(G_OBJECT(g_TextureBrowser.m_gl_widget), g_TextureBrowser.m_exposeHandler);

	gtk_widget_unref(g_TextureBrowser.m_gl_widget);
}

const Vector3& TextureBrowser_getBackgroundColour(TextureBrowser& textureBrowser) {
	return textureBrowser.color_textureback;
}

void TextureBrowser_setBackgroundColour(TextureBrowser& textureBrowser, const Vector3& colour) {
	textureBrowser.color_textureback = colour;
	TextureBrowser_queueDraw(textureBrowser);
}

void RefreshShaders(void) {
	ScopeDisableScreenUpdates disableScreenUpdates(_("Processing..."), _("Loading Shaders"));
	GlobalShaderSystem().refresh();
	UpdateAllWindows();
}

void TextureBrowser_ToggleShowShaders(void) {
	g_TextureBrowser.m_showShaders ^= 1;
	g_TextureBrowser.m_showshaders_item.update();
	TextureBrowser_queueDraw(g_TextureBrowser);
}

void TextureBrowser_showAll(void) {
	g_TextureBrowser_currentDirectory = "";
	TextureBrowser_heightChanged(g_TextureBrowser);
}

void TextureBrowser_FixedSize(void) {
	g_TextureBrowser_fixedSize ^= 1;
	GlobalTextureBrowser().m_fixedsize_item.update();
	TextureBrowser_activeShadersChanged(GlobalTextureBrowser());
}

void TextureScaleImport (TextureBrowser& textureBrowser, int value) {
	switch (value) {
	case 0:
		TextureBrowser_setScale(textureBrowser, 10);
		break;
	case 1:
		TextureBrowser_setScale(textureBrowser, 25);
		break;
	case 2:
		TextureBrowser_setScale(textureBrowser, 50);
		break;
	case 3:
		TextureBrowser_setScale(textureBrowser, 100);
		break;
	case 4:
		TextureBrowser_setScale(textureBrowser, 200);
		break;
	}
}
typedef ReferenceCaller1<TextureBrowser, int, TextureScaleImport> TextureScaleImportCaller;

void TextureScaleExport (TextureBrowser& textureBrowser, const IntImportCallback& importer) {
	switch (textureBrowser.m_textureScale) {
	case 10:
		importer(0);
		break;
	case 25:
		importer(1);
		break;
	case 50:
		importer(2);
		break;
	case 100:
		importer(3);
		break;
	case 200:
		importer(4);
		break;
	}
}
typedef ReferenceCaller1<TextureBrowser, const IntImportCallback&, TextureScaleExport> TextureScaleExportCaller;

static void TextureBrowser_constructPreferences (PreferencesPage& page) {
	page.appendCheckBox(
		"", "Texture scrollbar",
		TextureBrowserImportShowScrollbarCaller(GlobalTextureBrowser()),
		BoolExportCaller(GlobalTextureBrowser().m_showTextureScrollbar)
	);
	{
		const char* texture_scale[] = { "10%", "25%", "50%", "100%", "200%" };
		page.appendCombo(
			_("Texture Thumbnail Scale"),
			STRING_ARRAY_RANGE(texture_scale),
			IntImportCallback(TextureScaleImportCaller(GlobalTextureBrowser())),
			IntExportCallback(TextureScaleExportCaller(GlobalTextureBrowser()))
		);
	}
	page.appendEntry(_("Mousewheel Increment"), GlobalTextureBrowser().m_mouseWheelScrollIncrement);
}

void TextureBrowser_constructPage (PreferenceGroup& group) {
	PreferencesPage page(group.createPage(_("Texture Browser"), _("Texture Browser Preferences")));
	TextureBrowser_constructPreferences(page);
}

static void TextureBrowser_registerPreferencesPage (void) {
	PreferencesDialog_addSettingsPage(FreeCaller1<PreferenceGroup&, TextureBrowser_constructPage>());
}


#include "preferencesystem.h"
#include "stringio.h"

typedef ReferenceCaller1<TextureBrowser, std::size_t, TextureBrowser_setScale> TextureBrowserSetScaleCaller;

void TextureClipboard_textureSelected(const char* shader);

void TextureBrowser_Construct (void)
{
	GlobalCommands_insert("RefreshShaders", FreeCaller<RefreshShaders>());
	GlobalToggles_insert("ShowInUse", FreeCaller<TextureBrowser_ToggleHideUnused>(), ToggleItem::AddCallbackCaller(GlobalTextureBrowser().m_hideunused_item), Accelerator('U'));
	GlobalCommands_insert("ShowAllTextures", FreeCaller<TextureBrowser_showAll>(), Accelerator('A', (GdkModifierType)GDK_CONTROL_MASK));
	GlobalCommands_insert("ToggleTextures", FreeCaller<TextureBrowser_toggleShow>(), Accelerator('T'));
	GlobalCommands_insert("ToggleBackground", FreeCaller<WXY_BackgroundSelect>());
	GlobalToggles_insert("ToggleShowShaders", FreeCaller<TextureBrowser_ToggleShowShaders>(), ToggleItem::AddCallbackCaller(GlobalTextureBrowser().m_showshaders_item));
	GlobalToggles_insert("FixedSize", FreeCaller<TextureBrowser_FixedSize>(), ToggleItem::AddCallbackCaller(GlobalTextureBrowser().m_fixedsize_item));

	GlobalPreferenceSystem().registerPreference("TextureScale",
		makeSizeStringImportCallback(TextureBrowserSetScaleCaller(GlobalTextureBrowser())),
		SizeExportStringCaller(GlobalTextureBrowser().m_textureScale));
	GlobalPreferenceSystem().registerPreference("TextureScrollbar",
		makeBoolStringImportCallback(TextureBrowserImportShowScrollbarCaller(GlobalTextureBrowser())),
		BoolExportStringCaller(GlobalTextureBrowser().m_showTextureScrollbar));
	GlobalPreferenceSystem().registerPreference("ShowShaders", BoolImportStringCaller(GlobalTextureBrowser().m_showShaders), BoolExportStringCaller(GlobalTextureBrowser().m_showShaders));
	GlobalPreferenceSystem().registerPreference("FixedSize", BoolImportStringCaller(g_TextureBrowser_fixedSize), BoolExportStringCaller(g_TextureBrowser_fixedSize));
	GlobalPreferenceSystem().registerPreference("WheelMouseInc", SizeImportStringCaller(GlobalTextureBrowser().m_mouseWheelScrollIncrement), SizeExportStringCaller(GlobalTextureBrowser().m_mouseWheelScrollIncrement));
	GlobalPreferenceSystem().registerPreference("SI_Colors0", Vector3ImportStringCaller(GlobalTextureBrowser().color_textureback), Vector3ExportStringCaller(GlobalTextureBrowser().color_textureback));
	GlobalPreferenceSystem().registerPreference("TextureWnd", WindowPositionTrackerImportStringCaller(GlobalTextureBrowser().m_position_tracker), WindowPositionTrackerExportStringCaller(GlobalTextureBrowser().m_position_tracker));

	GlobalTextureBrowser().shader = texdef_name_default();

	Textures_setModeChangedNotify(ReferenceCaller<TextureBrowser, TextureBrowser_queueDraw>(GlobalTextureBrowser()));

	TextureBrowser_registerPreferencesPage();

	GlobalShaderSystem().attach(g_ShadersObserver);

	TextureBrowser_textureSelected = TextureClipboard_textureSelected;
}

void TextureBrowser_Destroy (void)
{
	GlobalShaderSystem().detach(g_ShadersObserver);

	Textures_setModeChangedNotify(Callback());
}
