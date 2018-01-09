#pragma once

#include <unordered_map>
#include <Core.hpp>

#include "Entity.hpp"
#include "Engine.hpp"

#include "ComponentBase.hpp"

namespace Poly {

	namespace DeferredTaskSystem
	{
		UniqueID ENGINE_DLLEXPORT SpawnEntityImmediate(World* w);
		void ENGINE_DLLEXPORT DestroyEntityImmediate(World* w, const UniqueID& entityId);
		template<typename T, typename ...Args> T* AddComponentImmediate(World* w, const UniqueID & entityId, Args && ...args);
		template<typename T, typename ...Args> T* AddWorldComponentImmediate(World* w, Args && ...args);
		template<typename T> void RemoveWorldComponentImmediate(World* w);
	}
	struct InputState;

	/// <summary>Entities per world limit.</summary>
	constexpr size_t MAX_ENTITY_COUNT = 65536;

	/// <summary>World components in limit.</summary>
	constexpr size_t MAX_WORLD_COMPONENTS_COUNT = 64;

	/// <summary>World represents world/scene/level in engine.
	/// It contains entities, its components and world components.</summary>
	class ENGINE_DLLEXPORT World : public BaseObject<>
	{
	public:
		/// <summary>Allocates memory for entities, world components and components allocators.</summary>
		World();

		virtual ~World();

		/// <summary>Gets a component of a specified type from entity with given UniqueID.</summary>
		/// <param name="entityId">UniqueID of the entity.</param>
		/// <returns>Pointer to a specified component or a nullptr, if none was found.</returns>
		/// <see cref="World.AddComponent()">
		/// <see cref="World.RemoveComponent()">
		template<typename T>
		T* GetComponent(const UniqueID& entityId)
		{
			HEAVY_ASSERTE(!!entityId, "Invalid entity ID");
			auto iter = IDToEntityMap.find(entityId);
			HEAVY_ASSERTE(iter != IDToEntityMap.end(), "Invalid entityId - entity with that ID does not exist!");
			return iter->second->GetComponent<T>();
		}

		/// <summary>Checks whether world has component of given ID.</summary>
		/// <param name="ID">Registered component ID.</param>
		/// <returns>True when world has component of given ID, false otherwise</returns>
		bool HasWorldComponent(size_t ID) const;

		/// <summary>Returns world component of given type.</summary>
		/// <returns>Pointer to world component</returns>
		template<typename T>
		T* GetWorldComponent()
		{
			const auto ctypeID = GetWorldComponentID<T>();
			if(HasWorldComponent(ctypeID))
				return static_cast<T*>(WorldComponents[ctypeID]);
			return nullptr;
		}

		//------------------------------------------------------------------------------
		/// <summary>Returns statically set component type ID.</summary>
		/// <tparam name="T">Type of requested component.</tparam>
		/// <returns>Associated ID.</returns>
		template<typename T> static size_t GetComponentID() noexcept
		{
			return ComponentsIDGroup::GetComponentTypeID<T>();
		}

		//------------------------------------------------------------------------------
		/// <summary>Returns statically set component type ID from 'World' group.</summary>
		/// <tparam name="T">Type of requested component.</tparam>
		/// <returns>Associated ID.</returns>
		template<typename T> static size_t GetWorldComponentID() noexcept
		{
			return WorldComponentsIDGroup::GetComponentTypeID<T>();
		}

		template<typename PrimaryComponent, typename... SecondaryComponents>
		struct IteratorProxy;

		/// <summary>Allows iteration over multiple component types.
		/// Iterator dereferences to a tuple of component pointers.</summary>
		/// <example>To get the component out of the tuple use std::get()
		/// e.g. <code>std::get{YourComponentType*}(components)</code>
		/// If you have a C++17-compliant compiler, you can use structured bindings
		/// e.g. <code>for(auto [a, b] : world->IterateComponents{ComponentA, ComponentB}())</code></example>
		/// <param name="PrimaryComponent">At least one component type must be specified</param>
		/// <param name="SecondaryComponents">Additional component types (warning: returned pointers might be null!)</param>
		/// <returns>A proxy object that can be used in a range-for loop.</returns>
		/// <see cref="World.ComponentIterator"/>
		template<typename PrimaryComponent, typename... SecondaryComponents>
		IteratorProxy<PrimaryComponent, SecondaryComponents...> IterateComponents()
		{
			return {this};
		}

		/// Component iterator.
		template<typename PrimaryComponent, typename... SecondaryComponents>
		class ComponentIterator : public BaseObject<>,
		                          public std::iterator<std::bidirectional_iterator_tag, std::tuple<typename std::add_pointer<PrimaryComponent>::type, typename std::add_pointer<SecondaryComponents>::type...>>
		{
			public:
			bool operator==(const ComponentIterator& rhs) const { return primary_iter == rhs.primary_iter; }
			bool operator!=(const ComponentIterator& rhs) const { return !(*this == rhs); }

			std::tuple<typename std::add_pointer<PrimaryComponent>::type, typename std::add_pointer<SecondaryComponents>::type...> operator*() const
			{
				PrimaryComponent* primary = &*primary_iter;
				return std::make_tuple(primary, primary->template GetSibling<SecondaryComponents>()...);
			}
			std::tuple<typename std::add_pointer<PrimaryComponent>::type, typename std::add_pointer<SecondaryComponents>::type...> operator->() const
			{
				return **this;
			}

			ComponentIterator& operator++() { ++primary_iter; return *this; }
			ComponentIterator operator++(int) { ComponentIterator ret(primary_iter); ++primary_iter; return ret; }
			ComponentIterator& operator--() { --primary_iter; return *this; }
			ComponentIterator operator--(int) { ComponentIterator ret(primary_iter); --primary_iter; return ret; }

			private:
			explicit ComponentIterator(typename IterablePoolAllocator<PrimaryComponent>::Iterator parent) : primary_iter(parent) {}
			friend struct IteratorProxy<PrimaryComponent, SecondaryComponents...>;

			typename IterablePoolAllocator<PrimaryComponent>::Iterator primary_iter;
		};

		/// Iterator proxy
		template<typename PrimaryComponent, typename... SecondaryComponents>
		struct IteratorProxy : BaseObject<>
		{
			IteratorProxy(World* w) : W(w) {}
			World::ComponentIterator<PrimaryComponent, SecondaryComponents...> Begin()
			{
				return ComponentIterator<PrimaryComponent, SecondaryComponents...>(W->GetComponentAllocator<PrimaryComponent>()->Begin());
			}
			World::ComponentIterator<PrimaryComponent, SecondaryComponents...> End()
			{
				return ComponentIterator<PrimaryComponent, SecondaryComponents...>(W->GetComponentAllocator<PrimaryComponent>()->End());
			}
			auto begin() { return Begin(); }
			auto end() { return End(); }
			World* const W;
		};

	private:
		friend class SpawnEntityDeferredTask;
		friend class DestroyEntityDeferredTask;
		template<typename T,typename... Args> friend class AddComponentDeferredTask;
		template<typename T> friend class RemoveComponentDeferredTask;

		friend UniqueID DeferredTaskSystem::SpawnEntityImmediate(World*);
		friend void DeferredTaskSystem::DestroyEntityImmediate(World* w, const UniqueID& entityId);
		template<typename T, typename ...Args> friend T* DeferredTaskSystem::AddComponentImmediate(World* w, const UniqueID & entityId, Args && ...args);
		template<typename T, typename ...Args> friend T* DeferredTaskSystem::AddWorldComponentImmediate(World* w, Args && ...args);
		template<typename T> friend void DeferredTaskSystem::RemoveWorldComponentImmediate(World* w);

		//------------------------------------------------------------------------------
		UniqueID SpawnEntity();

		//------------------------------------------------------------------------------
		void DestroyEntity(const UniqueID& entityId);

		//------------------------------------------------------------------------------
		template<typename T, typename... Args>
		void AddComponent(const UniqueID& entityId, Args&&... args)
		{
			const auto ctypeID = GetComponentID<T>();
			T* ptr = GetComponentAllocator<T>()->Alloc();
			::new(ptr) T(std::forward<Args>(args)...);
			Entity* ent = IDToEntityMap[entityId];
			HEAVY_ASSERTE(ent, "Invalid entity ID");
			HEAVY_ASSERTE(!ent->HasComponent(ctypeID), "Failed at AddComponent() - a component of a given UniqueID already exists!");
			ent->ComponentPosessionFlags.set(ctypeID, true);
			ent->Components[ctypeID] = ptr;
			ptr->Owner = ent;
			HEAVY_ASSERTE(ent->HasComponent(ctypeID), "Failed at AddComponent() - the component was not added!");
		}

		//------------------------------------------------------------------------------
		template<typename T>
		void RemoveComponent(const UniqueID& entityId)
		{
			const auto ctypeID = GetComponentID<T>();
			Entity* ent = IDToEntityMap[entityId];
			HEAVY_ASSERTE(ent, "Invalid entity ID");
			HEAVY_ASSERTE(ent->HasComponent(ctypeID), "Failed at RemoveComponent() - a component of a given UniqueID does not exist!");
			ent->ComponentPosessionFlags.set(ctypeID, false);
			T* component = static_cast<T*>(ent->Components[ctypeID]);
			ent->Components[ctypeID] = nullptr;
			component->~T();
			GetComponentAllocator<T>()->Free(component);
			HEAVY_ASSERTE(!ent->HasComponent(ctypeID), "Failed at AddComponent() - the component was not removed!");
		}

		//------------------------------------------------------------------------------
		template<typename T>
		IterablePoolAllocator<T>* GetComponentAllocator()
		{
			const auto ctypeID = GetComponentID<T>();
			HEAVY_ASSERTE(ctypeID < MAX_COMPONENTS_COUNT, "Invalid component ID");
			if (ComponentAllocators[ctypeID] == nullptr)
				ComponentAllocators[ctypeID] = new IterablePoolAllocator<T>(MAX_ENTITY_COUNT);
			return static_cast<IterablePoolAllocator<T>*>(ComponentAllocators[ctypeID]);
		}

		//------------------------------------------------------------------------------
		template<typename T, typename... Args>
		void AddWorldComponent(Args&&... args)
		{
			const auto ctypeID = GetWorldComponentID<T>();
			HEAVY_ASSERTE(!HasWorldComponent(ctypeID), "Failed at AddWorldComponent() - a world component of a given type already exists!");
			WorldComponents[ctypeID] = new T(std::forward<Args>(args)...);
		}

		//------------------------------------------------------------------------------
		template<typename T>
		void RemoveWorldComponent()
		{
			const auto ctypeID = GetComponentID<T>();
			HEAVY_ASSERTE(HasWorldComponent(ctypeID), "Failed at RemoveWorldComponent() - a component of a given type does not exist!");
			T* component = reinterpret_cast<T*>(WorldComponents[ctypeID]);
			WorldComponents[ctypeID] = nullptr;
			component->~T();
		}

		std::unordered_map<UniqueID, Entity*> IDToEntityMap;

		void RemoveComponentById(Entity* ent, size_t id);

		// Allocators
		PoolAllocator<Entity> EntitiesAllocator;
		IterablePoolAllocatorBase* ComponentAllocators[MAX_COMPONENTS_COUNT];

		ComponentBase* WorldComponents[MAX_COMPONENTS_COUNT];
	};

	//defined here due to circular inclusion problem; FIXME: circular inclusion
	template<typename T>
	T* Entity::GetComponent()
	{
		const auto ctypeID = World::GetComponentID<T>();
		if (HasComponent(ctypeID))
			return static_cast<T*>(Components[ctypeID]);
		else
			return nullptr;
	}

} //namespace Poly
