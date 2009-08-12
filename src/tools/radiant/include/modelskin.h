/*
 Copyright (C) 2001-2006, William Joseph.
 All Rights Reserved.

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

#if !defined(INCLUDED_MODELSKIN_H)
#define INCLUDED_MODELSKIN_H

#include "generic/constant.h"
#include "generic/callbackfwd.h"

#include <vector>
#include <string>

class SkinRemap
{
	public:
		const char* m_from;
		const char* m_to;
		SkinRemap (const char* from, const char* to) :
			m_from(from), m_to(to)
		{
		}
};

typedef Callback1<SkinRemap> SkinRemapCallback;
class ModuleObserver;

class ModelSkin
{
	public:
		STRING_CONSTANT(Name, "ModelSkin");

		virtual ~ModelSkin ()
		{
		}
		/// \brief Attach an \p observer whose realise() and unrealise() methods will be called when the skin is loaded or unloaded.
		virtual void attach (ModuleObserver& observer) = 0;
		/// \brief Detach an \p observer previously-attached by calling \c attach.
		virtual void detach (ModuleObserver& observer) = 0;
		/// \brief Returns true if this skin is currently loaded.
		virtual bool realised () const = 0;
		/// \brief Returns the shader identifier that \p name remaps to, or "" if not found or not realised.
		virtual const char* getRemap (const std::string& name) const = 0;
		/// \brief Calls \p callback for each remap pair. Has no effect if not realised.
		virtual void forEachRemap (const SkinRemapCallback& callback) const = 0;
};

class SkinnedModel
{
	public:
		STRING_CONSTANT(Name, "SkinnedModel");
		virtual ~SkinnedModel ()
		{
		}
		/// \brief Instructs the skinned model to update its skin.
		virtual void skinChanged () = 0;
};

// Model skinlist typedef
typedef std::vector<std::string> ModelSkinList;

class ModelSkinCache
{
	public:
		INTEGER_CONSTANT(Version, 1);
		STRING_CONSTANT(Name, "modelskin");
		virtual ~ModelSkinCache ()
		{
		}
		/// \brief Increments the reference count of and returns a reference to the skin uniquely identified by 'name'.
		virtual ModelSkin& capture (const std::string& name) = 0;
		/// \brief Decrements the reference-count of the skin uniquely identified by 'name'.
		virtual void release (const std::string& name) = 0;

		/**
		 * @brief Return the skins associated with the given model.
		 *
		 * @param
		 * The full pathname of the model, as given by the "model" key in the skin definition.
		 *
		 * @returns
		 * A vector of strings, each identifying the name of a skin which is associated with the
		 * given model. The vector may be empty as a model does not require any associated skins.
		 */
		virtual const ModelSkinList& getSkinsForModel (const std::string& modelName) = 0;
};

#include "modulesystem.h"

template<typename Type>
class GlobalModule;
typedef GlobalModule<ModelSkinCache> GlobalModelSkinCacheModule;

template<typename Type>
class GlobalModuleRef;
typedef GlobalModuleRef<ModelSkinCache> GlobalModelSkinCacheModuleRef;

inline ModelSkinCache& GlobalModelSkinCache ()
{
	return GlobalModelSkinCacheModule::getTable();
}

#endif
