#include "Renderer.h"
#include "Walnut/Random.h"
#include <execution>

namespace Utils {
	static uint32_t ConvertToRGBA(const glm::vec4& color)
	{
		uint8_t r = (color.r * 255.0f);
		uint8_t g = (color.g * 255.0f);
		uint8_t b = (color.b * 255.0f);
		uint8_t a = (color.a * 255.0f);

		uint32_t result = (a << 24) | (b << 16) | (g << 8) | r;
		return result;
	}
}

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

	delete[] m_AccumulationData;
	m_AccumulationData = new glm::vec4[width * height];

	m_ImageHorizontalIterator.resize(width);
	m_ImageVerticalIterator.resize(height);

	for (uint32_t i = 0; i < width; i++)
		m_ImageHorizontalIterator[i] = i;
	for (uint32_t i = 0; i < height; i++)
		m_ImageVerticalIterator[i] = i;
}

void Renderer::Render(const Scene& scene, const Camera& camera)
{
	m_ActiveScene = &scene;
	m_ActiveCamera = &camera;
	
	//clear buffer if we are at the start of the frameindex
	if (m_FrameIndex == 1)
		memset(m_AccumulationData, 0, m_FinalImage->GetWidth() * m_FinalImage->GetHeight() * sizeof(glm::vec4));


#define MT 1
#if MT
	//multithread setup
	std::for_each(std::execution::par, m_ImageVerticalIterator.begin(), m_ImageVerticalIterator.end(), [this](uint32_t y)
		{
			std::for_each(std::execution::par, m_ImageHorizontalIterator.begin(), m_ImageHorizontalIterator.end(), [this, y](uint32_t x)
				{
					glm::vec4 color = PerPixel(x, y);
					m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

					glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
					accumulatedColor /= (float)m_FrameIndex;

					accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
					m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
				});
		});
#else

	for (uint32_t y = 0; y < m_FinalImage->GetHeight(); y++)
	{
		for (uint32_t x = 0; x < m_FinalImage->GetWidth(); x++)
		{
			glm::vec4 color = PerPixel(x, y);
			m_AccumulationData[x + y * m_FinalImage->GetWidth()] += color;

			glm::vec4 accumulatedColor = m_AccumulationData[x + y * m_FinalImage->GetWidth()];
			accumulatedColor /= (float)m_FrameIndex;

			accumulatedColor = glm::clamp(accumulatedColor, glm::vec4(0.0f), glm::vec4(1.0f));
			m_ImageData[x + y * m_FinalImage->GetWidth()] = Utils::ConvertToRGBA(accumulatedColor);
		}
	}
#endif

	m_FinalImage->SetData(m_ImageData);

	if (m_Settings.Accumulate)
		m_FrameIndex++;
	else
		m_FrameIndex = 1;
}

glm::vec4 Renderer::PerPixel(uint32_t x, uint32_t y) 
{
	Ray ray;
	ray.Origin = m_ActiveCamera->GetPosition();
	ray.Direction = m_ActiveCamera->GetRayDirections()[x + y * m_FinalImage->GetWidth()];
	
	glm::vec3 color(0.0f);

	float multiplier = 1.0f;

	int bounces = 5;
	for (int i = 0; i < bounces; i++) 
	{
		Renderer::HitPayload payload = TraceRay(ray);

		//if ray misses, return the background color
		if (payload.HitDistance < 0.0f) 
		{
			glm::vec3 skyColor = glm::vec3(0.6f, 0.7f, 0.9f);
			color += skyColor * multiplier;
			break;

		}

		glm::vec3 lightDir = glm::normalize(glm::vec3(-1, -1, -1));


		float lightIntensity = glm::max(glm::dot(payload.WorldNormal, -lightDir), 0.0f); // same as cosine of the angle between them between -1 to 1 (light source intensity)

		const Sphere& sphere = m_ActiveScene->Spheres[payload.ObjectIndex];
		const Material& material = m_ActiveScene->Materials[sphere.MaterialIndex];

		glm::vec3 sphereColor = material.Albedo;
		sphereColor *= lightIntensity;
		color += sphereColor * multiplier;

		multiplier *= 0.5;

		ray.Origin = payload.WorldPosition + payload.WorldNormal * 0.0001f;

		//ray reflection = world norm + material rougness * random vec3 offset (RESULT IS VERY NOISY)
		ray.Direction = glm::reflect(ray.Direction, 
			payload.WorldNormal + material.Roughness * Walnut::Random::Vec3(-0.5f, 0.5f));
	}
	return glm::vec4(color, 1.0f);

	//alpha stays 1, set green channel in 8 bit offset, set red channel at end (ABGR)
	//return 0xff000000 | (g << 8) | r;

}

Renderer::HitPayload Renderer::TraceRay(const Ray& ray)
{
	// t is distance along the ray
	// (bx^2 + by^2)t^2 + (2(axbx + ayby))t + (ax^2 + ay^2 - r^2) = 0
	// a = ray origin
	// b = ray direction
	// r = radius of sphere
	// t = hit distance

	//quadratic coefficients

	int closestSphere = -1;
	float hitDistance = FLT_MAX;

	for (size_t i = 0 ; i < m_ActiveScene->Spheres.size() ; i++) 
	{

		const Sphere& sphere = m_ActiveScene->Spheres[i];
		glm::vec3 origin = ray.Origin - sphere.Position;

		//dot product of rayDirection with itself
		float a = glm::dot(ray.Direction, ray.Direction);
		float b = 2.0f * glm::dot(origin, ray.Direction);
		float c = glm::dot(origin, origin) - sphere.Radius * sphere.Radius;

		//quadratic formula discriminant (valid solution means hit occured)
		// b^2 - 4ac
		// >0 = 2 solutions
		// =0 = 1 solution
		// <0 = 0 solutions

		float discriminant = b * b - 4.0f * a * c;
		if (discriminant < 0.0f)
			continue;

		// (-b +- sqrt(discriminant)) / 2a hit calc check if closest sphere
		// float t0 = (-b + glm::sqrt(discriminant)) / (2.0f * a);
		
		float closestT = (-b - glm::sqrt(discriminant)) / (2.0f * a);
		if (closestT > 0.0f && closestT < hitDistance) {
			hitDistance = closestT;
			closestSphere = (int)i;
		}
		

	}

	if (closestSphere < 0)
		return Miss(ray);


	return ClosestHit(ray, hitDistance, closestSphere);

	
}

Renderer::HitPayload Renderer::ClosestHit(const Ray& ray, float hitDistance, int objectIndex) 
{
	
	Renderer::HitPayload payload;
	payload.HitDistance = hitDistance;
	payload.ObjectIndex = objectIndex;

	const Sphere& closestSphere = m_ActiveScene->Spheres[objectIndex];

	glm::vec3 origin = ray.Origin - closestSphere.Position;
	//determine both hit points, entry and exit
	payload.WorldPosition = origin + ray.Direction * hitDistance;
	payload.WorldNormal = glm::normalize(payload.WorldPosition);

	//reset world pos to sphere offset
	payload.WorldPosition += closestSphere.Position;

	return payload;
}

Renderer::HitPayload Renderer::Miss(const Ray& ray)
{
	Renderer::HitPayload payload;
	payload.HitDistance = -1;
	return payload;
}
