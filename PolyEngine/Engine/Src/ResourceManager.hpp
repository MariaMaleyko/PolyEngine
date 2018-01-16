#pragma once

#include <Core.hpp>
#include <Defines.hpp>
#include <String.hpp>
#include <FileIO.hpp>

#include "AssetsPathConfig.hpp"
#include "ResourceBase.hpp"
#include "OrderedMap.hpp"
#include <map>

namespace Poly
{
	class MeshResource;
	class TextureResource;
	class FontResource;
	class SoundResource;

	ENGINE_DLLEXPORT String LoadTextFileRelative(eResourceSource Source, const String& path);
	ENGINE_DLLEXPORT void SaveTextFileRelative(eResourceSource Source, const String& path, const String& text);

	namespace Impl { template<typename T> OrderedMap<String, std::unique_ptr<T>>& GetResources(); }

#define ENGINE_DECLARE_RESOURCE(type, map_name) \
	namespace Impl { \
		ENGINE_DLLEXPORT extern OrderedMap<String, std::unique_ptr<type>> map_name; \
		template<> inline OrderedMap<String, std::unique_ptr<type>>& GetResources<type>() { return map_name; } \
	}

#define DECLARE_RESOURCE(type, map_name) \
	namespace Impl { \
		GAME_DLLEXPORT extern OrderedMap<String, std::unique_ptr<type>> map_name; \
		template<> inline OrderedMap<String, std::unique_ptr<type>>& GetResources<type>() { return map_name; } \
	}

#define DEFINE_RESOURCE(type, map_name) namespace Poly { namespace Impl { OrderedMap<String, std::unique_ptr<type>> map_name = {}; }}

	ENGINE_DECLARE_RESOURCE(MeshResource, gMeshResourcesMap)
	ENGINE_DECLARE_RESOURCE(TextureResource, gTextureResourcesMap)
	ENGINE_DECLARE_RESOURCE(FontResource, gFontResourcesMap)
	ENGINE_DECLARE_RESOURCE(SoundResource, gALSoundResourcesMap)

	//------------------------------------------------------------------------------
	template<typename T>
	class ResourceManager
	{
	public:
		static T* LoadEngineAsset(const String& path)
		{
			return Load(path, true, eResourceSource::ENGINE);
		}

		static T* LoadGameAsset(const String& path)
		{
			return Load(path, true, eResourceSource::GAME);
		}

		static T* Load(const String& path, eResourceSource source = eResourceSource::NONE)
		{
			auto it = Impl::GetResources<T>().Entry(path);
			// Check if it is already loaded
			if (!it.IsVacant())  // T* - Texture, it - map. how to get access to value
			{
				T* resource = it.OrInsert(path);//.OccupiredGet()
				resource->AddRef();
				return resource;
			}

			// Load the resource
			gConsole.LogInfo("ResourceManager: Loading: {}", path);
			T* resource = nullptr;
			String absolutePath = gAssetsPathConfig.GetAssetsPath(source) + path;

			try
			{
				auto new_resource = new T(absolutePath);
				resource = new_resource;
			} catch (const ResourceLoadFailedException&) {
				resource = nullptr;
			} catch (const std::exception&) {
				HEAVY_ASSERTE(false, "Resource creation failed for unknown reason!");
				return nullptr;
			}

			if (!resource)
			{
				gConsole.LogError("Resource loading failed! {}", path);
				return nullptr;
			}

			Impl::GetResources<T>().Insert(path, std::unique_ptr<T>(resource));
			resource->Path = path;
			resource->AddRef();
			return resource;
		}

		//------------------------------------------------------------------------------
		static void Release(T* resource)
		{
			if (resource->RemoveRef())
			{
				auto it = Impl::GetResources<T>().Entry(resource->GetPath());
				HEAVY_ASSERTE(it.., "Resource creation failed!");
				//Map nie ma Geta. 
				Impl::GetResources<T>().Remove(resource->GetPath());// ::
			}
		}
	};
}