#include "testLayer.h"
#include "rayTracingLayer.h"

using namespace MyCore;

class Sandbox : public Application
{
public:
	Sandbox()
	{
		PushLayer(mkU<rayTracingLayer>("rayTracingLayer"));
	}

	~Sandbox()
	{

	}
};

uPtr<Application> MyCore::CreateApplication()
{
	return mkU<Sandbox>();
}