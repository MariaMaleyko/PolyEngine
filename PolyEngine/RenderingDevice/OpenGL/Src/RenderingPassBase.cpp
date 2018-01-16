#include "RenderingPassBase.hpp"

#include <ResourceManager.hpp>
#include <TextureResource.hpp>

#include "GLTextureDeviceProxy.hpp"
#include "GLRenderingDevice.hpp"


using namespace Poly;

//------------------------------------------------------------------------------
void RenderingPassBase::BindOutput(const String& outputName, RenderingTargetBase* target)
{
	if (target)
	{
		ASSERTE(!Outputs.Get(outputName), "There is a target already bound!");
		Outputs.Get(outputName) = target;
	}
	else
	{
		ASSERTE(Outputs.Get(outputName), "There is no target bound!");
		Outputs.Remove(outputName);
	}
}

//------------------------------------------------------------------------------
void RenderingPassBase::BindInput(const String& inputName, RenderingTargetBase* target)
{
	if (target)
	{
		ASSERTE(!Inputs.Get(inputName) , "There is a target already bound!");
		Inputs.Get(inputName) = target;
	}
	else
	{
		ASSERTE(Inputs.Get(inputName) , "There is no target bound!");
		Inputs.Remove(inputName);
	}
}

void Poly::RenderingPassBase::DebugDraw()
{
	if (FBO > 0)
	{
		uint32_t attachmentsCount = 0;
		for (auto& kv : GetOutputs())
		{
			RenderingTargetBase* target = kv->value;                  //
			if (target->GetType() == eRenderingTargetType::TEXTURE_2D)
					++attachmentsCount;
		}

		if (attachmentsCount == 0)
			return;

		uint32_t drawDivisor = std::max(3u, attachmentsCount);
		ScreenSize screenSize = gRenderingDevice->GetScreenSize();
		uint32_t divH = screenSize.Height / drawDivisor;

		glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
		glBindFramebuffer(GL_READ_FRAMEBUFFER, FBO);

		uint32_t count = 0;
		for (auto& kv : GetOutputs())
		{
			RenderingTargetBase* target = kv->value;

			if (target->GetType() == eRenderingTargetType::TEXTURE_2D)
			{
				glReadBuffer(GL_COLOR_ATTACHMENT0 + count);
				glBlitFramebuffer(0, 0, screenSize.Width, screenSize.Height,
					0, count * divH, screenSize.Width / drawDivisor, (count + 1) * divH,
					GL_COLOR_BUFFER_BIT, GL_LINEAR);
				++count;
			}
		}
		glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
		CHECK_GL_ERR();
	}
}

//------------------------------------------------------------------------------
void Poly::RenderingPassBase::ClearFBO(GLenum flags)
{
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);
	glClear(flags);
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
}

//------------------------------------------------------------------------------
Poly::RenderingPassBase::RenderingPassBase(const String& vertex, const String& fragment)
	: Program(vertex, fragment)
{
	CreateDummyTexture();
}

//------------------------------------------------------------------------------
Poly::RenderingPassBase::RenderingPassBase(const String& vertex, const String& geometry, const String& fragment)
	: Program(vertex, geometry, fragment)
{
	CreateDummyTexture();
}

//------------------------------------------------------------------------------
Poly::RenderingPassBase::~RenderingPassBase()
{
	if(FBO > 0)
		glDeleteFramebuffers(1, &FBO);
}

//------------------------------------------------------------------------------
void RenderingPassBase::Run(World* world, const CameraComponent* camera, const AARect& rect, ePassType passType)
{
	// Bind inputs
	Program.BindProgram();
	uint32_t samplerCount = 0;
	for (auto& kv : GetInputs())
	{
		const String& name = kv.key;
		RenderingTargetBase* target = kv->value;

		switch (target->GetType())
		{
		case eRenderingTargetType::DEPTH:
		case eRenderingTargetType::TEXTURE_2D:
		{
			GLuint textureID = static_cast<Texture2DRenderingTarget*>(target)->GetTextureID();
			uint32_t idx = samplerCount++;
			glActiveTexture(GL_TEXTURE0 + idx);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
			glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
			glBindTexture(GL_TEXTURE_2D, textureID);
			Program.SetUniform(name, (int)idx);
			break;
		}
		case eRenderingTargetType::TEXTURE_2D_INPUT:
		{
			GLuint textureID = static_cast<Texture2DInputTarget*>(target)->GetTextureID();
			uint32_t idx = samplerCount++;
			glActiveTexture(GL_TEXTURE0 + idx);
			glBindTexture(GL_TEXTURE_2D, textureID);
			Program.SetUniform(name, (int)idx);
			break;
		}
		default:
			ASSERTE(false, "Unhandled target type!");
		}
	}
	glActiveTexture(GL_TEXTURE0); // reset texture binding

	// bind outputs (by binding fbo)
	glBindFramebuffer(GL_DRAW_FRAMEBUFFER, FBO);

	// call run implementation
	OnRun(world, camera, rect, passType);
}

//------------------------------------------------------------------------------
void RenderingPassBase::Finalize()
{
	if (GetOutputs().IsEmpty())
		return; // we want the default FBO == 0, which is the screen buffer

	ASSERTE(FBO == 0, "Calling finalize twice!");
	glGenFramebuffers(1, &FBO);
	ASSERTE(FBO > 0, "Failed to create FBO!");
	glBindFramebuffer(GL_FRAMEBUFFER, FBO);

	bool foundDepth = false;

	Dynarray<GLenum> colorAttachements;
	for (auto& kv : GetOutputs())
	{
		const String& name = kv.key;
		RenderingTargetBase* target = kv->value;

		switch (target->GetType())
		{
		case eRenderingTargetType::TEXTURE_2D:
		{
			GLuint textureID = static_cast<Texture2DRenderingTarget*>(target)->GetTextureID();
			size_t idx = Program.GetOutputsInfo().Get(name).Value().Index;
			GLenum attachementIdx = GL_COLOR_ATTACHMENT0 + (uint32_t)idx;
			glBindTexture(GL_TEXTURE_2D, textureID);
			glFramebufferTexture2D(GL_FRAMEBUFFER, attachementIdx, GL_TEXTURE_2D, textureID, 0);
			colorAttachements.PushBack(attachementIdx);
			CHECK_FBO_STATUS();
			break;
		}
		case eRenderingTargetType::DEPTH:
		{
			GLuint textureID = static_cast<DepthRenderingTarget*>(target)->GetTextureID();
			glBindTexture(GL_TEXTURE_2D, textureID);
			glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, textureID, 0);
			foundDepth = true;
			CHECK_FBO_STATUS();
			break;
		}
		default:
			ASSERTE(false, "Unhandled target type!");
		}
	}
	ASSERTE(foundDepth, "Depth buffer not present when constructing FBO!");
	CHECK_GL_ERR();
	glDrawBuffers((GLsizei)colorAttachements.GetSize(), colorAttachements.GetData());
	CHECK_GL_ERR();
	CHECK_FBO_STATUS();

	// restore default FBO
	glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

//------------------------------------------------------------------------------
RenderingTargetBase* RenderingPassBase::GetInputTarget(const String& name)
{
	auto it = Inputs.Entry(name);
	if (!it.IsVacant())
		return it.OccupiedGet();
	return nullptr;
}

//------------------------------------------------------------------------------
RenderingTargetBase* RenderingPassBase::GetOutputTarget(const String& name)
{
	auto it = Outputs.Entry(name);
	if (!it.IsVacant())
		return it.OccupiedGet();
	return nullptr;
}

Poly::Texture2DRenderingTarget::Texture2DRenderingTarget(GLuint format)
	: Texture2DRenderingTarget(format, eInternalTextureUsageType::COLOR_ATTACHEMENT)
{
}

Poly::Texture2DRenderingTarget::Texture2DRenderingTarget(GLuint format, eInternalTextureUsageType internalUsage)
	: /*Format(format),*/ InternalUsage(internalUsage)
{
	ScreenSize size = gRenderingDevice->GetScreenSize();
	Texture = std::make_unique<GLTextureDeviceProxy>(size.Width, size.Height, InternalUsage, format);
}

void Poly::Texture2DRenderingTarget::Resize(const ScreenSize& size)
{
	Texture->Resize(size);
}

GLuint Poly::Texture2DRenderingTarget::GetTextureID()
{
	return Texture->GetTextureID();
}

Poly::DepthRenderingTarget::DepthRenderingTarget()
	: Texture2DRenderingTarget(GL_DEPTH_COMPONENT16, eInternalTextureUsageType::DEPTH_ATTACHEMENT)
{
}

Poly::Texture2DInputTarget::Texture2DInputTarget(const String & path)
{
	Texture = ResourceManager<TextureResource>::Load(path, eResourceSource::ENGINE);
}

Poly::Texture2DInputTarget::~Texture2DInputTarget()
{
	ResourceManager<TextureResource>::Release(Texture);
}

GLuint Poly::Texture2DInputTarget::GetTextureID() const
{
	return static_cast<const GLTextureDeviceProxy*>(Texture->GetTextureProxy())->GetTextureID();
}

void Poly::RenderingPassBase::CreateDummyTexture()
{

	glGenTextures(1, &FallbackWhiteTexture);

	GLubyte data[] = { 255, 255, 255, 255 };

	glBindTexture(GL_TEXTURE_2D, FallbackWhiteTexture);

	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
	glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

	glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
}
