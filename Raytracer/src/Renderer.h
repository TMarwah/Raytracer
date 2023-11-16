#pragma once

#include "Walnut/Image.h"
#include "Camera.h"
#include "Ray.h"
#include <memory>
#include <glm/glm.hpp>
#include "Scene.h"

class Renderer
{
public:
	Renderer() = default;

	void OnResize(uint32_t width, uint32_t height);
	void Render(const Scene& scene, const Camera& camera);

	std::shared_ptr<Walnut::Image> GetFinalImage() const { return m_FinalImage; }
private:
	struct HitPayload {
		float HitDistance;
		glm::vec3 WorldPosition;
		glm::vec3 WorldNormal;

		int ObjectIndex;
	};

	glm::vec4 PerPixel(uint32_t x, uint32_t y); //essentially a raygen shader (invoke per pixel being rendered)

	HitPayload TraceRay(const Ray& ray); //shoot ray and return hit payload
	HitPayload ClosestHit(const Ray& ray, float hitDistance, int objectIndex); //determine closest hit, uses hitdistance
	HitPayload Miss(const Ray& ray); //payload for missed ray
private:
	std::shared_ptr<Walnut::Image> m_FinalImage;

	const Scene* m_ActiveScene = nullptr;
	const Camera* m_ActiveCamera = nullptr;

	uint32_t* m_ImageData = nullptr;


};