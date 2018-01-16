#pragma once

#include <Defines.hpp>
#include <String.hpp>
#include "OrderedMap.hpp"
#include <map>
#include "GLUtils.hpp"
#include "GLShaderProgram.hpp"

namespace Poly
{
	class World;
	class CameraComponent;
	class RenderingTargetBase;
	class GLTextureDeviceProxy;
	class AARect;
	class TextureResource;
	struct ScreenSize;
	enum class eInternalTextureUsageType;

	enum class ePassType
	{
		BY_MATERIAL,
		GLOBAL,
		_COUNT
	};

	//------------------------------------------------------------------------------
	class RenderingPassBase : public BaseObject<>
	{
		struct IOBind
		{
			RenderingTargetBase* Target = nullptr;
			String IOName;
		};

	public:
		RenderingPassBase(const String& vertex, const String& fragment);
		RenderingPassBase(const String& vertex, const String& geometry, const String& fragment);

		virtual ~RenderingPassBase();

		void Run(World* world, const CameraComponent* camera, const AARect& rect, ePassType passType = ePassType::BY_MATERIAL);
		void Finalize();

		void BindOutput(const String& outputName, RenderingTargetBase* target);
		void BindInput(const String& inputName, RenderingTargetBase* target);

		void DebugDraw();

		void ClearFBO(GLenum flags = GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	
	protected:

		GLuint FallbackWhiteTexture;

		virtual void OnRun(World* world, const CameraComponent* camera, const AARect& rect, ePassType passType) = 0;

		RenderingTargetBase* GetInputTarget(const String& name);
		RenderingTargetBase* GetOutputTarget(const String& name);

		const OrderedMap<String, RenderingTargetBase*>& GetInputs() const { return Inputs; }
		const OrderedMap<String, RenderingTargetBase*>& GetOutputs() const { return Outputs; }
		GLShaderProgram& GetProgram() { return Program; }

	private:
		OrderedMap<String, RenderingTargetBase*> Inputs;
		OrderedMap<String, RenderingTargetBase*> Outputs;

		GLShaderProgram Program;
		GLuint FBO = 0;

		void CreateDummyTexture();
	};

	//------------------------------------------------------------------------------
	enum class eRenderingTargetType
	{
		TEXTURE_2D,
		TEXTURE_2D_INPUT,
		DEPTH,
		_COUNT
	};

	//------------------------------------------------------------------------------
	class RenderingTargetBase : public BaseObject<>
	{

	public:
		virtual eRenderingTargetType GetType() const = 0;

		virtual void Resize(const ScreenSize& /*size*/) {}
	};

	//------------------------------------------------------------------------------
	class Texture2DRenderingTarget : public RenderingTargetBase
	{
	public:
		Texture2DRenderingTarget(GLuint format);
		Texture2DRenderingTarget(GLuint format, eInternalTextureUsageType internalUsage);

		eRenderingTargetType GetType() const override { return eRenderingTargetType::TEXTURE_2D; }
		void Resize(const ScreenSize& /*size*/) override;

		GLuint GetTextureID();
	private:
		//GLuint Format;
		eInternalTextureUsageType InternalUsage = eInternalTextureUsageType(0);
		std::unique_ptr<GLTextureDeviceProxy> Texture;
	};

	//------------------------------------------------------------------------------
	class DepthRenderingTarget : public Texture2DRenderingTarget
	{
	public:
		DepthRenderingTarget();

		eRenderingTargetType GetType() const override { return eRenderingTargetType::DEPTH; }
	};

	class Texture2DInputTarget : public RenderingTargetBase
	{
	public:
		Texture2DInputTarget(const String& path);
		~Texture2DInputTarget();

		GLuint GetTextureID() const;
		eRenderingTargetType GetType() const override { return eRenderingTargetType::TEXTURE_2D_INPUT; }
	private:
		TextureResource* Texture;
	};
}
