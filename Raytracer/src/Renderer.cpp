#include "Renderer.h"
#include "Walnut/Random.h"

void Renderer::OnResize(uint32_t width, uint32_t height)
{
	if (m_FinalImage)
	{
		//no resize, exit
		if (m_FinalImage->GetWidth() == width && m_FinalImage->GetHeight() == height)
			return;
		m_FinalImage->Resize(width, height);
	}
	else
	{
		m_FinalImage = std::make_shared<Walnut::Image>(width, height, Walnut::ImageFormat::RGBA);

	}
	
	delete[] m_ImageData;
	m_ImageData = new uint32_t[width * height];

}

void Renderer::Render()
{
	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++) 
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			//coord is a 0-1 float based on resolution and image dimensions
			glm::vec2 coord = { (float)x / (float)m_FinalImage->GetWidth(), (float)y / (float)m_FinalImage->GetHeight() };
			
			//remap 0-1 into -1 - 1
			coord = coord * 2.0f - 1.0f;

			m_ImageData[x + y * m_FinalImage->GetWidth()] = PerPixel(coord);
		}	
	}	
	m_FinalImage->SetData(m_ImageData);
}

uint32_t Renderer::PerPixel(glm::vec2 coord)
{
	glm::vec3 rayOrigin(0.0f, 0.0f, 2.0f);
	glm::vec3 rayDirection(coord.x, coord.y, -1.0f);
	float radius = 0.5;
	// rayDirection = glm::normalize(rayDirection);

	// t is distance along the ray
	// (bx^2 + by^2)t^2 + (2(axbx + ayby))t + (ax^2 + ay^2 - r^2) = 0
	// a = ray origin
	// b = ray direction
	// r = radius of sphere
	// t = hit distance
	
	//quadratic coefficients

	//dot product of rayDirection with itself
	float a = glm::dot(rayDirection, rayDirection);
	float b = 2.0f * glm::dot(rayOrigin, rayDirection);
	float c = glm::dot(rayOrigin, rayOrigin) - radius * radius;

	//quadratic formula discriminant (valid solution means hit occured)
	// b^2 - 4ac
	// >0 = 2 solutions
	// =0 = 1 solution
	// <0 = 0 solutions
	float discriminant = b * b - 4.0f * a * c;

	if (discriminant >= 0.0f)
		return 0xffff00ff;
	return 0xff000000;
	//alpha stays 1, set green channel in 8 bit offset, set red channel at end (ABGR)
	//return 0xff000000 | (g << 8) | r;
}
