#include "BloomEffect.h"

void BloomEffect::Init(unsigned width, unsigned height)
{
	int index = int(_buffers.size());
	_buffers.push_back(new Framebuffer());
	_buffers[index]->AddColorTarget(GL_RGBA8);
	_buffers[index]->Init(width, height);
	index++;
	_buffers.push_back(new Framebuffer());
	_buffers[index]->AddColorTarget(GL_RGBA8);
	_buffers[index]->Init(unsigned(width/m_downscale), unsigned(height/m_downscale));
	index++;
	_buffers.push_back(new Framebuffer());
	_buffers[index]->AddColorTarget(GL_RGBA8);
	_buffers[index]->Init(unsigned(width/m_downscale), unsigned(height/m_downscale));
	index++;
	_buffers.push_back(new Framebuffer());
	_buffers[index]->AddColorTarget(GL_RGBA8);
	_buffers[index]->Init(width, height);

	//check if the shader is initialized
	//Load in the shader
	index = int(_shaders.size());
	_shaders.push_back(Shader::Create());
	_shaders[index]->LoadShaderPartFromFile("shader/passthrough_vert.glsl", GL_VERTEX_SHADER);
	_shaders[index]->LoadShaderPartFromFile("shader/passthrough_frag.glsl", GL_FRAGMENT_SHADER);
	_shaders[index]->Link();
	index++;
	_shaders.push_back(Shader::Create());
	_shaders[index]->LoadShaderPartFromFile("shader/passthrough_vert.glsl", GL_VERTEX_SHADER);
	_shaders[index]->LoadShaderPartFromFile("shader/BloomHighPass_frag.glsl", GL_FRAGMENT_SHADER);
	_shaders[index]->Link();
	index++;
	_shaders.push_back(Shader::Create());
	_shaders[index]->LoadShaderPartFromFile("shader/passthrough_vert.glsl", GL_VERTEX_SHADER);
	_shaders[index]->LoadShaderPartFromFile("shader/BlurHorizontal_frag.glsl", GL_FRAGMENT_SHADER);
	_shaders[index]->Link();
	index++;
	_shaders.push_back(Shader::Create());
	_shaders[index]->LoadShaderPartFromFile("shader/passthrough_vert.glsl", GL_VERTEX_SHADER);
	_shaders[index]->LoadShaderPartFromFile("shader/BloomComposite_frag.glsl", GL_FRAGMENT_SHADER);
	_shaders[index]->Link();
	index++;
	_shaders.push_back(Shader::Create());
	_shaders[index]->LoadShaderPartFromFile("shader/passthrough_vert.glsl", GL_VERTEX_SHADER);
	_shaders[index]->LoadShaderPartFromFile("shader/BloomComposite_frag.glsl", GL_FRAGMENT_SHADER);
	_shaders[index]->Link();

	//Pixel size
	m_pixelSize = glm::vec2(1.f / width, 1.f / height);
}

void BloomEffect::ApplyEffect(PostEffect * buffer)
{
	//Draws previous buffer to first render target
	BindShader(0);
	
	buffer->BindColorAsTexture(0, 0, 0);

	_buffers[0]->RenderToFSQ();

	buffer->UnbindTexture(0);

	UnbindShader();


	//Performs high pass on the first render target
	BindShader(1);
	_shaders[1]->SetUniform("uThreshold", m_threshold);

	BindColorAsTexture(0, 0, 0);

	_buffers[1]->RenderToFSQ();

	UnbindTexture(0);

	UnbindShader();


	//Computes blur, vertical and horizontal
	for (unsigned i = 0; i < m_passes; i++)
	{
		//Horizontal pass
		BindShader(0);
		_shaders[0]->SetUniform("uPixelSize", m_pixelSize.x);

		BindColorAsTexture(1, 0, 0);

		_buffers[0]->RenderToFSQ();

		UnbindTexture(0);

		UnbindShader();

		//Vertical pass
		BindShader(1);
		_shaders[1]->SetUniform("uPixelSize", m_pixelSize.y);

		BindColorAsTexture(2, 0, 0);

		_buffers[1]->RenderToFSQ();

		UnbindTexture(0);

		UnbindShader();
	}


	//Composite the scene and the bloom
	BindShader(3);

	buffer->BindColorAsTexture(0, 0, 0);
	BindColorAsTexture(1, 0, 1);

	_buffers[0]->RenderToFSQ();

	UnbindTexture(1);
	UnbindTexture(0);

	UnbindShader();
}

void BloomEffect::Reshape(unsigned width, unsigned height)
{
	_buffers[0]->Reshape(width, height);
	_buffers[1]->Reshape(unsigned(width / m_downscale), unsigned(height / m_downscale));
	_buffers[2]->Reshape(unsigned(width / m_downscale), unsigned(height / m_downscale));
	_buffers[3]->Reshape(width, height);
}

float BloomEffect::GetDownscale() const
{
	return m_downscale;
}

float BloomEffect::GetThreshold() const
{
	return m_threshold;
}

unsigned BloomEffect::GetPasses() const
{
	return m_passes;
}

void BloomEffect::SetDownscale(float downscale)
{
	m_downscale = downscale;
	Reshape(_buffers[0]->_width, _buffers[0]->_height);
}

void BloomEffect::SetThreshold(float threshold)
{
	m_threshold = threshold;
}

void BloomEffect::SetPasses(unsigned passes)
{
	m_passes = passes;
}
